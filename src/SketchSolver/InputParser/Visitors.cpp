#include "Visitors.h"
#include <iostream>

void ParentVisitor::visitArith(arith_node& n) {
    if (n.mother != nullptr) n.mother->accept(visitor);
    if (n.father != nullptr) n.father->accept(visitor);
    for(auto m : n.multi_mother) m->accept(visitor);
}

void ParentVisitor::visitBool(bool_node& n) {
    if (n.mother != nullptr) n.mother->accept(visitor);
    if (n.father != nullptr) n.father->accept(visitor);
}

void NodeTraverser::visitBool(bool_node& n) {
    if (visited.count(&n) == 0) {
        visited.insert(&n);
        pre(n);
        n.accept(ParentVisitor(*this));
        post(n);
    }
}

void NodeReacherBase::post(bool_node &n) {
    n.accept(ParentVisitor(FunVisitor([&](bool_node &m) {
        const auto &reachables_m = reachable.find(&m);
        if (reachables_m != reachable.end()) {
            auto &reachable_n = reachable[&n];
            for (auto reachable_m : reachables_m->second)
                reachable_n.insert(reachable_m);
        }
    })));
}

DeepDelete::~DeepDelete() {
    for (bool_node *bn : visited) {
        delete bn;
    }
}

void DeepDelete::del(bool_node *n) {
    n->accept(DeepDelete{});
}

void DeepClone::post(bool_node &n) {
    if (replacements.count(&n) == 0) {
        bool_node *bn = n.clone();
        bn->id = replacements.size();
        bn->mother = replacements[n.mother];
        bn->father = replacements[n.father];
        arith_node *an = dynamic_cast<arith_node*>(bn);
        for(int i = 0; an && i < an->multi_mother.size(); ++i) {
            an->multi_mother[i] = replacements[an->multi_mother[i]];
        }
        replacements[&n] = bn;
    }
}

bool_node *DeepClone::clone(bool_node *n) {
    return DeepClone{}.clone_node(n);
}

bool_node *DeepClone::clone_node(bool_node *n) {
    if (replacements.count(n) == 0) {
        n->accept(*this);
    }
    return replacements[n];
}

void CloneTraverser::post(bool_node &n) {
    if (replacements.count(&n) == 0) {
        DeepClone::post(n);
        bool_node *bn = replacements[&n];
        replacements[&n] = bd->new_node(bn->mother, bn->father, bn);
    }
}

void CloneTraverser::visit(SRC_node &n) {
    replacements[&n] = bd->get_node(n.lid());
}

void CloneTraverser::visit(CTRL_node &n) {
    CTRL_node *cn = dynamic_cast<CTRL_node*>(bd->create_controls(
        n.getOtype() == OutType::BOOL ? -1 : n.get_nbits(), n.lid(), n.get_toMinimize(), n.spAngelic, n.is_sp_concretize(), n.max, n.isFloat));
    cn->setParents(n.parents);
    replacements[&n] = cn;
}

void CloneTraverser::visit(CONST_node &n) {
    replacements[&n] = n.isFloat() ? bd->create_const(n.getFval()) : bd->create_const(n.getVal());
}

void PrintTree::pre(bool_node &n) {
    std::cout << indent << n.mrprint() << ":\n";
    indent += "  ";
}

void PrintTree::post(bool_node &) {
    indent = indent.substr(2);
}

std::string PrettyDag::pretty(bool_node *n) {
    PrettyDag p;
    n->accept(p);
    return p.show();
}

