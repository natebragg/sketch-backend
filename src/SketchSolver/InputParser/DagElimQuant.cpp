#include "DagElimQuant.h"
#include <utility>

class Inst : public NodeTraverser {
protected:
    std::set<bool_node*> deathRow;
    const std::string &qName;
public:
    bool_node *psi;
    Inst(const std::string &qName, bool_node *psi)
        : qName(qName), psi(psi) { }
    ~Inst() {
        for (bool_node *n : deathRow)
            delete n;
    }
    void inst(bool_node **n) {
        (*n)->accept(*this);
        if (deathRow.count(*n) != 0)
            *n = psi;
    }
protected:
    void visit(SRC_node &n) {
        if (n.name == qName)
            deathRow.insert(&n);
    }
    void post(bool_node &n) {
        if (deathRow.count(n.mother) != 0)
            n.mother = psi;
        if (deathRow.count(n.father) != 0)
            n.father = psi;
        arith_node *an = dynamic_cast<arith_node*>(&n);
        for(int i = 0; an && i < an->multi_mother.size(); ++i) {
            if (deathRow.count(an->multi_mother[i]) != 0)
                an->multi_mother[i] = psi;
        }
    }
};

void FreeIn::post(bool_node &n) {
    auto &fvs_n = fvs[&n];
    n.accept(ParentVisitor(FunVisitor([&](bool_node &m) {
        auto fvs_m = fvs.find(&m);
        if (fvs_m != fvs.end()) {
            for (const std::string &fv_m : fvs_m->second)
                fvs_n.insert(fv_m);
        }
    })));
}

void FreeIn::visit(SRC_node &n) {
    fvs[&n] = { n.name };
}

void FreeIn::visit(DST_node &n) {
    fvs[&n] = { n.name };
}

bool_node *mkNode(bool_node::Type t, bool_node *m, bool_node *f) {
    bool_node *bn = newNode(t);
    bn->mother = m;
    bn->father = f;
    return bn;
}

