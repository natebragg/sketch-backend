#include "BoolAutoDiff.h"
#include <algorithm>
#include <limits>
#include <math.h>


BoolAutoDiff::BoolAutoDiff(BooleanDAG& bdag_p, FloatManager& _floats, const map<string, int>& floatCtrls_p, const map<string, int>& boolCtrls_p): bdag(bdag_p), floats(_floats), floatCtrls(floatCtrls_p), boolCtrls(boolCtrls_p) {
	values.resize(bdag.size(), NULL);
	distances.resize(bdag.size(), NULL);
	nctrls = floatCtrls.size() + boolCtrls.size();
	if (nctrls == 0) nctrls = 1;
	ctrls = gsl_vector_alloc(nctrls);
}

BoolAutoDiff::~BoolAutoDiff(void) {
	delete ctrls;
	for (int i = 0; i < values.size(); i++) {
		if (values[i] != NULL) {
			delete values[i];
		}
		if (distances[i] != NULL) {
			delete distances[i];
		}
	}
}

void BoolAutoDiff::visit( SRC_node& node ) {
	//cout << "Visiting SRC node" << endl;
	Assert(false, "NYI: BoolAutoDiff for src");
}

void BoolAutoDiff::visit( DST_node& node ) {
	//cout << "Visiting DST node" << endl;
	// Ignore
}

void BoolAutoDiff::visit( ASSERT_node& node ) {
    DistanceGrad* dg = d(node);
    GradUtil::default_grad(dg->grad);
	//cout << "Visiting ASSERT node" << endl;
}

void BoolAutoDiff::visit( CTRL_node& node ) {
	//cout << "Visiting CTRL node" << endl;
	string name = node.get_name();
	
	if (isFloat(node)) {
		int idx = -1;
		if (floatCtrls.find(name) != floatCtrls.end()) {
			idx = floatCtrls[name];
		} else {
			Assert(false, "All float holes should be handled by numerical solver");
		}
		ValueGrad* val = v(node);
		val->update(gsl_vector_get(ctrls, idx));
		val->set = true;
		GradUtil::default_grad(val->getGrad());
		gsl_vector_set(val->getGrad(), idx, 1.0);
		
	} else {
		DistanceGrad* dg = d(node);
		int ival = getInputValue(node);
		if (ival != DEFAULT_INP) {
			dg->dist = (ival == 1) ? 1000 : -1000;
			GradUtil::default_grad(dg->grad);
			dg->set = true;
		} else {
			dg->set = false;
		}
	}
}

void BoolAutoDiff::visit( PLUS_node& node ) {
	//cout << "Visiting PLUS node" << endl;
	Assert(isFloat(node), "NYI: plus with ints");
	ValueGrad* val = v(node);
	ValueGrad* mval = v(node.mother);
	ValueGrad* fval = v(node.father);
	ValueGrad::vg_plus(mval, fval, val);
}

void BoolAutoDiff::visit( TIMES_node& node ) {
	//cout << "Visiting TIMES node" << endl;
	Assert(isFloat(node), "NYI: times with ints");
	ValueGrad* val = v(node);
	ValueGrad* mval = v(node.mother);
	ValueGrad* fval = v(node.father);
	ValueGrad::vg_times(mval, fval, val);
}