std::string PrettyDag::pretty(const std::string &name, BooleanDAG &bdag) {
    PrettyDag p;
    std::string def = bdag.isModel ? "mdl_def" : "def";
    p.stmts << def << " " << name << "(";
    std::string sep = "";
    for (auto n : bdag.getNodesByType(bool_node::SRC)) {
        SRC_node *m = dynamic_cast<SRC_node*>(n);
        p.stmts << sep << pretty(m->getOtype(), m->getArrSz()) << " " << m->lid();
        sep = " , ";
    }
    for (auto n : bdag.getNodesByType(bool_node::DST)) {
        DST_node *m = dynamic_cast<DST_node*>(n);
        p.stmts << sep << "! NOREC " << m->lid();
        sep = " , ";
    }
    p.stmts << "){\n";
    p.process(bdag);
    p.stmts << "}\n";
    return p.show();
}

std::string PrettyDag::show() {
    return stmts.str();
}

void PrettyDag::process(BooleanDAG &bdag) {
    for (auto *n : bdag.getNodesByType(bool_node::ASSERT)) {
        n->accept(*this);
    }
    for (auto *n : bdag.getNodesByType(bool_node::DST)) {
        n->accept(*this);
    }
}

std::string PrettyDag::popExp() {
    bool_node *n;
    bool spill;
    std::string v;
    std::tie(n, spill, v) = std::move(exps.top());
    exps.pop();
    auto i = vars.find(n);
    if(i != vars.end()) {
        v = i->second;
    } else if(spill) {
        std::string x = "x" + std::to_string(freshvar++);
        vars[n] = x;
        stmts << x << " = " << v << ";\n";
        v = x;
    }
    return v;
}

std::string PrettyDag::listOfExp(int arity, std::string sep) {
    std::string exp = "";
    std::string glue = "";
    for(int i = 0; i < arity; ++i) {
        exp = popExp() + glue + exp;
        glue = sep;
    }
    return exp;
}

#define recurse(name, arity) \
    do {                                                         \
        bool_node &bn = static_cast<bool_node&>(n);              \
        bn.accept(ParentVisitor(FunVisitor([&](bool_node &m) {   \
            auto i = vars.find(&m);                              \
            if(i != vars.end()) {                                \
                exps.push(std::make_tuple(&m, false, i->second));\
            } else {                                             \
                m.accept(*this);                                 \
            }                                                    \
        })));                                                    \
        int r = (arity);                                         \
        Assert(exps.size() >= r,                                 \
               name" expects " + std::to_string(r) + " arg(s)"); \
    } while(0)

