#ifndef DAGELIMQUANT_H_
#define DAGELIMQUANT_H_

#include <map>
#include <set>
#include <string>
#include <utility>
#include "Visitors.h"
#include "BooleanNodes.h"

bool_node *mkNode(bool_node::Type, bool_node*, bool_node* = nullptr);

bool_node* elimQuant(
        const std::set<std::string> &qNames,
        std::map<bool_node*, bool_node*> witnEqns,
        bool_node *formulaBody);

struct FreeIn : public NodeTraverser {
    std::map<bool_node*, std::set<std::string> > fvs;
private:
    void post(bool_node &n) override;
    void visit(SRC_node &n) override;
};

#endif