static std::pair<bool_node*, bool_node*> theoryXform(
        const std::map<bool_node*, std::set<std::string> > &fvs,
        const std::string &qName,
        bool_node *lhs, bool_node *rhs
    )
{
    struct Xform : public NodeVisitor {
        std::vector<bool_node*> pre;
        const std::map<bool_node*, std::set<std::string> > &fvs;
        const std::string &qName;
        bool_node *lhs;

        Xform( const std::map<bool_node*, std::set<std::string> > &fvs,
               const std::string &qName, bool_node *lhs)
            : fvs(fvs), qName(qName), lhs(lhs) { }

        void clear() {
            DeepDelete::del(lhs);
            lhs = nullptr;
            for (bool_node *n : pre) {
                DeepDelete::del(n);
            }
            pre.clear();
        }

        void visit(EQ_node &n) override {
            bool_node *equiv = nullptr, *rest = nullptr;
            if (fvs.at(n.mother).count(qName) == 0) {
                equiv = n.mother;
                rest = n.father;
            } else if (fvs.at(n.father).count(qName) == 0) {
                equiv = n.father;
                rest = n.mother;
            } else {
                // qName is split
                clear();
                return;
            }
            if (equiv->getOtype() == OutType::BOOL) {
                // iff on bools is associative and commutative
                lhs = mkNode(bool_node::EQ, DeepClone::clone(equiv), lhs);
            } else {
                clear();
                return;
            }
            rest->accept(*this);
        }

        void visit(SRC_node &n) override {
            Assert(n.name == qName, "in theoryXform, expected " + qName + " but found " + n.name);
        }

        void visit(CONST_node&) override {
            Assert(false, "in theoryXform, CONST should be impossible");
        }

        void visit(PLUS_node &n) override {
            if (fvs.at(n.mother).count(qName) == 0) {
                lhs = mkNode(bool_node::PLUS, mkNode(bool_node::NEG, DeepClone::clone(n.mother)), lhs);
                n.father->accept(*this);
            } else if (fvs.at(n.father).count(qName) == 0) {
                lhs = mkNode(bool_node::PLUS, lhs, mkNode(bool_node::NEG, DeepClone::clone(n.father)));
                n.mother->accept(*this);
            } else {
                // qName is split
                clear();
            }
        }

        void visit(TUPLE_R_node&) override {
            Assert(false, "in theoryXform, TUPLE_R should have been rewritten");
        }

        void visit(ARR_R_node&) override {
            Assert(false, "in theoryXform, ARR_R should have been rewritten");
        }

        void visit(ARRACC_node&) override {
            Assert(false, "in theoryXform, ARRACC should have been rewritten");
        }

        void visit(TIMES_node &n) override {
            bool_node *divisor = nullptr, *rest = nullptr;
            if (fvs.at(n.mother).count(qName) == 0) {
                divisor = n.mother;
                rest = n.father;
            } else if (fvs.at(n.father).count(qName) == 0) {
                divisor = n.father;
                rest = n.mother;
            } else {
                // qName is split
                clear();
                return;
            }

            bool_node *mod = DeepClone::clone(lhs);
            divisor = DeepClone::clone(divisor);
            lhs = mkNode(bool_node::DIV, lhs, divisor);
            if (divisor->getOtype() == OutType::INT) {
                bool_node *zero = new CONST_node(0);
                divisor = DeepClone::clone(divisor);
                pre.push_back(mkNode(bool_node::NOT, mkNode(bool_node::EQ, zero, divisor)));
                zero = DeepClone::clone(zero);
                divisor = DeepClone::clone(divisor);
                pre.push_back(mkNode(bool_node::EQ, zero, mkNode(bool_node::MOD, mod, divisor)));
            } else if (divisor->getOtype() == OutType::FLOAT) {
                bool_node *zero = new CONST_node(0.0);
                divisor = DeepClone::clone(divisor);
                pre.push_back(mkNode(bool_node::NOT, mkNode(bool_node::EQ, zero, divisor)));
            } else {
                // This should be impossible, but just in case...
                clear();
                return;
            }
            rest->accept(*this);
        }

        void visit(CTRL_node&) override {
            Assert(false, "in theoryXform, CTRL should be impossible");
        }

        void visit(NEG_node &n) override {
            lhs = mkNode(bool_node::NEG, lhs);
            n.mother->accept(*this);
        }

        void visit(NOT_node &n) override {
            lhs = mkNode(bool_node::NOT, lhs);
            n.mother->accept(*this);
        }

        void visit(DIV_node &n) override {
            if (n.getOtype() != OutType::FLOAT) {
                clear();
                return;
            }

            if (fvs.at(n.mother).count(qName) == 0) {
                bool_node *dividend = DeepClone::clone(n.mother);
                bool_node *divisor = DeepClone::clone(lhs);
                lhs = mkNode(bool_node::DIV, dividend, lhs);
                bool_node *zero = new CONST_node(0.0);
                pre.push_back(mkNode(bool_node::NOT, mkNode(bool_node::EQ, zero, lhs->father)));
                n.father->accept(*this);
            } else if (fvs.at(n.father).count(qName) == 0) {
                bool_node *divisor = DeepClone::clone(n.father);
                lhs = mkNode(bool_node::TIMES, divisor, lhs);
                bool_node *zero = new CONST_node(0.0);
                divisor = DeepClone::clone(divisor);
                pre.push_back(mkNode(bool_node::NOT, mkNode(bool_node::EQ, zero, divisor)));
                n.mother->accept(*this);
            } else {
                // qName is split
                clear();
                return;
            }
        }

        void visit(XOR_node &n) override {
            // P <=> Q xor R   ->   (!P <=> Q) <=> R
            if (fvs.at(n.mother).count(qName) == 0) {
                lhs = mkNode(bool_node::EQ, DeepClone::clone(n.mother), mkNode(bool_node::NOT, lhs));
                n.father->accept(*this);
            } else if (fvs.at(n.father).count(qName) == 0) {
                lhs = mkNode(bool_node::EQ, mkNode(bool_node::NOT, lhs), DeepClone::clone(n.father));
                n.mother->accept(*this);
            } else {
                // qName is split
                clear();
            }
        }

        void visit(DST_node&) override {
            Assert(false, "in theoryXform, DST should be impossible");
        }

        void visitBool(bool_node&) override {
            // In the case of LT, AND, OR, UFUN, and ASSERT:
            //      Nothing reasonable can be transformed.
            // For MOD, ARR_W, ARRASS, ARR_CREATE, TUPLE_CREATE, and ACTRL:
            //      Not sure what to do, but until then it's prudent to bail
            clear();
        }
    } xform(fvs, qName, DeepClone::clone(lhs));
    rhs->accept(xform);
    bool_node *pre = nullptr;
    for (bool_node *n : xform.pre) {
        pre = (pre == nullptr) ? n : mkNode(bool_node::AND, pre, n);
    }

    if (PARAMS->verbosity > 8) {
        std::cout << "xform(" << qName << ", " << lhs->mrprint() << ") => (";
        std::cout << (pre == nullptr ? "T" : pre->mrprint()) << ", ";
        std::cout << (xform.lhs == nullptr ? "F" : xform.lhs->mrprint()) << ")\n";
    }
    return std::make_pair(pre, xform.lhs);
}

