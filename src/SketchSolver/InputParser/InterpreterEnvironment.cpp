#include "InterpreterEnvironment.h"
//#include "ABCSATSolver.h"
#include "InputReader.h"
#include "CallGraphAnalysis.h"
#include "ComplexInliner.h"
#include "DagFunctionToAssertion.h"
#include "InputReader.h" // INp yylex_init, yyparse, etc.

#include "Util.h"

#ifdef CONST
#undef CONST
#endif



class Strudel{
	vector<Tvalue>& vals;
	SATSolver* solver;
	FloatManager& floats;
public:
	Strudel(vector<Tvalue>& vtv, SATSolver* solv, FloatManager& fm):vals(vtv), solver(solv), floats(fm){
		cout<<"This is strange size="<<vtv.size()<<endl;
	}




int valueForINode(INTER_node* inode, VarStore& values, int& nbits){
	Tvalue& tv = vals[inode->id];
	int retval = tv.eval(solver);
	{ cout<<" input "<<inode->get_name()<<" has value "<< retval <<endl; }
	return retval;
}

	void checker(BooleanDAG* dag, VarStore& values, bool_node::Type type){
		cout<<"Entering ~!"<<endl;
		BooleanDAG* newdag = dag->clone();
		vector<bool_node*> inodeList = newdag->getNodesByType(type);
			
		// cout<<" * Specializing problem for "<<(type == bool_node::CTRL? "controls" : "inputs")<<endl; 
		cout<<" * Before specialization: nodes = "<<newdag->size()<<" Ctrls = "<<  inodeList.size() <<endl;	
		{
			DagOptim cse(*newdag, floats);			
			
			BooleanDAG* cl = newdag->clone();
			for(int i=0; i<newdag->size() ; ++i ){ 
				// Get the code for this node.				
				if((*newdag)[i] != NULL){
					if((*newdag)[i]->type == bool_node::CTRL){
						INTER_node* inode = dynamic_cast<INTER_node*>((*newdag)[i]);	
						int nbits;
						int t = valueForINode(inode, values, nbits);
						bool_node * repl=NULL;
						if( nbits ==1 ){
							repl = cse.getCnode( t == 1 );						
						}else{
							repl = cse.getCnode( t);							
						}
						Tvalue& tv = vals[i];
						cout<<"ctrl=";
						tv.print(cout, solver);
						cout<<endl;
						Assert( (*newdag)[inode->id] == inode , "The numbering is wrong!!");
						newdag->replace(inode->id, repl);
					}else{
						bool_node* node = cse.computeOptim((*newdag)[i]);
						Tvalue& tv = vals[i];
						cout<<" old = "<<(*cl)[i]->lprint()<<" new "<<node->lprint()<<"  ";
						tv.print(cout, solver);
						if(node->type == bool_node::CONST && cse.getIval( node )==tv.eval(solver)){
							cout<<" good";
						}else{
							cout<<" bad";
						}
						cout<<endl;
						
						if((*newdag)[i] != node){														
								newdag->replace(i, node);							
						}
					}
				}
			}
		}
	}

};





InterpreterEnvironment::~InterpreterEnvironment(void)
{
	for(map<string, BooleanDAG*>::iterator it = functionMap.begin(); it != functionMap.end(); ++it){
		it->second->clear();
		delete it->second;
	}
	if(bgproblem != NULL){
		bgproblem->clear();
		delete bgproblem;
	}
	delete finder;
	delete _pfind;
}

/* Runs the input command in interactive mode. 'cmd' can be:
 * exit -- exits the solver
 * print -- print the controls
 * import -- read the file generated by the front-end
 */
int InterpreterEnvironment::runCommand(const string& cmd, list<string*>& parlist){
	if(cmd == "exit"){
		return 0;
	}
	if(cmd == "print"){
		if(parlist.size() > 0){
			printControls(*parlist.front());
			for(list<string*>::iterator it = parlist.begin(); it != parlist.end(); ++it){
				delete *it;
			}
		}else{
			printControls("");
		}
		return -1;
	}
	if(cmd == "import"){
					
		string& fname = 	*parlist.front();
		cout<<"Reading SKETCH Program in File "<<fname<<endl;
		
		
		void* scanner;
		INp::yylex_init(&scanner);
		INp::yyset_in(fopen(fname.c_str(), "r"), scanner);			
		int tmp = INp::yyparse(scanner);
		INp::yylex_destroy(scanner);
		cout<<"DONE INPUTING"<<endl;
		for(list<string*>::iterator it = parlist.begin(); it != parlist.end(); ++it){
			delete *it;
		}
		if(tmp != 0) return tmp;
		return -1;			
	}
	
	Assert(false, "NO SUCH COMMAND"<<cmd);
	return 1;
}

