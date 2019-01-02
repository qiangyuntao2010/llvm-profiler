#include "corelab/CAMP/CampCache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAMP_CONTEXT_TREE_APPROACH

//#define debug_for_camp_cache

using namespace corelab;
using namespace llvm;

char CAMPCache::ID = 0;
static RegisterPass<CAMPCache> X("camp-cache", "modify for cache grind", false, false);

bool CAMPCache::runOnModule(Module& M) {

	module = &M;

	cxtTreeBuilder = &getAnalysis<ContextTreeBuilder>();

	#ifdef CAMP_CONTEXT_TREE_APPROACH

	
	pCxtTree = cxtTreeBuilder->getContextTree();
	locIdOf_callSite = cxtTreeBuilder->getLocIDMapForCallSite();
	locIdOf_indCall = cxtTreeBuilder->getLocIDMapForIndirectCalls();

	for (Iterator =0 /*= pCxtTree->begin()*/; Iterator < pCxtTree->size() /*!= pCxtTree->end()*/; ++Iterator){
		ContextTreeNode *node = pCxtTree->at(Iterator); // *Iterator;
		if ( node->isCallSite && node->getUCID() != 0 )
			if ( node->done == false && (pCxtTree->at(getParentUCID(node)))->done == false ){
				Instruction *iInst = const_cast<Instruction*>(node->invokeOrCallInst);
				if ( !recurDuplicateFunction ( node, Iterator, iInst ) )
					node->invokeOrCallInst->dump();
			}
			else if ( node->done == false && (pCxtTree->at(getParentUCID(node)))->done == true)
				node->done = true;

#ifdef debug_for_camp_cache

		errs() << "Node " << node->getUCID() << " done \n";
	
#endif

		if (Iterator == pCxtTree->size()/*pCxtTree->end()*/)
		{
#ifdef debug_for_camp_cache
			errs() << "done all\b";
#endif
			break;
		}
	}

/*	
	for (Iterator = pCxtTree->begin(); Iterator != pCxtTree->end(); ++Iterator){
		ContextTreeNode *node = *Iterator;
		if ( node->loop == NULL && node->isCallSite && node->getUCID() != 0 ){
			assert( isa<InvokeInst>(node->invokeOrCallInst) || isa<CallInst>(node->invokeOrCallInst) );
			Instruction *Inst = const_cast<Instruction *>(node->invokeOrCallInst);
			assert(Inst != NULL);
			CallInst *CInst = dyn_cast<CallInst>(Inst);
			assert(CInst != NULL);
			Function* F = (dyn_cast<CallInst>(Inst))->getCalledFunction();
			
			if ( F!=NULL ){
				//errs() << node->getUCID() <<"\n";
				ValueToValueMapTy VMap;
				Function* duplicateFunction = CloneFunction(F, VMap, false);
				duplicateFunction->setName(F->getName() + ":" + to_string(node->getUCID()));
				duplicateFunction->setLinkage(GlobalValue::InternalLinkage);
				F->getParent()->getFunctionList().push_back(duplicateFunction);
				
				//errs() << F->getName() <<"\n";

				CallInst *CI = dyn_cast<CallInst>(Inst);

				int argSize = CI->getNumArgOperands();
				std::vector<Value*> actuals(argSize);
				for (int inx =0; inx < argSize; inx ++){
					Value *av = CI->getArgOperand(inx);
					actuals[inx] = av;
				}

				CallInst *DCI = CallInst::Create(duplicateFunction, actuals, "", Inst);
				CI->replaceAllUsesWith( DCI );
				//Inst->removeFromParent();
			}
		}
	}
*/
// don't do this when erasing
/*
	for (auto it = pCxtTree->begin(); it != pCxtTree->end(); ++it){
		ContextTreeNode *node = *it;
		if ( node->loop == NULL && node->isCallSite && node->getUCID() != 0 ){
			assert(node->invokeOrCallInst != NULL);
			Instruction *Inst = const_cast<Instruction *>(node->invokeOrCallInst);
			assert(Inst != NULL);
			Inst->dump();
			if (Inst != NULL){
				CallInst *CInst = dyn_cast<CallInst>(Inst);
				assert(CInst != NULL);
				Function* F = (dyn_cast<CallInst>(Inst))->getCalledFunction();
				//errs() << F->getName() <<"\n";
				if ( F!=NULL ){
					Inst->eraseFromParent();
				}}
		}}
*/
/*	
	int j=0;
	for( std::pair<const Instruction *, LocalContextID> &normalCallSite : *locIdOf_callSite ){
		if (normalCallSite.second != (LocalContextID)(-1) 
				&& dyn_cast<CallInst>(normalCallSite.first)->getCalledFunction()!=NULL){

			//one target indirect call = normalcallsite
			
			//normalCallSite.first->dump();

			ValueToValueMapTy VMap;
			//VMap.clear();
			Function* F = dyn_cast<CallInst>(normalCallSite.first)->getCalledFunction();
			assert(F!=NULL);
			errs() << F->getName();
			Function* duplicateFunction = CloneFunction(F, VMap, false);
			duplicateFunction->setName(F->getName()+":"+to_string(j));

			duplicateFunction->setLinkage(GlobalValue::InternalLinkage);
			F->getParent()->getFunctionList().push_back(duplicateFunction);
			
			Instruction *Inst = const_cast<Instruction *>(normalCallSite.first);
			CallInst *CI = dyn_cast<CallInst>(Inst);

			int argSize = CI->getNumArgOperands();
			std::vector<Value*> actuals(argSize);
			for (int inx =0; inx < argSize; inx ++){
				Value *av = CI->getArgOperand(inx);
				actuals[inx] = av;
			}
			
			CallInst *DCI = CallInst::Create(duplicateFunction, actuals, "", Inst);

			CI->replaceAllUsesWith( DCI );

			//Inst->removeFromParent();//it makes error
			Inst->eraseFromParent();
			j++;
		}
	}
*/

/*
for( std::pair<const Instruction *, LocalContextID> &normalCallSite : *locIdOf_callSite ){
		if (normalCallSite.second != (LocalContextID)(-1) 
				&& dyn_cast<CallInst>(normalCallSite.first)->getCalledFunction()!=NULL){
			Instruction *Inst = const_cast<Instruction *>(normalCallSite.first);
			Inst->eraseFromParent();
			
			for(auto e: Inst->users())
				errs() << "+++> "<<*e<<"\n";
			
		}
	}	
*/
	//errs() << "end\n";

	#endif //CAMP_CONTEXT_TREE_APPROACH
	return true;
}

