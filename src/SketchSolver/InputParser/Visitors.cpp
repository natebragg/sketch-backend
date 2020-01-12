#include "Visitors.h"

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