/* Takes the specification (spec) and implementation (sketch) and creates a
 * single function asserting their equivalence. The Miter is created for
 * expressions 'assert sketch SKETCHES spec' in the input file to back-end.
 */
BooleanDAG* InterpreterEnvironment::prepareMiter(BooleanDAG* spec, BooleanDAG* sketch, int inlineAmnt){
	if(params.verbosity > 2){
		
		cout<<"* before  EVERYTHING: "<< spec->get_name() <<"::SPEC nodes = "<<spec->size()<<"\t "<< sketch->get_name() <<"::SKETCH nodes = "<<sketch->size()<<endl;
	}

	if(params.verbosity > 2){
		cout<<" INBITS = "<<params.NINPUTS<<endl;
		cout<<" CBITS  = "<<INp::NCTRLS<<endl;
	}

	{
		Dout( cout<<"BEFORE Matching input names"<<endl );
		vector<bool_node*>& specIn = spec->getNodesByType(bool_node::SRC);
		vector<bool_node*>& sketchIn = sketch->getNodesByType(bool_node::SRC);

		int inints = 0;
		int inbits = 0;

		Assert(specIn.size() <= sketchIn.size(), "The number of inputs in the spec and sketch must match");	
		for(int i=0; i<specIn.size(); ++i){
			SRC_node* sknode = dynamic_cast<SRC_node*>(sketchIn[i]);
			SRC_node* spnode = dynamic_cast<SRC_node*>(specIn[i]);
			Dout( cout<<"Matching inputs spec: "<<sknode->name<<" with sketch: "<<spnode->name<<endl );
			sketch->rename(sknode->name, spnode->name);
			if(sketchIn[i]->getOtype() == OutType::BOOL){
				inbits++;
			}else{
				inints++;
			}
      if (sknode->isTuple) {
        if (sknode->depth == -1)
          sknode->depth = params.srcTupleDepth;
      }
		}

		if(params.verbosity > 2){
			cout<<" input_ints = "<<inints<<" \t input_bits = "<<inbits<<endl;
		}

	}

	{
		Dout( cout<<"BEFORE Matching output names"<<endl );
		vector<bool_node*>& specDST = spec->getNodesByType(bool_node::DST);
		vector<bool_node*>& sketchDST = sketch->getNodesByType(bool_node::DST);
		Assert(specDST.size() == sketchDST.size(), "The number of inputs in the spec and sketch must match");	
		for(int i=0; i<sketchDST.size(); ++i){
			DST_node* spnode = dynamic_cast<DST_node*>(specDST[i]);
			DST_node* sknode = dynamic_cast<DST_node*>(sketchDST[i]);
			sketch->rename(sknode->name, spnode->name);			
		}
	}

	
	
	//spec->repOK();
	//sketch->repOK();


	if(false){
		CallGraphAnalysis cga;
		cout<<"sketch:"<<endl;
		cga.process(*sketch, functionMap, floats);
		cout<<"spec:"<<endl;
		cga.process(*spec, functionMap, floats);
	}
	
 	if(params.olevel >= 3){
		if(params.verbosity > 3){ cout<<" Inlining amount = "<<inlineAmnt<<endl; }
		{
			if(params.verbosity > 3){ cout<<" Inlining functions in the sketch."<<endl; }
			try {
				doInline(*sketch, functionMap, inlineAmnt, replaceMap);
			}catch (BadConcretization& bc) {
				sketch->clear();
				spec->clear();
				delete sketch;
				delete spec;
				throw bc;
			}
			
			/*
			ComplexInliner cse(*sketch, functionMap, params.inlineAmnt, params.mergeFunctions );	
			cse.process(*sketch);
			*/
		}
		{
			if(params.verbosity > 3){ cout<<" Inlining functions in the spec."<<endl; }
			try {
				doInline(*spec, functionMap, inlineAmnt, replaceMap);
			} catch (BadConcretization& bc) {
				sketch->clear();
				spec->clear();
				delete sketch;
				delete spec;
				throw bc;
			}
			
			/*
			ComplexInliner cse(*spec, functionMap,  params.inlineAmnt, params.mergeFunctions  );	
			cse.process(*spec);
			*/
		}
		
	}
	//spec->repOK();
	//sketch->repOK();
    Assert(spec->getNodesByType(bool_node::CTRL).size() == 0, "ERROR: Spec should not have any holes!!!");
  
  if(false){
		/* Eliminates uninterpreted functions */
        DagElimUFUN eufun;
		eufun.process(*spec);
        
        
     	/* ufunSymmetry optimizes based on the following Assumption: -- In the sketch if you have uninterpreted functions it 
can only call them with the parameters used in the spec */
		if(params.ufunSymmetry){ eufun.stopProducingFuns(); }
        eufun.process(*sketch);
       
	}
    
    {
        //Post processing to replace ufun inputs with tuple of src nodes.
       replaceSrcWithTuple(*spec);
       replaceSrcWithTuple(*sketch);
    }
	//At this point spec and sketch may be inconsistent, because some nodes in spec will have nodes in sketch as their children.
    spec->makeMiter(sketch);
	BooleanDAG* result = spec;
	
	if(params.verbosity > 6){ cout<<"after Creating Miter: Problem nodes = "<<result->size()<<endl; }
		

	result = runOptims(result);
  return result;
}