void BoolAutoDiff::visit(ARRACC_node& node )  {
	Assert(node.multi_mother.size() == 2, "NYI: BoolAutoDiff ARRACC of size > 2");
	//cout << node.lprint() << endl;
	//cout << node.mother->lprint() << endl;
	//cout << node.multi_mother[0]->lprint() << endl;
	//cout << node.multi_mother[1]->lprint() << endl;
	if (node.getOtype() == OutType::BOOL) {
		DistanceGrad* dg = d(node);
        DistanceGrad* mdist = d(node.multi_mother[0]);
        DistanceGrad* fdist = d(node.multi_mother[1]);
        int cval = getInputValue(node.mother);
		if (cval != DEFAULT_INP) {
						if (cval == 1.0) {
				DistanceGrad::dg_copy(fdist, dg);
			} else if (cval == 0.0) {
				DistanceGrad::dg_copy(mdist, dg);
			} else {
				Assert(false, "dafuerq");
			}
		} else {
            if (boolCtrls.size() > 0 && node.mother->type == bool_node::CTRL) {
                string name = node.mother->get_name();
                int idx = -1;
                if (boolCtrls.find(name) != boolCtrls.end()) {
                    idx = boolCtrls[name];
                } else {
                    Assert(false, "qwe8yqwi");
                }
                GradUtil::default_grad(GradUtil::tmp2);
                gsl_vector_set(GradUtil::tmp2, idx, 1.0);
                DistanceGrad::dg_ite(mdist, fdist, gsl_vector_get(ctrls, idx), GradUtil::tmp2, dg);
            } else {
                cout << node.lprint() << endl;
                cout << node.mother->lprint() << endl;
                cout << node.multi_mother[0]->lprint() << endl;
                cout << node.multi_mother[1]->lprint() << endl;
                Assert(false, "eruiqge");
                dg->set = false;
            }
		}
	}
	
	if (node.getOtype() == OutType::FLOAT) {
		ValueGrad* val  = v(node);
        ValueGrad* mval = v(node.multi_mother[0]);
        ValueGrad* fval = v(node.multi_mother[1]);
        int cval = getInputValue(node.mother);
        if (cval != DEFAULT_INP) {
            if (cval == 1.0) {
                ValueGrad::vg_copy(fval, val);
            } else if (cval == 0.0) {
                ValueGrad::vg_copy(mval, val);
            } else {
                Assert(false, "Not possible");
            }
        } else {
            if (boolCtrls.size() > 0 && node.mother->type == bool_node::CTRL) {
                string name = node.mother->get_name();
                int idx = -1;
                if (boolCtrls.find(name) != boolCtrls.end()) {
                    idx = boolCtrls[name];
                } else {
                    Assert(false, "qwe8yqwi");
                }
                GradUtil::default_grad(GradUtil::tmp2);
                gsl_vector_set(GradUtil::tmp2, idx, 1.0);
                ValueGrad::vg_ite(mval, fval, gsl_vector_get(ctrls, idx), GradUtil::tmp2, val);
                
            } else {
                DistanceGrad* cdist = d(node.mother);
                ValueGrad::vg_ite(mval, fval, cdist, val);
            }
        }
    }
}

void BoolAutoDiff::visit( DIV_node& node ) {
	//cout << "Visiting DIV node" << endl;
	Assert(isFloat(node), "NYI: div with ints");
	ValueGrad* val = v(node);
	ValueGrad* mval = v(node.mother);
	ValueGrad* fval = v(node.father);
	ValueGrad::vg_div(mval, fval, val);
}

void BoolAutoDiff::visit( MOD_node& node ) {
	cout << "Visiting MOD node" << endl;
	Assert(false, "NYI: BoolAutoDiff mod");
}

void BoolAutoDiff::visit( NEG_node& node ) {
	//cout << "Visiting NEG node" << endl;
	Assert(isFloat(node), "NYI: neg with ints");
	ValueGrad* val = v(node);
	ValueGrad* mval = v(node.mother);
	ValueGrad::vg_neg(mval, val);
}

void BoolAutoDiff::visit( CONST_node& node ) {
	if (node.getOtype() == OutType::FLOAT) {
		ValueGrad* val = v(node);
		val->update(node.getFval());
		val->set = true;
		GradUtil::default_grad(val->getGrad());
	} else {
		int val = node.getVal();
		ValueGrad* vg = v(node);
		DistanceGrad* dg = d(node);
		if (node.getOtype() == OutType::BOOL) {
			dg->dist = (val == 1) ? 1000 : -1000;
			GradUtil::default_grad(dg->grad);
			dg->set = true;
			//vg->update(val);
			//vg->set = true;
			//GradUtil::default_grad(vg->getGrad());
		} else {
            Assert(false, "Integers are not handled yet");
			dg->set = false;
			vg->update(val);
			vg->set = true;
			GradUtil::default_grad(vg->getGrad());
		}
	}
}

