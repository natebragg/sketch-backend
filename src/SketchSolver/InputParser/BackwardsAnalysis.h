#pragma once
#include "BooleanDAG.h"
#include "NodeVisitor.h"
#include "NodeStore.h"
#include <algorithm>
#include <iterator>
#include <set>
#include <climits>
#include "DeriveImplications.h"

using namespace std;

#ifdef CONST
#undef CONST
#endif

/*!

The goal of the backwards analysis is to compute, for each node in the dag, a set of conditions which are
guaranteed to be true if the value of the node is to matter. 

Each condition is of the form value(bool_node) == val. i.e. If for node n, there is only one condition of the form
value(p) == 4. It means that the value of node n will only matter if node p evaluates to 4. This means that we can 
simplify n to take advantage of the fact that we can assume p to have value 4.

The conditions are stored in Datums. 

For each node, we have an object of type Info, which contains a set of Datums. 
The Info for an object is the intersection of what is known to be true through each of its children.


*/

class Datum{
public:
	bool_node* node;
	int val;

	Datum(bool_node* p_node):node(p_node->type == bool_node::NOT? p_node->mother: p_node), val(p_node->type == bool_node::NOT? 0 : 1){
		
	}

	Datum(bool_node* p_node, int p_val):node(p_node->type == bool_node::NOT? p_node->mother: p_node),  val(p_node->type == bool_node::NOT? (1-p_val) : p_val){
		
	}
	Datum(const Datum& d): node(d.node), val(d.val){

	}
	Datum():node(NULL), val(-1){
	
	}
	Datum& operator=(const Datum& d){
		node = d.node;
		val = d.val;
		return *this;
	}
	bool hasDerivedDatums() const{
		if(node->type == bool_node::AND){
			return val == 1;
		}
		if(node->type == bool_node::OR){
			return val == 0;
		}
		if(node->type == bool_node::PLUS){
			return node->mother->type == bool_node::CONST || node->father->type == bool_node::CONST;
		}
		return false;
	}

	void addDerivedDatums(set<Datum>& sd) const{
		if(node->type == bool_node::AND && val == 1){
			Datum d1(node->mother, 1);
			Datum d2(node->father, 1);
			sd.insert(d1);
			d1.addDerivedDatums(sd);
			sd.insert(d2);
			d2.addDerivedDatums(sd);
		}
		if(node->type == bool_node::OR && val == 0){
			Datum d1(node->mother, 0);
			Datum d2(node->father, 0);
			if(sd.count(d1)==0){
				sd.insert(d1);
				d1.addDerivedDatums(sd);
			}
			if(sd.count(d2)==0){
				sd.insert(d2);
				d2.addDerivedDatums(sd);
			}
		}
		if(node->type == bool_node::PLUS && (node->mother->type == bool_node::CONST || node->father->type == bool_node::CONST)){			
			if(node->mother->type == bool_node::CONST){
				int C = dynamic_cast<CONST_node*>(node->mother)->getVal();
				Datum d1(node->father, val - C);
				sd.insert(d1);
			}else{
				int C = dynamic_cast<CONST_node*>(node->father)->getVal();
				Datum d1(node->mother, val - C);
				sd.insert(d1);
			}
			return ;
		}
	}

};


inline bool operator==(const Datum& d1, const Datum& d2){
	if(d1.node->id == d2.node->id){
		return d1.val == d2.val;
	}else{
		return false;
	}
}

inline bool operator<(const Datum& d1, const Datum& d2){
	if(d1.node->id == d2.node->id){
		return d1.val < d2.val;
	}else{
		return d1.node->id < d2.node->id;
	}
}

class Info{
	set<Datum> known;
	typedef enum{ BOTTOM, MIDDLE, TOP} State;
	State state;
	int hasD;
	set<Datum> temp;

public:
	Info(){
		state = BOTTOM;
		hasD = false;
	}
	string lprint(){
		stringstream str;
		for(set<Datum>::iterator it = known.begin(); it != known.end(); ++it){
			str<<it->node->lprint()<<"\t== "<<it->val<<endl;
		}
		return str.str();
	}
	bool isBottom(){
		return state == BOTTOM;
	}

	void makeTop(){
		state = TOP;
		known.clear();
	}

