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
protected:
    void visitBool ( bool_node& n) override { f(n); }
};

class ParentVisitor : public NodeVisitor {
    NodeVisitor &visitor;
public:
    template<typename Visitor>
    ParentVisitor(Visitor &&v) : visitor(v) {}
protected:
    void visitArith(arith_node& n) override;
    void visitBool(bool_node& n) override;
};

class NodeTraverser : public NodeVisitor {
protected:
    std::set<bool_node*> visited;
    void visitBool(bool_node& n) override;
    virtual void pre(bool_node &) {};
    virtual void post(bool_node &) {};
};

class DeepClone : public NodeTraverser {
protected:
    std::map<bool_node*, bool_node*> replacements;
public:
    DeepClone() {
        replacements[nullptr] = nullptr;
    }
    static bool_node *clone(bool_node*);
    bool_node *clone_node(bool_node*);
protected:
    void post(bool_node &n) override;
};

class CloneTraverser : public DeepClone {
    BooleanDAGCreator *bd;
public:
    CloneTraverser(BooleanDAGCreator *bd) : bd(bd) { }
protected:
    void post(bool_node &n) override;
    void visit(SRC_node &n) override;
    void visit(CTRL_node &n) override;
    void visit(CONST_node &n) override;
};

#endif