static std::pair<bool_node*, bool_node*> witn(
        const std::map<bool_node*, std::set<std::string> > &fvs,
        const std::string &qName,
        const std::map<bool_node*, bool_node*> &witnEqns
    )
{
    for (auto &eqn : witnEqns ) {
        bool_node *lhs, *rhs;
        std::tie(lhs, rhs) = eqn;
        if (fvs.at(rhs).count(qName) != 0) {
            auto xform = theoryXform(fvs, qName, lhs, rhs);
            if (xform.second != nullptr) {
                return xform;
            }
        }
    }
    return std::make_pair(nullptr, nullptr);
}

static std::pair<bool_node*, bool_node*> synth(
        const std::set<std::string> &qNames,
        std::map<bool_node*, bool_node*> witnEqns,
        bool_node *formulaBody
    )
{
    {
        std::map<bool_node*, bool_node*> witnClones;
        for (auto &eqn : witnEqns) {
            bool_node *n  = DeepClone::clone(eqn.first);
            witnClones[n] = DeepClone::clone(eqn.second);
        }
        witnEqns = std::move(witnClones);
    }
    formulaBody = DeepClone::clone(formulaBody);

    FreeIn freein;
    for (auto &eqn : witnEqns) {
        eqn.second->accept(freein);
    }

    bool_node *pre = nullptr;
    for (const std::string &qName : qNames) {
        bool_node *pre_n, *psi_n;
        std::tie(pre_n, psi_n) = witn(freein.fvs, qName, witnEqns);
        if (psi_n == nullptr) {
            for (auto &eqn : witnEqns) {
                DeepDelete::del(eqn.first);
                DeepDelete::del(eqn.second);
            }
            if (pre != nullptr) {
                DeepDelete::del(pre);
            }
            DeepDelete::del(formulaBody);

            return std::make_pair(nullptr, nullptr);
        }
        pre = (pre   == nullptr) ? pre_n :
              (pre_n == nullptr) ? pre :
                mkNode(bool_node::AND, pre, pre_n);

        Inst inst(qName, psi_n);
        for (auto &eqn : witnEqns) {
            bool_node *rhs = eqn.second;
            if (freein.fvs[rhs].count(qName) != 0) {
                rhs->accept(FunTraverser(FunTraverser::Order::Post, [&freein](bool_node &n) {
                    freein.fvs.erase(&n);
                }));
                inst.inst(&eqn.second);
                eqn.second->accept(freein);
            }
        }

        if (pre != nullptr) {
            FreeIn freein;
            pre->accept(freein);
            Inst inst(qName, DeepClone::clone(psi_n));
            if (freein.fvs[pre].count(qName) != 0) {
                inst.inst(&pre);
            } else {
                DeepDelete::del(inst.psi);
            }
        }

        {
            FreeIn freein;
            formulaBody->accept(freein);
            Inst inst(qName, DeepClone::clone(psi_n));
            if (freein.fvs[formulaBody].count(qName) != 0) {
                inst.inst(&formulaBody);
            } else {
                DeepDelete::del(inst.psi);
            }
        }
    }

    for (auto &eqn : witnEqns) {
        DeepDelete::del(eqn.first);
        DeepDelete::del(eqn.second);
    }

    return std::make_pair(pre, formulaBody);
}

bool_node* elimQuant(
        std::set<std::string> qNames,
        std::map<bool_node*, bool_node*> witnEqns,
        bool_node *formulaBody
    )
{
    bool_node *pre, *newFormula;
    std::tie(pre, newFormula) = synth(qNames, std::move(witnEqns), formulaBody);
    if (pre != nullptr) {
        return mkNode(bool_node::OR, mkNode(bool_node::NOT, pre), newFormula);
    } else {
        return newFormula;
    }
}