bool CAMPCache::recurDuplicateFunction (ContextTreeNode *node, int oldIterator, Instruction* targetInst)
{

#ifdef debug_for_camp_cache
	errs() << " ========doing " << node->getUCID() << "=========\n";
#endif

	assert (node->isCallSite);
	assert ( isa<InvokeInst>(node->invokeOrCallInst) || isa<CallInst>(node->invokeOrCallInst) );
	assert (node->done==false);

	//Instruction *iInst = const_cast<Instruction *>(node->invokeOrCallInst);
	assert ( targetInst != NULL );
	CallInst *cInst = dyn_cast<CallInst>(targetInst);
	assert ( cInst != NULL );
	Function* originalFunction = cInst->getCalledFunction();
	
	if ( originalFunction == NULL ) // indirect call
	{
		node->done = true;
		return true;
	}
	if ( node->isRecursiveCallSite )
	{
		node->done = true;
		return true;
	}
	
	ValueToValueMapTy VMap;
/* Add false parameter in CloneFuction by qyt*/
	Function* duplicateFunction = CloneFunction(originalFunction, VMap, false);

	int newIterator = oldIterator;
	
	for (Function::iterator Biterator = duplicateFunction->begin(); Biterator != duplicateFunction->end(); ++Biterator){
		for (BasicBlock::iterator Iiterator = Biterator->begin(); Iiterator != Biterator->end(); ){
			Instruction *dInst = &*Iiterator;
			assert ( dInst != NULL );
			++Iiterator;

			if ( dyn_cast<CallInst>(dInst) != NULL  ){ // callinst appear
				Function *funCalledByDuplicateFunction = dyn_cast<CallInst>(dInst)->getCalledFunction();
				if ( funCalledByDuplicateFunction != NULL && !funCalledByDuplicateFunction->isDeclaration() ){ // check normal call instr
					// find same node with this callinst
	
					for	(newIterator; newIterator < pCxtTree->size() ; ){
						ContextTreeNode *newNode = pCxtTree->at(newIterator);
						//assert (newNode->done == false);
#ifdef debug_for_camp_cache
						errs() << newIterator;
#endif
						if ( newNode->isCallSite && newNode->done == false){
							Function *compareFunction =  dyn_cast<CallInst>(newNode->invokeOrCallInst)->getCalledFunction();
							if ( compareFunction == funCalledByDuplicateFunction 
									&& ( node->getUCID() == getParentUCID(newNode) )){
								//if ( !newNode->isRecursiveCallSite ){
									recurDuplicateFunction(newNode, newIterator, dInst);
									newIterator = node->getUCID();
								//}
								break;//find and dup this callinst
							}
#ifdef debug_for_camp_cache

							else{//compare is indirect call??
								errs() << "\n----------------------------------\n";
								errs() << "father function : ";
								errs() << duplicateFunction->getName() << "  Node : " << node->getUCID()  << "\n";
								errs() << "target function : ";
								errs() << funCalledByDuplicateFunction->getName() << "\n";
								errs() << "compared function : ";
								if (compareFunction != NULL)
									errs() << compareFunction->getName() << "\n";
								errs() << "compared node : " << newNode->getUCID() << "  parentUCID : "<<getParentUCID(newNode);
								errs() << "\n----------------------------------\n";
								//newNode->done = true;
							}
#endif
						}
						//else
							//newNode->done = true;
						newIterator++;
						if (newIterator == pCxtTree->size())
							assert ( 0 && " ERRRRRRRORRRRRRRR " );
					}//finding proper node is end
				
				}
			}
		}//bb
	}//ff

#ifdef debug_for_camp_cache

	errs() << "done recurDup : " << node->getUCID() <<"\n";

#endif

	duplicateFunction->setName(originalFunction->getName() + ":" + to_string(node->getUCID()));
	duplicateFunction->setLinkage(GlobalValue::InternalLinkage);
	originalFunction->getParent()->getFunctionList().push_back(duplicateFunction);

	int argSize = cInst->getNumArgOperands();
	std::vector<Value*> actuals(argSize);
	for (int inx = 0; inx < argSize; inx ++){
		Value *av = cInst->getArgOperand(inx);
		actuals[inx] = av;
	}

	CallInst *dCallInst = CallInst::Create(duplicateFunction, actuals, "", targetInst);
	targetInst->replaceAllUsesWith( dCallInst );
	targetInst->eraseFromParent();

	node->done = true;

	return true;
}

UniqueContextID CAMPCache::getParentUCID ( ContextTreeNode *node )
{
	int parentUCID = node->getUCID() - node->getLocalContextID();
	ContextTreeNode *parentNode = pCxtTree->at(parentUCID);

	if ( parentNode->isCallSite )
		return parentUCID;
	else
		return getParentUCID ( parentNode );
}