void BoolAutoDiff::visit( LT_node& node ) {
	DistanceGrad* dg = d(node);
	
	// Comput the value from the parents
	Assert(isFloat(node.mother) && isFloat(node.father), "NYI: BoolAutoDiff for lt with integer parents");
	ValueGrad* mval = v(node.mother);
	ValueGrad* fval = v(node.father);
	
	ValueGrad::vg_lt(mval, fval, dg);
}

void BoolAutoDiff::visit( EQ_node& node ) {
	//cout << "Visiting EQ node" << endl;
	Assert(false, "NYI: BoolAutoDiff for eq");
}

void BoolAutoDiff::visit( AND_node& node ) {
	DistanceGrad* dg = d(node);
    int mval = getInputValue(node.mother);
    int fval = getInputValue(node.father);
    
    if (mval == 0 || fval == 0) {
        dg->dist = -1000.0;
        GradUtil::default_grad(dg->grad);
        dg->set = true;
        return;
    }
    // Compute the value from the parents
    DistanceGrad* mdist = d(node.mother);
    DistanceGrad* fdist = d(node.father);
    if (mval == 1) {
        DistanceGrad::dg_copy(fdist, dg);
        return;
    }
    if (fval == 1) {
        DistanceGrad::dg_copy(mdist, dg);
        return;
    }
	if (mdist->set && fdist->set) {
		DistanceGrad::dg_and(mdist, fdist, dg);
	} else {
		dg->set = false;
	}
}

void BoolAutoDiff::visit( OR_node& node ) {
	DistanceGrad* dg = d(node);
    int mval = getInputValue(node.mother);
    int fval = getInputValue(node.father);
    
    if (mval == 1 || fval == 1) {
        dg->dist = 1000.0;
        GradUtil::default_grad(dg->grad);
        dg->set = true;
        return;
    }
	// Compute the value from the parents
	DistanceGrad* mdist = d(node.mother);
	DistanceGrad* fdist = d(node.father);
    if (mval == 0) {
        DistanceGrad::dg_copy(fdist, dg);
        return;
    }
    if (fval == 0) {
        DistanceGrad::dg_copy(mdist, dg);
        return;
    }
	if (mdist->set && fdist->set) {
		DistanceGrad::dg_or(mdist, fdist, dg);
	} else {
		dg->set = false;
	}
}

void BoolAutoDiff::visit( NOT_node& node ) {
	DistanceGrad* dg = d(node);
    int mval = getInputValue(node.mother);
    if (mval == 0) {
        dg->dist = 1000.0;
        GradUtil::default_grad(dg->grad);
        dg->set = true;
        return;
    }
    if (mval == 1) {
        dg->dist = -1000.0;
        GradUtil::default_grad(dg->grad);
        dg->set = true;
        return;
    }
	DistanceGrad* mdist = d(node.mother);

	if (mdist->set) {
		DistanceGrad::dg_not(mdist, dg);
	} else {
		dg->set = false;
	}
}

void BoolAutoDiff::visit( ARRASS_node& node ) {
	cout << "Visiting ARRASS node" << endl;
	Assert(false, "NYI: BoolAutoDiff for arrass");
}

void BoolAutoDiff::visit( UFUN_node& node ) {
    DistanceGrad* dg = d(node);
    GradUtil::default_grad(dg->grad);
	ValueGrad* val = v(node);
	ValueGrad* mval = v(node.multi_mother[0]);
	
	const string& name = node.get_ufname();
	if (name == "_cast_int_float_math") {
		ValueGrad::vg_cast_int_float(mval, val);
	} else if (floats.hasFun(name)) {
		if (name == "arctan_math") {
			ValueGrad::vg_arctan(mval, val);
		} else if (name == "sin_math") {
			ValueGrad::vg_sin(mval, val);
		} else if (name == "cos_math") {
			ValueGrad::vg_cos(mval, val);
		} else if (name == "tan_math") {
			ValueGrad::vg_tan(mval, val);
		} else if (name == "sqrt_math") {
			ValueGrad::vg_sqrt(mval, val);
		} else if (name == "exp_math") {
			ValueGrad::vg_exp(mval, val);
		} else {
			Assert(false, "NYI");
		}
	} else {
		Assert(false, "NYI");
	}
}