bool_node* createTupleSrcNode(string tuple_name, string node_name, int depth, vector<bool_node*>& newnodes, bool ufun) {
  if (depth == 0) {
    CONST_node* cnode = new CONST_node(-1);
    newnodes.push_back(cnode);
    return cnode;
  }
  
  Tuple* tuple_type = dynamic_cast<Tuple*>(OutType::getTuple(tuple_name));
  TUPLE_CREATE_node* new_node = new TUPLE_CREATE_node();
  new_node->depth = depth;
  new_node->setName(tuple_name);
  int size = tuple_type->actSize;
  for (int j = 0; j < size ; j++) {
    stringstream str;
    str<<node_name<<"_"<<j;
    
    OutType* type = tuple_type->entries[j];
    
    if (type->isTuple) {
      new_node->multi_mother.push_back(createTupleSrcNode(((Tuple*)type)->name, str.str(), depth - 1, newnodes, ufun));
    } else if (type->isArr && ((Arr*) type)->atype->isTuple) {
      CONST_node* cnode = new CONST_node(-1);
      newnodes.push_back(cnode);
      new_node->multi_mother.push_back(cnode);
      
    } else {
    
    SRC_node* src =  new SRC_node( str.str() );
    
    int nbits = 0;
    if (type == OutType::BOOL || type == OutType::BOOL_ARR) {
      nbits = 1;
    }
    if (type == OutType::INT || type == OutType::INT_ARR) {
      nbits = 2;
    }
      
    if (nbits > 1) { nbits = PARAMS->NANGELICS; }
    src->set_nbits(nbits);
    if(type == OutType::INT_ARR || type == OutType::BOOL_ARR) {
      int sz = 1 << PARAMS->NINPUTS;
      src->setArr(sz);
    }
    newnodes.push_back(src);
    
    new_node->multi_mother.push_back(src);
    }
  }
  
  CONST_node* cnode = new CONST_node(-1);
  newnodes.push_back(cnode);
  for (int i = size; i < tuple_type->entries.size(); i++) {
    new_node->multi_mother.push_back(cnode);
  }
  new_node->addToParents();
  newnodes.push_back(new_node);
  
  if (ufun) return new_node;

  ARRACC_node* ac = new ARRACC_node();
  stringstream str;
  str<<node_name<<"__";

  SRC_node* src = new SRC_node(str.str());
  src->set_nbits(1);
  newnodes.push_back(src);
  
  ac->mother = src;
  ac->multi_mother.push_back(cnode);
  ac->multi_mother.push_back(new_node);
  ac->addToParents();
  newnodes.push_back(ac);
  return ac;
}