#define PrettyOp(name, beg, sep, end, spill, arity)               \
    void PrettyDag::visit(name##_node &n) {                       \
        int r = (arity);                                          \
        recurse(#name, r);                                        \
        std::string exp = listOfExp(r, (sep));                    \
        exps.push(std::make_tuple(&n, (spill), beg + exp + end)); \
    }

PrettyOp(NOT,        "( ! ", "",  ")", false, 1)
PrettyOp(AND,        "(", " & ",  ")", false, 2)
PrettyOp(OR,         "(", " | ",  ")", false, 2)
PrettyOp(XOR,        "(", " ^ ",  ")", false, 2)
PrettyOp(LT,         "(", " < ",  ")", false, 2)
PrettyOp(EQ,         "(", " == ", ")", false, 2)
PrettyOp(NEG,        "( - ", "",  ")", false, 1)
PrettyOp(PLUS,       "(", " + ",  ")", true,  2)
PrettyOp(TIMES,      "(", " * ",  ")", true,  2)
PrettyOp(DIV,        "(", " / ",  ")", true,  2)
PrettyOp(MOD,        "(", " % ",  ")", true,  2)
PrettyOp(ACTRL,      "$$", " ",  "$$", true,  n.multi_mother.size())
PrettyOp(ARR_CREATE, "{$", " ",  "$}", true,  n.multi_mother.size())

void PrettyDag::visit(SRC_node &n) {
    exps.push(std::make_tuple(&n, false, n.lid()));
}

void PrettyDag::visit(DST_node &n) {
    recurse("DST", 1);
    stmts << n.lid() << " = " << popExp() << ";\n";
}

void PrettyDag::visit(CONST_node &n) {
    exps.push(std::make_tuple(&n, false, n.isFloat() ?
                                            std::to_string(n.getFval()) :
                                            std::to_string(n.getVal())));
}

void PrettyDag::visit(CTRL_node &n) {
    exps.push(std::make_tuple(&n, false, "<" + n.get_name() + " >"));
}

void PrettyDag::visit(ASSERT_node &n) {
    recurse("ASSERT", 1);
    std::string atype = n.isNormal() ? "assert" : n.isAssume() ? "assume" : "hassert";
    stmts << atype << " " << popExp() << " : \"" << n.getMsg() << "\" ;\n";
}

std::string PrettyDag::pretty(OutType *ot, int size) {
    std::string length = "";
    if (ot->isArr) {
        ot = static_cast<Arr*>(ot)->atype;
        if (size < 0) {
            length = "_arr";
        } else {
            length = "[*" + std::to_string(size) + "]";
        }
    }

    std::string ty =
       ot == OutType::BOOL  ? "bit" :
       ot == OutType::INT   ? "int" :
       ot == OutType::FLOAT ? "float" :
        (({ Assert(ot->isTuple, "Expected a tuple") }), ot->str());
    return ty + length;
}

void PrettyDag::visit(UFUN_node &n) {
    std::string ty = pretty(n.getOtype());
    recurse("UFUN", n.multi_mother.size() + 1);
    std::string args = listOfExp(n.multi_mother.size(), " ");
    std::string path = popExp();
    std::string call = n.get_ufname() + "[*" + ty + "](" + args + ")(" + path + ")[" + n.outname + "," + std::to_string(n.fgid) + "]";
    exps.push(std::make_tuple(&n, true, call));
}

void PrettyDag::visit(ARRACC_node &n) {
    recurse("ARRACC", n.multi_mother.size() + 1);
    if(n.multi_mother.size() == 2) {
        std::string tbr = popExp();
        std::string fbr = popExp();
        std::string cnd = popExp();
        exps.push(std::make_tuple(&n, true, cnd + " ? " + tbr + " : " + fbr));
    } else {
        std::string exp = listOfExp(n.multi_mother.size(), " ");
        std::string idx = popExp();
        exps.push(std::make_tuple(&n, true, "$" + exp + "$[" + idx + "]"));
    }
}

void PrettyDag::visit(ARRASS_node &n) {
    recurse("ARRASS", 2);
    std::string x = "x" + std::to_string(freshvar++);
    vars[&n] = x;
    std::string rhs = popExp();
    std::get<1>(exps.top()) = true; //force v to spill
    std::string v = popExp();
    std::string e = popExp();
    stmts << "$  " << x << "$$  " << v << "$[ " << e << "]=" << rhs << ";\n";
    exps.push(std::make_tuple(&n, false, x));
}

void PrettyDag::visit(ARR_R_node &n) {
    recurse("ARR_R", 2);
    std::string beg = "[|", end = "|]";
    if(n.father->type == bool_node::CONST) {
        beg = "["; end = "]";
    }
    std::string arr = popExp();
    std::string idx = popExp();
    exps.push(std::make_tuple(&n, false, arr + beg + idx + end));
}

void PrettyDag::visit(ARR_W_node &n) {
    recurse("ARR_W", 3);
    std::string dst = popExp();
    std::string id = popExp();
    std::string src = popExp();
    exps.push(std::make_tuple(&n, true, id + "[[" + src + "->" + dst + "]]"));
}

void PrettyDag::visit(TUPLE_CREATE_node &n) {
    recurse("TUPLE_CREATE", n.multi_mother.size());
    std::string exp = listOfExp(n.multi_mother.size(), " ");
    exps.push(std::make_tuple(&n, false, "[" + n.name + "]{<" + exp + ">}"));
}

void PrettyDag::visit(TUPLE_R_node &n) {
    recurse("TUPLE_R", 1);
    exps.push(std::make_tuple(&n, true, popExp() + ".[" + std::to_string(n.idx) + "]"));
}
