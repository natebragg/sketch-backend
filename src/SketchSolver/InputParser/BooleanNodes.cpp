#include "BooleanNodes.h"
#include "BooleanDAG.h"

#include <sstream>
#include <algorithm>


int bool_node::NEXT_GLOBAL_ID = 0;


void bool_node::replace_child(bool_node* ori, bool_node* replacement){
	child_iter it = children.find(ori);
	if(it != children.end() ){
		children.erase(it);
		children.insert(replacement);
	}

}




void arith_node::replace_child_inParents(bool_node* ori, bool_node* replacement){
	bool_node::replace_child_inParents(ori, replacement);
	for(int i=0; i<multi_mother.size(); ++i){
		multi_mother[i]->replace_child(ori, replacement);
	}
}


void bool_node::replace_child_inParents(bool_node* ori, bool_node* replacement){
	if(mother != NULL){
		mother->replace_child(ori, replacement);
	}
	if(father != NULL){
		father->replace_child(ori, replacement);
	}
}


void bool_node::neighbor_replace(bool_node* replacement){
	bool_node* onode = this;
	onode->dislodge();
	child_iter end = onode->children.end();
	for(child_iter it = onode->children.begin(); 
										it !=end; ++it){
		
		(*it)->replace_parent(onode, replacement);
	}

}



void arith_node::addToParents(bool_node* only_thisone){
  for(vector<bool_node*>::iterator it = multi_mother.begin(); it != multi_mother.end(); ++it){
  	if(*it == only_thisone){
	  	only_thisone->children.insert(this);
		break;
  	}
  }
}



void arith_node::addToParents(){
  bool_node::addToParents();
  for(vector<bool_node*>::iterator it = multi_mother.begin(); it != multi_mother.end(); ++it){
  	if(*it != NULL){
		bool_node* tmp = (*it);
	  	tmp->children.insert(this);
  	}
  }
}


void bool_node::addToParents(){
  if(father != NULL){
    father->children.insert(this);
  }
  if(mother != NULL && father != mother){
    mother->children.insert(this);
  }
}




void arith_node::switchInputs(BooleanDAG& bdag){	
	for(vector<bool_node*>::iterator it = multi_mother.begin(); it != multi_mother.end(); ++it){
	  	if(*it != NULL){
	  		if(  (*it)->type == bool_node::SRC ){
	  			if( bdag.has_name((*it)->name)){
    				(*it) = bdag.get_node((*it)->name);
	  			}
	  		}
	  	}
  	}
  	bool_node::switchInputs(bdag);
}


void bool_node::switchInputs(BooleanDAG& bdag){
	if(father != NULL){
		if(  father->type == bool_node::SRC ){
    		if( bdag.has_name(father->name )){
    			father = bdag.get_node(father->name);
    		}
		}
  	}
	if(mother != NULL){
		if(  mother->type == bool_node::SRC ){
			if( bdag.has_name(mother->name )){
				mother = bdag.get_node(mother->name);
			}
		}
	}
	addToParents();
}



void bool_node::redirectParentPointers(BooleanDAG& oribdag, const vector<const bool_node*>& bdag, bool setChildrn, bool_node* childToInsert){
	if(father != NULL){
		Assert( father->id != -22, "This node should not exist anymore");
		if( oribdag.checkNodePosition(father) ){
			//If the father was from the original bdag, then we switch to the father in the new bdag.
	    	father = (bool_node*) bdag[father->id];
			if(setChildrn){
				/*
				child_iter it = father->children.find(childToInsert);
				if(it != father->children.end() ){
					father->children.erase(it);
				}
				*/
				father->children.insert(this); 
			}
		}
  	}
	if(mother != NULL){
		Assert( mother->id != -22, "This node should not exist anymore");
		if( oribdag.checkNodePosition(mother)){
			mother = (bool_node*) bdag[mother->id];
			if(setChildrn){ 
				/*
				child_iter it = mother->children.find(childToInsert);
				if(it != mother->children.end() ){
					mother->children.erase(it);
				}
				*/
				mother->children.insert(this); 
			}
		}
	}
}