void InterpreterEnvironment::replaceSrcWithTuple(BooleanDAG& dag) {
    vector<bool_node*> newnodes;
    for(int i=0; i<dag.size(); ++i ){
		if (dag[i]->type == bool_node::SRC) {
            SRC_node* srcNode = dynamic_cast<SRC_node*>(dag[i]);
            if (srcNode->isTuple) {
                int depth = srcNode->depth;
                if (depth == -1) depth = params.srcTupleDepth;
                bool_node* new_node = createTupleSrcNode(srcNode->tupleName, srcNode->get_name(), depth, newnodes, srcNode->ufun);
                dag.replace(i, new_node);
            }
        }
	}
  
    dag.addNewNodes(newnodes);
	newnodes.clear();
    dag.removeNullNodes();
}




void InterpreterEnvironment::doInline(BooleanDAG& dag, map<string, BooleanDAG*> functionMap, int steps, map<string, map<string, string> > replaceMap){
	//OneCallPerCSiteInliner fin;
	// InlineControl* fin = new OneCallPerCSiteInliner(); //new BoundedCountInliner(PARAMS->boundedCount);
	TheBestInliner fin(steps, params.boundmode == CommandLineArgs::CALLSITE);
	/*
	if(PARAMS->boundedCount > 0){
		fin = new BoundedCountInliner(PARAMS->boundedCount);
	}else{
		fin = new OneCallPerCSiteInliner();
	}	 
	*/
	DagFunctionInliner dfi(dag, functionMap, replaceMap, floats, &hardcoder, params.randomassign, &fin, params.onlySpRandAssign,
                         params.spRandBias); 

	int oldSize = -1;
	bool nofuns = false;
	for(int i=0; i<steps; ++i){
		int t = 0;
    int ct = 0;
		do{
			if (params.randomassign && params.onlySpRandAssign) {
				if (ct < 2) {
				dfi.turnOffRandomization();
				ct++;
				} else {
				dfi.turnOnRandomization();
				}
			}
            dfi.process(dag);
			// dag.lprint(cout);
            // dag.repOK();
			set<string>& dones = dfi.getFunsInlined();						
			if(params.verbosity> 6){ cout<<"inlined "<<dfi.nfuns()<<" new size ="<<dag.size()<<endl; }
			if (params.bndDAG > 0 && dag.size() > params.bndDAG) {
				cout << "WARNING: Preemptively stopping CEGIS because the graph size exceeds the limit: " << params.bndDAG << endl;
				exit(1);
			}
			if(oldSize > 0){
				if(dag.size() > 400000000 && dag.size() > oldSize * 10){
					i=steps;
					cout<<"WARNING: Preemptively stopping inlining because the graph was growing too big too fast"<<endl; 
					break;
				}
				if((dag.size() > 400000 && dag.size() > oldSize * 2)|| dag.size() > 1000000){
					hardcoder.tryHarder();
				}
			}
			oldSize = dag.size();
			++t;			
		}while(dfi.changed());
		if(params.verbosity> 6){ cout<<"END OF STEP "<<i<<endl; }
		// fin.ctt.printCtree(cout, dag);

		fin.clear();
		if(t==1 && params.verbosity> 6){ cout<<"Bailing out"<<endl; break; }
	}
	hardcoder.afterInline();		
	{
		DagFunctionToAssertion makeAssert(dag, functionMap, floats);
		makeAssert.process(dag);
	}
	
}


ClauseExchange::ClauseExchange(MiniSATSolver* ms, const string& inf, const string& outf)
	:msat(ms), infile(inf), outfile(outf)
{
	failures = 0;

	msat->getShareable(single, baseline, dble);

	//Wipe the files clean
	{
	FILE* f = fopen(infile.c_str(), "w");	
	fclose(f);
	}
	{
	FILE* f = fopen(outfile.c_str(), "w");	
	fclose(f);
	}	
}

void ClauseExchange::exchange(){
	analyzeLocal();
	int ssize = single.size();
	int dsize = dble.size();	
	if (PARAMS->verbosity > 8) {
		cout << "Before readInfile" << endl;
		printToExchange();
	}
	

	readInfile();

	if (PARAMS->verbosity > 8) {
		cout << "After readInfile" << endl;
		printToExchange();
	}

	if(ssize > 0 || dsize > 0){
		pushOutfile();
	}
}

