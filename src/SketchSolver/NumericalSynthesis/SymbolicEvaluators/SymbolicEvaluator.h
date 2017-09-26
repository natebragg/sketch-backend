#pragma once
#include <gsl/gsl_vector.h>
#include <map>
#include <set>

class SymbolicEvaluator {
public:
	virtual void run(const gsl_vector* ctrls_p, const map<int, int>& inputValues_p) = 0;
	virtual bool check(bool_node* n, int expected) = 0;
	virtual double computeError(bool_node* n, int expected, gsl_vector* errorGrad) = 0;
	virtual double computeDist(bool_node* n, gsl_vector* distgrad) = 0;
	virtual bool hasDist(bool_node* n) = 0;
	//virtual set<int> getConflicts(int nid) = 0;
	virtual void print() = 0;
	virtual void printFull() = 0;

};