void BoolAutoDiff::visit( TUPLE_R_node& node) {
	if (node.mother->type == bool_node::UFUN) {
		Assert(((UFUN_node*)(node.mother))->multi_mother.size() == 1, "NYI"); // TODO: This assumes that the ufun has a single output
		ValueGrad* mval = v(node.mother);
		ValueGrad* val = v(node);
		ValueGrad::vg_copy(mval, val);
	} else {
		Assert(false, "NYI");
	}
}

void BoolAutoDiff::run(const gsl_vector* ctrls_p, const map<int, int>& inputValues_p) {
	Assert(ctrls->size == ctrls_p->size, "BoolAutoDiff ctrl sizes are not matching");

	for (int i = 0; i < ctrls->size; i++) {
		gsl_vector_set(ctrls, i, gsl_vector_get(ctrls_p, i));
	}
	inputValues = inputValues_p; // this does copying again

	for(BooleanDAG::iterator node_it = bdag.begin(); node_it != bdag.end(); ++node_it){
		//cout << (*node_it)->lprint() << endl;
		(*node_it)->accept(*this);
	}
}

bool BoolAutoDiff::checkAll(const gsl_vector* ctrls_p, const map<int, int>& inputValues_p) {
    Assert(ctrls->size == ctrls_p->size, "BoolAutoDiff ctrl sizes are not matching");
    
    for (int i = 0; i < ctrls->size; i++) {
        gsl_vector_set(ctrls, i, gsl_vector_get(ctrls_p, i));
    }
    inputValues = inputValues_p; // this does copying again
    
    for(BooleanDAG::iterator node_it = bdag.begin(); node_it != bdag.end(); ++node_it){
        //cout << (*node_it)->lprint() << endl;
        (*node_it)->accept(*this);
    }
    
    bool failed = false;
    for (BooleanDAG::iterator node_it = bdag.begin(); node_it != bdag.end(); node_it++) {
        bool_node* node = *node_it;
        if (node->type == bool_node::ASSERT) {
            ASSERT_node* an = (ASSERT_node*) node;
            Assert(hasDist(node->mother), "weoqypq");
            float dist = getVal(node->mother);
            if (an->isHard()) {
                if (dist > 0.05) {
                    cout << "Failed " << node->lprint() << " " << dist << endl;
                    failed = true;
                }
            } else {
                if (dist < -0.05) {
                    cout << "Failed " << node->lprint() << " " << dist << endl;
                    failed = true;
                    
                }
            }
        }
        if (Util::isSqrt(node)) {
            bool_node* xnode = ((UFUN_node*)node)->multi_mother[0];
            Assert(hasVal(node->mother), "weoqypq");
            float dist = getVal(node->mother);
            if (dist < -0.05) {
                cout << "Failed " << node->lprint() << " " << dist << endl;
                failed =  true;
            }
        }
    }
    return !failed;
}

bool BoolAutoDiff::hasDist(bool_node* n) {
	DistanceGrad* dg = d(n);
	return dg->set;
}