void ClauseExchange::readInfile(){
	int ssize = single.size();
	int dsize = dble.size();
	int bufsize = ssize + dsize*2 + 3;
	bufsize = bufsize * 3;
	bufsize = max(bufsize, 200);
	vector<int> sbuf(bufsize);
	FILE* f = fopen(outfile.c_str(), "r");
	int rsize = fread(&sbuf[0], sizeof(int), bufsize, f);
	if(rsize < 3){
		fclose(f);
		cout<<"Nothing read"<<endl;
		return;
	}
	ssize = sbuf[0]; dsize=sbuf[1];
	int realsize = ssize + dsize*2 + 3;
	if(rsize != realsize){
		if(rsize > realsize){
			fclose(f);
			cout<<"Corrupted"<<endl;
			return;
		}
		if(rsize != bufsize){
			fclose(f);
			cout<<"Corrupted"<<endl;
			return;
		}
		sbuf.resize(realsize);
		rsize = fread(&sbuf[bufsize], sizeof(int), realsize - bufsize, f);
		fclose(f);
		if(rsize + bufsize != realsize){
			cout<<"Corrupted"<<endl;
			return;
		}
	}else{
		fclose(f);
		sbuf.resize(realsize);
	}
	cout << "Received: ";
	for (int i = 0; i < sbuf.size(); ++i) {
		cout << ", " << sbuf[i];
	}
	cout << endl;
	unsigned chksum = 0;
	for(int i=0; i<sbuf.size()-1; ++i){
		chksum += sbuf[i];
	}
	if(chksum != sbuf[sbuf.size()-1]){
		cout<<"Failed checksum"<<endl;
		return;
	}
	{
		vec<Lit> vl(1);
		for(int i=2; i < 2+ssize; ++i){
			int sin = sbuf[i];
			if(single.count(sin)==0){
				single.insert(sin);
				vl[0] = toLit(sin);
				msat->addHelperClause(vl);
			}
		}
	}
	{
		vec<Lit> vl(2);
		for(int i=2+ssize; i< sbuf.size()-1; i+=2){
			int f = sbuf[i];
			int s = sbuf[i+1];
			pair<int, int> p = make_pair(f, s);
			if(dble.count(p) ==0){
				dble.insert(p);
				vl[0] = toLit(f); vl[1] = toLit(s);
				msat->addHelperClause(vl);
			}
		}
	}
}

void ClauseExchange::pushOutfile(){
	int ssize = single.size();
	int dsize = dble.size();
	unsigned chksum = 0;
	vector<int> sbuf(ssize + dsize*2 + 3);
	chksum += ssize;
	chksum += dsize;
	sbuf[0] = ssize;
	sbuf[1] = dsize;
	int i=2;
	for(set<int>::iterator it = single.begin(); it != single.end(); ++it){
		chksum += *it;
		sbuf[i] = *it; ++i;
	}
	for(set<pair<int, int> >::iterator it = dble.begin(); it != dble.end(); ++it){
		chksum += it->first;
		chksum += it->second;
		sbuf[i] = it->first; ++i;
		sbuf[i] = it->second; ++i;
	}
	sbuf[i] = chksum;
	cout << "Sending: " << endl;
	for (int ii = 0; ii < sbuf.size(); ++ii) {
		cout << ", " << sbuf[ii];
	}
	cout << endl;
	FILE* f = fopen(outfile.c_str(), "w");
	fwrite(&sbuf[0], sizeof(int), i+1, f);
	fclose(f);
}

void ClauseExchange::analyzeLocal(){
	single.clear(); dble.clear();
	msat->getShareable(single, dble, baseline);	
}


void InterpreterEnvironment::share(){
	if(exchanger!=NULL){
		exchanger->exchange();
	}
}