void arith_node::redirectParentPointers(BooleanDAG& oribdag, const vector<const bool_node*>& bdag, bool setChildrn, bool_node* childToInsert){
  bool_node::redirectParentPointers(oribdag, bdag, setChildrn, childToInsert);
  for(vector<bool_node*>::iterator it = multi_mother.begin(); it != multi_mother.end(); ++it){
  	if(*it != NULL){
  		if( oribdag.checkNodePosition(*it) ){
	  		(*it) = (bool_node*) bdag[(*it)->id];
			if(setChildrn){ 
				/*
				child_iter ttt = (*it)->children.find(childToInsert);
				if(ttt != (*it)->children.end() ){
					(*it)->children.erase(ttt);
				}
				*/
				(*it)->children.insert(this);
			}
  		}
  	}
  }
}



void bool_node::redirectPointers(BooleanDAG& oribdag, const vector<const bool_node*>& bdag){
	redirectParentPointers(oribdag, bdag, false, NULL);
	childset bset;
	for(child_iter child = children.begin(); child != children.end(); ++child){
		Assert( (*child)->id != -22, "This node should not exist anymore");
		if( oribdag.checkNodePosition((*child)) ){
			bset.insert((bool_node*) bdag[ (*child)->id ] );
		}else{
			bset.insert(*child);
		}
	}
	swap(bset, children);
}


string bool_node::get_name() const {
    stringstream str;
    if(name.size() > 0)
      str<<name<<"__"<<get_tname();
    else{      
      str<<"name_"<<abs(id)<<"__"<<get_tname();
      
    }    
    //str<<"_"<<this;
    //str<<":"<<id;
    Assert( id != -22, "This is a corpse. It's living gargabe "<<str.str()<<" id ="<<id );
    return str.str();
  }

bool_node::OutType bool_node::getOtype() const{
	Assert(false, "This shouldn't get called");
	return BOOL;
}

bool_node::OutType arith_node::getOtype() const{
	return INT;
}

void bool_node::set_layer(bool isRecursive){
	if(!isRecursive){ layer = 0;}
  if(mother != NULL){
	  if(isRecursive && mother->layer < 0){ mother->set_layer(isRecursive); }
      layer = mother->layer + 1;
  }
  if(father != NULL){
	  if(isRecursive && father->layer < 0){
		father->set_layer(isRecursive); 
	  }
	if(father->layer >= layer){
		layer = father->layer + 1;
	}
  }
}


void arith_node::set_layer(bool isRecursive){
	bool_node::set_layer(isRecursive);

	for(vector<bool_node*>::iterator it = multi_mother.begin(); it != multi_mother.end(); ++it){
		if(*it != NULL){
			if(isRecursive && (*it)->layer < 0){
			(*it)->set_layer(isRecursive); 
			}
			if((*it)->layer >= layer){
			layer = (*it)->layer + 1;
			}
		}
	}	
}


int bool_node::do_dfs(int idx){
	Assert( id != -22, "This node should be dead (dfs) "<<this->get_name());
  if( flag != 0)
    return idx;
  flag = 1;    
  for(child_iter child = children.begin(); child != children.end(); ++child){  	
    idx = (*child)->do_dfs(idx);
	Assert(idx >= 0, "This shouldn't happen");
  }
  id = idx;
  return idx-1;
}

int bool_node::back_dfs(int idx){
	Assert( id != -22, "This node should be dead (back_dfs)"<<this->get_name());
  if( flag != 0)
    return idx;
  flag = 1;    
  if(father != NULL){
  	 idx = father->back_dfs(idx);
  }
  if(mother != NULL){
  	 idx = mother->back_dfs(idx);
  }
  id = idx;
  return idx+1;
}


int arith_node::back_dfs(int idx){
	Assert( id != -22, "This node should be dead (arith:back_dfs) "<<this->get_name());
  if( flag != 0)
    return idx;
  flag = 1;    
  if(father != NULL){
  	 idx = father->back_dfs(idx);
  }
  if(mother != NULL){
  	 idx = mother->back_dfs(idx);
  }
  for(vector<bool_node*>::iterator it = multi_mother.begin(); it != multi_mother.end(); ++it){
  	if(*it != NULL){
	  	idx = (*it)->back_dfs(idx);
  	}
  }
  id = idx;
  return idx+1;
}



