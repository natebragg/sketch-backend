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
    void visitBool ( bool_node& n) override { f(n); }
};

class ParentVisitor : public NodeVisitor {
    NodeVisitor &visitor;
public:
    template<typename Visitor>
    ParentVisitor(Visitor &&v) : visitor(v) {}
    void visitArith(arith_node& n) override;
    void visitBool(bool_node& n) override;
};

class NodeTraverser : public NodeVisitor {
    std::set<bool_node*> visited;
protected:
    void visitBool(bool_node& n) override;
    virtual void pre(bool_node &) {};
    virtual void post(bool_node &) {};
};

struct DeepClone : public NodeTraverser {
    std::map<bool_node*, bool_node*> replacements;
    DeepClone() {
        replacements[nullptr] = nullptr;
    }
    void post(bool_node &n) override;
    static bool_node *clone(bool_node*);
};

struct CloneTraverser : public DeepClone {
    BooleanDAGCreator *bd;
    CloneTraverser(BooleanDAGCreator *bd) : bd(bd) { }
    void post(bool_node &n) override;
    void visit(SRC_node &n) override;
    void visit(CTRL_node &n) override;
    void visit(CONST_node &n) override;
};

#endif