int InterpreterEnvironment::doallpairs(){
	int howmany = params.ntimes;
	if(howmany < 1 || !params.randomassign){ howmany = 1; }
	int result=-1;
	vector<int> rd(2);
	map<int, vector<double> > scores;
    
    // A dummy ctrl for inlining bound
	CTRL_node* inline_ctrl = NULL;
    if (params.randomInlining) {
        inline_ctrl = new CTRL_node();
        inline_ctrl->name = "inline";
        hardcoder.declareControl(inline_ctrl);
    }
    
	if(howmany > 1 || params.randomassign){
        for(map<string, BooleanDAG*>::iterator it = functionMap.begin(); it != functionMap.end(); ++it){
			BooleanDAG* bd = it->second;
			vector<bool_node*>& ctrl = bd->getNodesByType(bool_node::CTRL);
			for(int i=0; i<ctrl.size(); ++i){
				hardcoder.declareControl((CTRL_node*) ctrl[i]);
			}
		}
		if(exchanger == NULL && howmany > 5){
			string inf = params.inputFname;
			inf += ".com";
			exchanger = new ClauseExchange(hardcoder.getMiniSat(), inf, inf);			
			if(params.randdegree == 0){
				rd[0] = 10;
				rd[1] = 5;
			}
		}			 
	}
	maxRndSize = 0;
	hardcoder.setHarnesses(spskpairs.size());	
	for(int tt = 0; tt<howmany; ++tt){
		if(howmany>1){ cout<<"ATTEMPT "<<tt<<endl; }
		if(tt % 5 == 4){
			share();
			if(params.randdegree == 0){
				hardcoder.adjust(rd, scores);
			}
		}
		if(params.randdegree == 0){
			hardcoder.setRanddegree(rd[tt % 2]);			
		}

		timerclass roundtimer("Round");
		roundtimer.start();
        
        // Fix a random value to the inlining bound
        int inlineAmnt = params.inlineAmnt;
        int minInlining = 3;
        if (params.inlineAmnt > minInlining && params.randomInlining) {
            inline_ctrl->special_concretize(params.inlineAmnt - minInlining);
            hardcoder.fixValue(*inline_ctrl, params.inlineAmnt - minInlining, 5);
            inlineAmnt = hardcoder.getValue(inline_ctrl->name) + minInlining;
        }
		for(int i=0; i<spskpairs.size(); ++i){
			hardcoder.setCurHarness(i);
			try{
        BooleanDAG* bd= prepareMiter(getCopy(spskpairs[i].first), getCopy(spskpairs[i].second), inlineAmnt);
				result = assertDAG(bd, cout);
				cout<<"RESULT = "<<result<<"  "<<endl;;
				printControls("");				
			}catch(BadConcretization& bc){
				hardcoder.dismissedPending();
				result = 1;
				break;
			}
				if(result!=0){
					break;
				}
				if(hardcoder.isDone()){
					break;
				}
		}
		roundtimer.stop();
		cout<<"**ROUND "<<tt<<" : "<<hardcoder.getTotsize()<<" ";
		roundtimer.print("time");
		cout<<"RNDDEG = "<<hardcoder.getRanddegree()<<endl;
		double comp = log(roundtimer.get_cur_ms()) + hardcoder.getTotsize();
		scores[hardcoder.getRanddegree()].push_back(comp);
		if(result==0){
			cout<<"return 0"<<endl;
			return result;
		}
		reset(); 
		if(hardcoder.isDone()){
			cout<<"return 1"<<endl;
			return 1;
		}
	}
	cout<<"return 2"<<endl;
	return 2; // undefined.
}



int InterpreterEnvironment::assertDAGNumerical(BooleanDAG* dag, ostream& out) {
    Assert(status==READY, "You can't do this if you are UNSAT");
    ++assertionStep;
    
    reasSolver->addProblem(dag);
    
    int solveCode = 0;
    try{
        solveCode = reasSolver->solve();
        reasSolver->get_control_map(currentControls);
    }catch(SolverException* ex){
        cout<<"ERROR "<<basename()<<": "<<ex->code<<"  "<<ex->msg<<endl;
        status=UNSAT;
        return ex->code + 2;
    }catch(BasicError& be){
        reasSolver->get_control_map(currentControls);
        cout<<"ERROR: "<<basename()<<endl;
        status=UNSAT;
        return 3;
    }
    if( !solveCode ){
        status=UNSAT;				
        return 1;	
    }
    
    return 0;
}