double BoolAutoDiff::computeDist(bool_node* n, gsl_vector* distgrad) {
	DistanceGrad* dg = d(n);
	if (dg->set) {
        /*if (gsl_blas_dnrm2(dg->grad) > 1e10 || dg->dist > 1e10) {
            cout << "LARGE VALUES" << endl;
            cout << n->lprint() << " " << dg->dist << endl;
            for (int i = 0; i < dg->grad->size; i++) {
                cout << gsl_vector_get(dg->grad, i) << ";";
            }
            cout << endl;
        }*/
		gsl_vector_memcpy(distgrad, dg->grad);
		return dg->dist;
	} else {
        if (n->type == bool_node::CTRL && n->getOtype() == OutType::BOOL) {
            string name = n->get_name();
            int idx = -1;
            if (boolCtrls.find(name) != boolCtrls.end()) {
                idx = boolCtrls[name];
            } else {
                Assert(false, "qwe8yqwi");
            }
            GradUtil::default_grad(distgrad);
            gsl_vector_set(distgrad, idx, 1.0);
            return gsl_vector_get(ctrls, idx);
        }
		Assert(false, "Value not set -- check hasDist() first");
	}
}

bool BoolAutoDiff::hasVal(bool_node* n) {
    ValueGrad* val = v(n);
    return val->set;
}

double BoolAutoDiff::computeVal(bool_node* n, gsl_vector* distgrad) {
    ValueGrad* val = v(n);
    if (val->set) {
        gsl_vector_memcpy(distgrad, val->getGrad());
        return val->getVal();
    } else {
        Assert(false, "Value not set -- check hasDist() first");
    }
}



double BoolAutoDiff::computeError(bool_node* n, int expected, gsl_vector* errorGrad) {
	double error = 0.0;
	DistanceGrad* dg = d(n);
	if (dg->set) {
		double dist = dg->dist;
		gsl_vector* distgrad = dg->grad;
		if ((expected == 1 && dist < 0) || (expected == 0 && dist > 0)) {
			error += abs(dist);
			gsl_vector_memcpy(GradUtil::tmp3, distgrad);
			gsl_vector_scale(GradUtil::tmp3, dist >= 0 ? 1 : -1);
			gsl_vector_add(errorGrad, GradUtil::tmp3);
		}
	}
	return error;
}

bool BoolAutoDiff::hasSqrtDist(bool_node* n) {
    UFUN_node* un = (UFUN_node*) n;
    bool_node* x = un->multi_mother[0];
    return hasVal(x);
}
double BoolAutoDiff::computeSqrtError(bool_node* n, gsl_vector* errorGrad) {
    UFUN_node* un = (UFUN_node*) n;
    bool_node* x = un->multi_mother[0];
    ValueGrad* xval = v(x);
    if (xval->set) {
        double dist = xval->getVal();
        gsl_vector* distgrad = xval->getGrad();
        if (dist < 0) {
            error += abs(dist);
            gsl_vector_memcpy(GradUtil::tmp3, distgrad);
            gsl_vector_scale(GradUtil::tmp3, dist >= 0 ? 1 : -1);
            gsl_vector_add(errorGrad, GradUtil::tmp3);
        }
    }
    return error;
}
double BoolAutoDiff::computeSqrtDist(bool_node* n, gsl_vector* errorGrad) {
    UFUN_node* un = (UFUN_node*) n;
    bool_node* x = un->multi_mother[0];
    return computeVal(x, errorGrad);
}

bool BoolAutoDiff::check(bool_node* n, int expected) {
	DistanceGrad* dg = d(n);
	if (dg->set) {
		double dist = dg->dist;
		if ((expected == 1 && dist < 0) || (expected == 0 && dist > 0)) {
			return false;
		}
	}
	return true;
}


double BoolAutoDiff::getVal(bool_node* n) {
    if (n->getOtype() == OutType::FLOAT || n->type == bool_node::UFUN) {
        ValueGrad* val = v(n);
        return val->getVal();
    } else {
        DistanceGrad* dg = d(n);
        return dg->dist;
    }
}

gsl_vector* BoolAutoDiff::getGrad(bool_node* n) {
    if (n->getOtype() == OutType::FLOAT || n->type == bool_node::UFUN) {
        ValueGrad* val = v(n);
        return val->getGrad();
    } else {
        DistanceGrad* dg = d(n);
        return dg->grad;
    }
}
