#ifndef VISITORS_H_
#define VISITORS_H_

#include "NodeVisitor.h"
#include "BooleanDAGCreator.h"
#include "BooleanNodes.h"
#include <functional>
#include <string>
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

class FunTraverser : public NodeTraverser {
    std::function<void(bool_node&)> f;
public:
    enum class Order { Pre, Post };
private:
    Order order;
public:
    FunTraverser(Order o, std::function<void(bool_node&)> g)
        : order(o), f(g) {}
protected:
    void pre  ( bool_node& n) override { if (order == Order::Pre)  f(n); }
    void post ( bool_node& n) override { if (order == Order::Post) f(n); }
};

class DeepDelete : public NodeTraverser {
public:
    ~DeepDelete();
    static void del(bool_node *n);
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

class PrintTree : public NodeTraverser {
public:
    std::string indent = "  ";
protected:
    void pre(bool_node &n) override;
    void post(bool_node &) override;
};

class PrettyDag : public NodeVisitor {
public:
    static std::string pretty(OutType *ot, int size = -1);
    static std::string pretty(bool_node *n);
    static std::string pretty(const std::string &name, BooleanDAG &bdag);

    std::string show();
    void process(BooleanDAG &bdag) override;

protected:
    std::stringstream stmts;
    std::stack<std::tuple<bool_node*, bool, std::string> > exps;
    std::map<bool_node*, std::string> vars;
    int freshvar = 0;

    std::string popExp();
    std::string listOfExp(int arity, std::string sym);

	void visit(AND_node &n) override;
	void visit(OR_node &n) override;
	void visit(XOR_node &n) override;
	void visit(SRC_node &n) override;
	void visit(DST_node &n) override;
	void visit(NOT_node &n) override;
	void visit(CTRL_node &n) override;
	void visit(PLUS_node &n) override;
	void visit(TIMES_node &n) override;
	void visit(UFUN_node &n) override;
	void visit(ARRACC_node &n) override;
	void visit(DIV_node &n) override;
	void visit(MOD_node &n) override;
	void visit(NEG_node &n) override;
	void visit(CONST_node &n) override;
	void visit(LT_node &n) override;
	void visit(EQ_node &n) override;
	void visit(ARRASS_node &n) override;
	void visit(ACTRL_node &n) override;
	void visit(ASSERT_node &n) override;

	void visit(ARR_R_node &n) override;
	void visit(ARR_W_node &n) override;
	void visit(ARR_CREATE_node &n) override;

	void visit(TUPLE_CREATE_node &n) override;
	void visit(TUPLE_R_node &n) override;
};

#endif