int InterpreterEnvironment::assertDAG(BooleanDAG* dag, ostream& out){
    if (params.numericalSolver) {
        return assertDAGNumerical(dag, out);
    }
	Assert(status==READY, "You can't do this if you are UNSAT");
	++assertionStep;	
	
	solver->addProblem(dag);
	
//	cout << "InterpreterEnvironment: new problem" << endl;
//	problem->lprint(cout);

	if(params.superChecks){
		history.push_back(dag->clone());	
	}

	// problem->repOK();
	
		
  	
	if(params.outputEuclid){      		
		ofstream fout("bench.ucl");
		solver->outputEuclid(fout);
	}
  	
	if(params.output2QBF){
		string fname = basename();
		fname += "_2qbf.cnf";
		ofstream out(fname.c_str());
		cout<<" OUTPUTING 2QBF problem to file "<<fname<<endl;
		solver->setup2QBF(out);		
	}
  	
  		
	int solveCode = 0;
	try{
		
		solveCode = solver->solve();
		
		solver->get_control_map(currentControls);
	}catch(SolverException* ex){
		cout<<"ERROR "<<basename()<<": "<<ex->code<<"  "<<ex->msg<<endl;
		status=UNSAT;				
		return ex->code + 2;
	}catch(BasicError& be){
		solver->get_control_map(currentControls);
		cout<<"ERROR: "<<basename()<<endl;
		status=UNSAT;				
		return 3;
	}
	if( !solveCode ){
		status=UNSAT;				
		return 1;	
	}

	if(false){
		statehistory.push_back(solver->find_history);

		for(int i=0; i<history.size(); ++i){
			cout<<" ~~~ Order = "<<i<<endl;
			BooleanDAG* bd = solver->hardCodeINode(history[i], solver->ctrlStore, bool_node::CTRL);
			int sz = bd->getNodesByType(bool_node::ASSERT).size();
			cout<<" ++ Order = "<<i<<" size = "<<sz<<endl;
			if(sz > 0){
				Strudel st(statehistory[i], &finder->getMng(), floats);
				st.checker(history[i] , solver->ctrlStore, bool_node::CTRL);
			}
		}
	}
		
	return 0;

}

int InterpreterEnvironment::assertDAG_wrapper(BooleanDAG* dag){
	ostream& out = std::cout;
	return assertDAG(dag, out);
}

int InterpreterEnvironment::assertDAG_wrapper(BooleanDAG* dag, const char* fileName){
	ofstream out(fileName, ios_base::out);
	return assertDAG(dag, out);
}

BooleanDAG* InterpreterEnvironment::runOptims(BooleanDAG* result){		
	if(params.olevel >= 3){
		DagOptim cse(*result, floats);	
		//cse.alterARRACS();
		cse.process(*result);
	}
	// result->repOK();

	// if(params.verbosity > 3){cout<<"* after OPTIM: Problem nodes = "<<result->size()<<endl;	}
	/*{
		DagOptim op(*result);
		result->replace(5598, op.getCnode(1));
		op.process(*result);
	}*/

	
	
	if(false && params.olevel >= 5){		
		BackwardsAnalysis opt;
		cout<<"BEFORE ba: "<<endl;
		//result->print(cout);
		opt.process(*result);
		cout<<"AFTER ba: "<<endl;
		// result->print(cout);
	}
	// result->repOK();
	if(params.olevel >= 7){
		cout << "BEFORE OptimizeCommutAssoc"<< result->size() << endl;
		DagOptimizeCommutAssoc opt;
		opt.process(*result);
		cout << "AFTER OptimizeCommutAssoc "<<result->size() << endl;
	}
	// result->repOK();
	//result->print(cout) ;

	// cout<<"* after CAoptim: Problem nodes = "<<result->size()<<endl;

	if(params.olevel >= 4){		
		DagOptim cse(*result, floats);	
		if(params.alterARRACS){ 
			cout<<" alterARRACS"<<endl;
			cse.alterARRACS(); 
		}
		cse.process(*result);		
	}
	// result->repOK();	
	if(params.verbosity > 0){ cout<<"* Final Problem size: Problem nodes = "<<result->size()<<endl;	}
	if(params.showDAG){ 
		result->lprint(cout);		
	}
	if(params.outputMRDAG){
		ofstream of(params.mrdagfile.c_str());
		cout<<"Outputing Machine Readable DAG to file "<<params.mrdagfile<<endl;
		result->mrprint(of);
		of.close();
	}
	if(params.outputSMT){
		ofstream of(params.smtfile.c_str());
		cout<<"Outputing SMT for DAG to file "<<params.smtfile<<endl;
		result->smtlinprint(of, params.NINPUTS);
		of.close();
	}
    if(params.outputExistsSMT){
		ofstream of(params.smtfile.c_str());
		cout<<"Outputing SMT for DAG to file "<<params.smtfile<<endl;
		result->smt_exists_print(of);
		of.close();
	}
	return result;
}