	void push(const Datum& d){
		Assert(!hasD, "Only one datum can be pushed at a time");
		temp.insert(d);
		if(d.hasDerivedDatums()){
			d.addDerivedDatums(temp);
		}
		hasD = true;
	}

	void pop(){
		Assert(hasD, "Only one datum can be pushed at a time");
		hasD = false;
		temp.clear();
	}

	void filter(set<Datum>& filter){
		Assert(hasD, "This is to filter temp, so hadD must be true");
		set<Datum>::iterator fit = filter.begin();
		set<Datum>::iterator tit = temp.begin();
		while(tit != temp.end()){
			while(fit != filter.end() &&  fit->node->id < tit->node->id){
				++fit;
			}
			if(fit == filter.end()){ return; }
			while(tit != temp.end() && fit->node->id == tit->node->id){
				temp.erase(tit++);
			}
			while(tit != temp.end() && fit->node->id > tit->node->id){
				++tit;
			}
		}
	}

	void pop(set<Datum>& out){
		Assert(hasD, "Only one datum can be pushed at a time");
		Assert(out.size()==0, "The out should be empty");
		hasD = false;
		swap(out, temp);
	}

	bool seek(bool_node* node, set<Datum>& ds, int& out){
		set<Datum>::iterator iter = ds.lower_bound(Datum(node, INT_MIN));
		if(iter == ds.end()){ return false; }
		if(node->type == bool_node::NOT){
				if(iter->node == node->mother){ 
					out = iter->val; 
					out = 1-out;
					return true; 
				}
		}else{
			if(iter->node == node){
				out = iter->val; 
				return true; 
			}
		}
		return false;
	}
 
	bool getValue(bool_node* node, int& out){
		if(hasD && seek(node, temp, out)){			
			return true;
		}
		return seek(node, known, out);		
	}
	/*
	Info& add(bool_node* node, int val){
		int tmp;
		Assert(!getValue(node, tmp), "Node should not already be here");
		known.insert(Datum(node, val));
	}
	
	Info& add(bool_node* node){
		int tmp;
		Assert(!getValue(node, tmp), "Node should not already be here");
		known.insert(Datum(node));
	}
	*/

	Info& operator+=(const Info& inf){
		Assert(!hasD, "This shouldn't happen");
		if(state==BOTTOM){
			if(inf.state != BOTTOM || inf.hasD){
				Assert( (!inf.hasD) || state==BOTTOM, "This is a bug!!");
				state = MIDDLE;
				known = inf.known;
				if(inf.hasD){
					known.insert(inf.temp.begin(), inf.temp.end());
				}
			}
		}else{
			set<Datum> tmp;
			if(inf.hasD){
				set<Datum> sd = inf.known;
				sd.insert(inf.temp.begin(), inf.temp.end());
				set_intersection(known.begin(), known.end(), sd.begin(), sd.end(), inserter(tmp, tmp.begin()));
				swap(tmp, known);
			}else{				
				set_intersection(known.begin(), known.end(), inf.known.begin(), inf.known.end(), inserter(tmp, tmp.begin()));
				swap(tmp, known);				
			}
			state = MIDDLE;
		}
		return *this;
	}
};











class BackwardsAnalysis :
	public NodeVisitor, public virtual NodeStore
{
private:
	map<bool_node*, Info> info;
	map<int, CONST_node*> cnmap;
	// DeriveImplications dimp;
protected:
	virtual void visitArith(arith_node& node );
	virtual void visitBool(bool_node& node );
	// bool check(Info& t, bool_node* n, int& v);
public:
	BackwardsAnalysis(void);
	virtual ~BackwardsAnalysis(void);
	virtual void visit( AND_node& node );
	virtual void visit( OR_node& node );
	bool_node* localModify(ARRASS_node* node, Info& t);
	bool_node* localModify(ARRACC_node* node, Info& t);
	bool_node* localModify(OR_node* node, Info& t);
	bool_node* localModify(AND_node* node, Info& t);

	bool_node* modifyNode(bool_node* node, Info& t);

	virtual void visit( ARRACC_node& node );
	
	virtual void visit( ARRASS_node& node );
	
	virtual void visit( DST_node& node );

	virtual void visit( ASSERT_node &node);	
	
	virtual void process(BooleanDAG& bdag);
	
	CONST_node* getCnode(int val);
	CONST_node* getCnode(bool c);
};
