#ifndef VISITORS_H_
#define VISITORS_H_

#include "NodeVisitor.h"
#include "BooleanDAGCreator.h"
#include "BooleanNodes.h"
#include <functional>
#include <map>
#include <set>

class FunVisitor : public NodeVisitor {
    std::function<void(bool_node&)> f;
public:
    FunVisitor(std::function<void(bool_node&)> g) : f(g) {}
    void visitArith(arith_node& n) override { f(n); }
    void visitBool ( bool_node& n) override { f(n); }
};

class ParentVisitor : public NodeVisitor {
    NodeVisitor &visitor;
public:
    template<typename Visitor>
    ParentVisitor(Visitor &&v) : visitor(v) {}
    void visitArith(arith_node& n) override {
        if (n.mother != nullptr) n.mother->accept(visitor);
        if (n.father != nullptr) n.father->accept(visitor);
        for(auto m : n.multi_mother) m->accept(visitor);
    }

    void visitBool(bool_node& n) override {
        if (n.mother != nullptr) n.mother->accept(visitor);
        if (n.father != nullptr) n.father->accept(visitor);
    }
};

class NodeTraverser : public NodeVisitor {
    std::set<bool_node*> visited;

    void go(bool_node& n) {
        if (visited.count(&n) == 0) {
            visited.insert(&n);
            pre(n);
            n.accept(ParentVisitor(*this));
            post(n);
        }
    }
protected:
    void visitArith(arith_node& n) override { go(n); }
    void visitBool(bool_node& n) override { go(n); }
    virtual void pre(bool_node &) {};
    virtual void post(bool_node &) {};
};

struct CloneTraverser : public NodeTraverser {
    std::map<bool_node*, bool_node*> replacements;
    BooleanDAGCreator *bd;
    CloneTraverser(BooleanDAGCreator *bd) : bd(bd) {
        replacements[nullptr] = nullptr;
    }
    void post(bool_node &n) override {
        if (replacements.count(&n) == 0) {
            bool_node *bn = n.clone();
            bool_node *mn = replacements[n.mother];
            bool_node *fn = replacements[n.father];
            arith_node *an = dynamic_cast<arith_node*>(bn);
            for(int i = 0; an && i < an->multi_mother.size(); ++i) {
                an->multi_mother[i] = replacements[an->multi_mother[i]];
            }
            replacements[&n] = bd->new_node(mn, fn, bn);
        }
    }
    void visit(SRC_node &n) override {
        replacements[&n] = bd->get_node(n.lid());
    }
    void visit(CTRL_node &n) override {
        CTRL_node *cn = dynamic_cast<CTRL_node*>(bd->create_controls(
            n.getOtype() == OutType::BOOL ? -1 : n.get_nbits(), n.lid(), n.get_toMinimize(), n.spAngelic, n.is_sp_concretize(), n.max, n.isFloat));
        cn->setParents(n.parents);
        replacements[&n] = cn;
    }
    void visit(CONST_node &n) override {
        replacements[&n] = n.isFloat() ? bd->create_const(n.getFval()) : bd->create_const(n.getVal());
    }
};

#endif