void bool_node::replace_parent(const bool_node * oldpar, bool_node* newpar){
	Assert( oldpar != NULL, "oldpar can't be null");
	Assert( newpar != NULL, "oldpar can't be null");
	if(father == oldpar){
  		father = newpar;
  		//oldpar->remove_child(this);
  		//we assume the old parent is going to be distroyed, so we don't
  		//bother modifying it.
		newpar->children.insert( this );
  	}
  	if(mother == oldpar){
  		mother = newpar;
  		//oldpar->remove_child(this);
  		//we assume the old parent is going to be distroyed, so we don't
  		//bother modifying it.
  		newpar->children.insert( this );
  	}
}


void arith_node::replace_parent(const bool_node * oldpar, bool_node* newpar){
	bool_node::replace_parent(oldpar, newpar);
	replace<vector<bool_node*>::iterator, bool_node*>(multi_mother.begin(), multi_mother.end(), ( bool_node*)oldpar, newpar);
	newpar->children.insert( this );	
}




void arith_node::printSubDAG(ostream& out, set<const bool_node* >& s)const{	
	int i=0;
	s.insert(this);
	for(vector<bool_node*>::const_iterator it = multi_mother.begin(); it != multi_mother.end(); ++it, ++i){
	  	if(*it != NULL && s.count(*it) == 0){
	  		(*it)->printSubDAG(out, s);  		
	  	}
	}
	bool_node::printSubDAG(out, s);
}


void bool_node::printSubDAG(ostream& out, set<const bool_node* >& s)const{
	s.insert(this);
	if( father != NULL && s.count(father) == 0 ){
			father->printSubDAG(out, s);
	}	
	if(mother != NULL && s.count(mother) == 0){
			mother->printSubDAG(out, s);
	}
	outDagEntry(out);	
}



void bool_node::outDagEntry(ostream& out) const{
	if( father != NULL){
        out<<" "<<father->get_name()<<" -> "<<get_name()<<" ; "<<endl;
    }
    if( mother != NULL){
          out<<" "<<mother->get_name()<<" -> "<<get_name()<<" ; "<<endl;
    }
    if(father == NULL && mother == NULL){
   // 	  Dout( out<<"// orphan node: "<<get_name()<<" ; "<<endl );
    }
	for(child_citer child = children.begin(); child != children.end(); ++child){  	
    //	Dout( out<<"// "<<get_name()<<" -> "<<children[i]->get_name()<<" ; "<<endl );
    }
}


void arith_node::outDagEntry(ostream& out) const{
	bool_node::outDagEntry(out);
	int i=0;
	for(vector<bool_node*>::const_iterator it = multi_mother.begin(); it != multi_mother.end(); ++it, ++i){
	  	if(*it != NULL){
	  		out<<" "<<(*it)->get_name()<<" -> "<<get_name()<<" ; "<<endl;	  		
	  	}
	}
}


void arith_node::removeFromParents(bool_node* bn){
	bool_node::removeFromParents(bn);
	bool_node* tmp = NULL;
	for(vector<bool_node*>::iterator it = multi_mother.begin(); it != multi_mother.end(); ++it){
	  	if(*it != NULL && *it != tmp){
	  		(*it)->remove_child(bn);
			tmp = *it;
	  	}
	}
}


void bool_node::removeFromParents(bool_node* bn){
  if(father != NULL){
  	 father->remove_child(bn);
  }
  if(mother != NULL){
  	mother->remove_child(bn);
  }
}


void bool_node::dislodge(){
  if(father != NULL){
  	 father->remove_child(this);
  }
  if(mother != NULL){
  	mother->remove_child(this);
  }
}

void arith_node::dislodge(){
	bool_node::dislodge();
	bool_node* tmp = NULL;
	for(vector<bool_node*>::iterator it = multi_mother.begin(); it != multi_mother.end(); ++it){
	  	if(*it != NULL && *it != tmp){
	  		(*it)->remove_child(this);	
			tmp = *it;
	  	}
	}
}
