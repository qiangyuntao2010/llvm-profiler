#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/InstIterator.h"

#include "corelab/CAMP/ContextTreeBuilder.h"
#include "corelab/CAMP/CampExectime.h"

#include <iostream>
#include <fstream>
#include <algorithm>    // std::for_each
#include <iterator>

using namespace llvm;
using namespace corelab;

extern cl::opt<bool> RemoveLoop;

//Utils
const Value *getCalledValueOfIndCall(const Instruction* indCall);
Function *getCalledFunction_aux(Instruction* indCall);

char ContextTreeBuilder::ID = 0;

//This template class is used to notify the system that a Pass is available for use, and registers it into the internal database maintained by the PassManager
static RegisterPass<ContextTreeBuilder> X("camp-context-tree-builder", "build context tree(only analyze)", false, true);

bool ContextTreeBuilder::verifyUCID(){
	std::list<UniqueContextID> ucIDs;
	for (auto node : cxtTree)
		ucIDs.push_back(node->ucID);
	//std::for_each(ucIDs.begin(), ucIDs.end(), [](UniqueContextID u){errs()<<u<<", ";});

/*
 * Because the unique id is an unique number in one program so there should not have another same one   in the list.
*  Pop the last one in the list and if we can find another same one in this list, it is incorrect, if not it is correct.
*  std::find() is a function: std::find(start_position, end_postition, element_need_to_find) return the elemet's iterator if success if not return the last element in the list.  
    */
	while(!ucIDs.empty()){
		UniqueContextID cmpID = ucIDs.back();
		ucIDs.pop_back();
		auto findIt = std::find(ucIDs.begin(), ucIDs.end(), cmpID);
		if(findIt != ucIDs.end()){ //found it!
			errs()<< "["<< *findIt << "]\n";
			return false;
		}
	}

	//cxtTree
	return true;
}

bool ContextTreeBuilder::runOnModule(Module& M) {
	assert(loopInfoOf.empty() && "ERROR: CAMP ContextTreeBuilder pass should be called once!");
	//DEBUG(errs()<<"############## runOnModule [ CAMP ContextTreeBuilder ] START ##############\n");
	module = &M;

	//Analysis Pass info caching
	pLoadNamer = &getAnalysis< LoadNamer >();

	loopInfoOf.clear();
	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
/*  isDeclaration: Return true if the primary definition of this global value is outside of the current translation unit. */
		Function &F = *fi;
		if(F.isDeclaration()) continue;
/* std::move is a function that can move an object, cast it and assignment it into rvalue */
		LoopInfo *li = new LoopInfo(std::move(getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo()));
		//*li = getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
		loopInfoOf.insert( std::pair<const Function *, LoopInfo * >(&*fi, li) );
		//loopInfoOf[fi] = &getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
	}

	RecursiveFuncAnal &recFunAnal = getAnalysis<RecursiveFuncAnal>();
	recFuncList.insert(recFunAnal.begin(), recFunAnal.end());

	IndirectCallAnal &indCallAnal = getAnalysis<IndirectCallAnal>();
	indCallAnal.getTypeBasedMatching(possibleTargetOf);

	recordLoopIDforEachLoop();

	//building procedure starts from main
	Function *mainFcn = &*std::find_if(M.begin(), M.end(), [](Function &fi){return (fi.getName() == "main" && !fi.hasLocalLinkage());});
	assert(recFuncList.find(mainFcn) == recFuncList.end() && "ERROR: main is Recursive Function. Impossible!!");
	ContextTreeNode *root = new ContextTreeNode(true, NULL, assignerUcID++);
	//root->addCallSiteInfo(mainFcn, -1); //bacause main is root, it doesnt have parent;
	root->func = mainFcn;
	root->ucID = 0; //START OF ucID(ROOT of ucID) is 0
	cxtTree.push_back(root);

	recFunAnal.printAllRecursiveFunction();
	// indCallAnal.printAllIndirectCalls();

	functionNodeTraverse(root);

	errs()<<"#of RecFun"<<recFuncList.size()<<", # of recursiveFunCalls:"<< recursiveFunCalls.size()<<"\n";

	bool successUnique = verifyUCID();
	// errs()<<" #### CAMP ContextTreeBuilder : "
	// 	  << (successUnique ? " found UNIQUE calculation " : " FAIL to find unique calculation ")
	// 	  <<" #### \n";
	// assert(successUnique);

	// printContextTree();
	
	if(callsWithNoTarget.size() != 0) printCallsWithNoTarget();
	contextTreeDumpToFile(std::string("ContextTree.data"));
	contextTreeDumpToGvfile(std::string("ContextTree.gv"));
	//DEBUG(errs()<<"[ CAMP ContextTreeBuilder ] # of Node in Tree: "<<cxtTree.size()<<",maxDepth: "<<maxDepth<<", Success?:"<<successUnique<<" ;\n");
	//DEBUG(errs()<<"[ CAMP ContextTreeBuilder ] # of CallSiteNode : "<<nCallSiteNode<<", # of LoopNode: "<< nLoopNode<<", sum:"<<nCallSiteNode+nLoopNode<<" \n");
	//DEBUG(errs()<<"############## runOnModule [ CAMP ContextTreeBuilder ] END ##############\n");
	return false;
}

bool ContextTreeBuilder::functionNodeTraverse(ContextTreeNode *callSiteNode){
	assert(callSiteNode->isCallSite && callSiteNode->func);
	Function &F = *callSiteNode->func;
	assert((!F.isDeclaration()) && "ERROR: func is declaration!!");

	//LoopInfo &li = getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
	LoopInfo &li = *loopInfoOf[callSiteNode->func];
	std::vector<Loop*> all_loops( li.begin(), li.end() );

	std::vector<BasicBlock*> blocksInCurDepth;
	for(Function::iterator bbi=F.begin(), be=F.end(); bbi!=be; ++bbi) { blocksInCurDepth.push_back(&*bbi); }

	//subtract nesting loop's basic block from "blocksInCurDepth"
	for(Loop *loop : all_loops){
		auto it = std::remove_if (blocksInCurDepth.begin(), blocksInCurDepth.end(),
			[loop](BasicBlock *bb){
				auto slbi = find (loop->block_begin(), loop->block_end(), bb);
				return slbi != loop->block_end();
			} );
		blocksInCurDepth.assign(blocksInCurDepth.begin(), it);
	}

	for(auto bbi=blocksInCurDepth.begin(), be=blocksInCurDepth.end(); bbi!=be; ++bbi){
		BasicBlock *bbPtr = *bbi;
		for(BasicBlock::iterator ii=bbPtr->begin(), ie=bbPtr->end(); ii!=ie; ++ii){
			if(isa<CallInst>(ii) || isa<InvokeInst>(ii)){
				makeNodeOfCallSite(&*ii, callSiteNode);
			}
		}
	}

	for(Loop *loop : all_loops){
		makeNodeOfLoop(loop, callSiteNode);
	}

	return false;
}

bool ContextTreeBuilder::LoopNodeTraverse(ContextTreeNode *loopNode){
	assert(!loopNode->isCallSite && loopNode->loop);
	Loop *curLoop = loopNode->loop;
	const std::vector<Loop*>& subLoops = curLoop->getSubLoops();
	unsigned numSubLoops = subLoops.size();
	const std::vector<BasicBlock*>& blocks = curLoop->getBlocks();

	//traverse all functions called in blocksInCurDepth
	for(BasicBlock* bbi : blocks){ //loop->blocks()
		if(numSubLoops != 0){
			if(std::any_of(subLoops.begin(), subLoops.end(), [bbi](Loop *loop){return loop->contains(bbi);}))
				continue;
		}
		
		for(BasicBlock::iterator ii=(bbi)->begin(), ie=(bbi)->end(); ii!=ie; ++ii){
			if(isa<CallInst>(ii) || isa<InvokeInst>(ii))
				makeNodeOfCallSite(&*ii, loopNode);
		}
	}

	// traverse all subloops
	if(numSubLoops != 0 ){
		for(Loop *subloop : subLoops){
			makeNodeOfLoop(subloop, loopNode);
		}
	}

	return false;
}

void ContextTreeBuilder::makeNodeOfCallSite(Instruction *invokeOrCallinst, ContextTreeNode *parent){
	if(isa<CallInst>(invokeOrCallinst) || isa<InvokeInst>(invokeOrCallinst)){
		Function* callee = getCalledFunction_aux(invokeOrCallinst);
		if(callee == NULL){//Indirect call
			int numTargetCandidate = possibleTargetOf[invokeOrCallinst].size();
			if(numTargetCandidate == 0){ // collect callsWithNoTarget for statistic purpose
				//filter-out exteranl func call (declaration only)
				if (const Function *tarFun = dyn_cast<Function>(getCalledValueOfIndCall(invokeOrCallinst)->stripPointerCasts())) {
					if(!tarFun->isDeclaration())
						callsWithNoTarget.push_back(invokeOrCallinst);
				}
				else
					callsWithNoTarget.push_back(invokeOrCallinst);
			}
			else if(numTargetCandidate == 1){
				//aggressively, we can think of this as if it is just an ordinary (direct) call.
				callee = possibleTargetOf[invokeOrCallinst].front();
				makeNodeOfCallSite_aux(callee, invokeOrCallinst, parent);
			}
			else{ //multiple target candidate
				//handle as if parent sequentially call every cadidate by this indirect call.

				// for debug
				// errs()<<"multiple target candidate case [";
				// for_each(possibleTargetOf[invokeOrCallinst].begin(), possibleTargetOf[invokeOrCallinst].end(), [](Function *f){errs()<<"("<<f->getName()<<"), ";});
				// errs()<<"]\n";
				
				//make sure locIdOf_callSite[invokeOrCallinst] == -1
				locIdOf_callSite[invokeOrCallinst] = -1;
				for(Function *oneOfCallees : possibleTargetOf[invokeOrCallinst]){
					ContextTreeNode *pNewNode = makeNodeOfCallSite_aux(oneOfCallees, invokeOrCallinst, parent);
					locIdOf_indCall[invokeOrCallinst].push_back( std::make_pair(oneOfCallees, pNewNode->getLocID()) );
				}
			}
		}
		else if(callee->isDeclaration())
			;//errs()<<"[Extr :"<<*invokeOrCallinst<<"]\n"; //for debug
			//externalCalls.push_back(invokeOrCallinst); //add to external function call list.	
		else{
			makeNodeOfCallSite_aux(callee, invokeOrCallinst, parent);
		}
	}
	else
		assert(0 && "ERROR:: WTF?");
	return;
}

ContextTreeNode * ContextTreeBuilder::makeNodeOfCallSite_aux(Function *callee, Instruction *invokeOrCallinst, ContextTreeNode *parent){
	assert(!callee->isDeclaration() && "WTF??");
	FuncID funId = pLoadNamer->getFunctionId(*callee);
	assert(funId != 0 && "ERROR:: funId == 0");

	//add Node
	ContextTreeNode *pNewNode = new ContextTreeNode(true, parent, assignerUcID++);
	pNewNode->addCallSiteInfo(invokeOrCallinst, callee, locIdOf_callSite);
	unsigned nDup = pNewNode->numDuplicationOnPathToRoot(pNewNode->isCallSite, pNewNode->invokeOrCallInst, -1);
	if(nDup != 1){
		errs()<<"### PRINT path to root \n";
		pNewNode->printPathToRoot();
		errs()<<"\n### PRINT path to root END\n";
		assert(0 && "ERROR: Duplication occurs !! (in CallSite)" );
	}


	cxtTree.push_back(pNewNode);
	parent->addChild(pNewNode);
	maxDepth = std::max(maxDepth, pNewNode->depth());
	//assert(maxDepth<TREE_MAX_DEPTH_LIMIT && "ERROR: Context Tree exceeds max depth limit!");
	nCallSiteNode++;
	

	if(recFuncList.find(callee) != recFuncList.end()){
		//errs()<<"[Rec CS :"<<callee->getName()<<" <"<<*invokeOrCallinst<<"> ]\n"; //for debug
		recursiveFunCalls.push_back(invokeOrCallinst);
		pNewNode->markRecursive();
		//in recursive call site node, no further traverse
	}
	else{
		//errs()<<"[CS :"<<callee->getName()<<" <"<<*invokeOrCallinst<<"> ]\n"; //for debug
		functionNodeTraverse(pNewNode);// further traverse
	}

	return pNewNode;
}

void ContextTreeBuilder::makeNodeOfLoop(Loop *loop, ContextTreeNode *parent){
	ContextTreeNode *pNewNode = new ContextTreeNode(false, parent, assignerUcID++);
	CntxID cntxID = loopIdOf[loop];
	pNewNode->addLoopInfo(loop, cntxID, locIdOf_loop);
	//errs()<<"[L :"<<*loop<<" <"<<cntxID<<"> ]\n"; //for debug
	cxtTree.push_back(pNewNode);
	parent->addChild(pNewNode);
	maxDepth = std::max(maxDepth, pNewNode->depth());
	//assert(maxDepth<TREE_MAX_DEPTH_LIMIT && "ERROR: Context Tree exceeds max depth limit!");

	unsigned nDup = pNewNode->numDuplicationOnPathToRoot(pNewNode->isCallSite, NULL, cntxID);
	assert(nDup == 1 && "ERROR: Duplication occurs !! (in Loop)");
	nLoopNode++;
	//errs()<<"LOOP md:"<<maxDepth<<", # nodes:"<<cxtTree.size()<<", name:"<<*pNewNode->loop<<"\n";
	// if(cxtTree.size() > 20000){
	// 	errs()<<"### PRINT path to root \n";
	// 	pNewNode->printPathToRoot();
	// 	errs()<<"\n### PRINT path to root END\n";
	// }
	
	LoopNodeTraverse(pNewNode);
}

void ContextTreeBuilder::recordLoopIDforEachLoop(){
	for(Module::iterator fi = module->begin(), fe = module->end(); fi != fe; ++fi) {
		Function &F = *fi;
		if(F.isDeclaration()) continue;
		FuncID functionId = pLoadNamer->getFunctionId(F);

		LoopInfo &li = *loopInfoOf[&*fi];
		std::list<Loop*> all_loops( li.begin(), li.end() );
		while(!all_loops.empty()){
			Loop* loop = all_loops.front();
			CntxID loopContextId = pLoadNamer->getLoopContextId(loop, functionId);
			loopIdOf[loop] = loopContextId;
			loopOfCntxID[loopContextId] = loop;
			//errs()<<"##[ Loop ID :<"<<loopContextId<<">"<<*loop; //for debug

			const std::vector<Loop*>& subLoops = loop->getSubLoops();
			for (auto i = subLoops.begin(); i != subLoops.end(); ++i){
				all_loops.push_back(*i);
			}
			all_loops.pop_front();
		}
	}
}

void ContextTreeBuilder::printContextTree(){
	errs()<<"[[[######### Context TREE (size:"<<cxtTree.size()<<") Print #########]]]\n";
	int cnt = 1;
	for (auto ti = cxtTree.begin(); ti != cxtTree.end(); ++ti){
		ContextTreeNode &node = **ti;
		errs()<<cnt++<<":[ucID:"<<node.ucID<<", locID:"<<node.locID<<"](";
		if(node.isCallSite){
			assert(node.invokeOrCallInst);
			errs()<<(node.isRecursiveCallSite?"RecCS<":"CS<");
			if(node.ucID == 0)// for main
				errs()<<"main"<<">:"<<node.func->getName();
			else
				errs()<<*node.invokeOrCallInst<<">:"<<node.func->getName();
		}
		else
			errs()<<"L<"<<node.cntxID<<">:"<<*node.loop;
		errs()<<")\n";
	}
	errs()<<"##########@@#########\n";
	//drawing Tree in Level order?
	errs()<<"######local ID Map (size: "<<locIdOf_callSite.size() + locIdOf_loop.size()<<")#########\n";
	for (auto it = locIdOf_callSite.begin(); it != locIdOf_callSite.end(); ++it){
		errs()<<"[ inst: "<<*it->first<<" ("<<it->first<<")"<<", locID: ";
		if(it->second == (LocalContextID)(-1)){
			errs()<<"<";
			std::for_each(locIdOf_indCall[it->first].begin(), locIdOf_indCall[it->first].end(), [](std::pair<Function *, LocalContextID> p){
				errs()<<"("<<p.first->getName()<<":"<<p.second<<"), ";
			});
			errs()<<"> ]\n";
		errs()<<"\n";
		}
		else
			errs()<<it->second<<" ]\n";
	}

	for (auto it = locIdOf_loop.begin(); it != locIdOf_loop.end(); ++it){
		errs()<<"[ CntxID: "<<it->first<<", locID: "<<it->second<<" ]\n";
	}

	errs()<<"######loop ID Map (size: "<<loopIdOf.size()<<")#########\n";
	for (auto it = loopIdOf.begin(); it != loopIdOf.end(); ++it){
		errs()<<"[ loop: "<<*it->first<<", loopID: "<<it->second<<" ]\n";
	}

	errs()<<"[[[######### Context TREE Print END #####]]]\n";
}

void ContextTreeBuilder::printCallsWithNoTarget(){
	errs()<<"[[[ In ContextTreeBuilder:: CallsWithNoTarget (size:"<<callsWithNoTarget.size()<<") Print ..]]]\n";
	for(auto indCall : callsWithNoTarget){
		errs()<<" ["<<*indCall<<"]\n";
	}
	errs()<<"[[[ In ContextTreeBuilder:: CallsWithNoTarget Print .. END]]]\n";
}

void ContextTreeBuilder::contextTreeDumpToFile(std::string path){
	// // ########## ostream style
	std::ofstream outfile(path, std::ios::out | std::ofstream::binary);

	if(outfile.is_open()){
		outfile<<"$$$$$ CONTEXT TREE IN PREORDER $$$$$\n";
		outfile<<cxtTree.size()<<"\n";
		//std::copy (cxtTree.begin(), cxtTree.end(),std::ostream_iterator<ContextTreeNode>(outfile, "\n"));
		for ( auto it = cxtTree.begin(); it != cxtTree.end(); ++it ){
			ContextTreeNode *node = *it;
			if(node->isCallSite){
				outfile<<node->getUCID()<<" "<<node->getLocID()
				       <<" CS "<<(node->isRecursiveCallSite?'R':'N')<<" "<<node->getCallee()->getName().str();
				       //node->getCallInst()->print(outfile);
			}
			else{
				outfile<<node->getUCID()<<" "<<node->getLocID()
				       <<" L "<<" "<<node->getCntxIDforLoop()<<" "<<node->getLoop()->getHeader()->getName().str();
			}
			outfile<<"\n";
		}
		outfile<<"$$$$$ CONTEXT TREE END $$$$$\n";
	}
	outfile.close();

	// std::error_code EC (errno,std::generic_category());
	// raw_fd_ostream fout(path, EC, (sys::fs::OpenFlags)8/*RW*/);
	// fout<<"$$$$$ CONTEXT TREE IN PREORDER $$$$$\n";
	// //std::copy (cxtTree.begin(), cxtTree.end(),std::ostream_iterator<ContextTreeNode>(fout, "\n"));
	// for ( auto it = cxtTree.begin(); it != cxtTree.end(); ++it ){
	// 	ContextTreeNode *node = *it;
	// 	if(node->isCallSite){
	// 		fout<<node->getUCID()<<" "<<node->getLocID()
	// 		     <<" CS "<<(node->isRecursiveCallSite?'R':'N')<<" "<<node->getCallee()->getName().str()<<" ";
	// 		if(node->getUCID() != 0)
	// 			fout<<*node->getCallInst();
	// 		       //node->getCallInst()->print(fout);
	// 	}
	// 	else{
	// 		fout<<node->getUCID()<<" "<<node->getLocID()
	// 		       <<" L "<<" "<<node->getCntxIDforLoop()<<" "<<node->getLoop()->getHeader()->getName().str();
	// 	}
	// 	fout<<"\n";
	// }
	// fout<<"$$$$$ CONTEXT TREE END $$$$$\n";
}

// ####################

void ContextTreeBuilder::contextTreeDumpToGvfile(std::string path){
        // // ########## ostream style
        std::ofstream outfile(path, std::ios::out | std::ofstream::binary);
					
					outfile<<"digraph G {";
					//outfile<<cxtTree.size()<<"\n";
					//std::copy (cxtTree.begin(), cxtTree.end(),std::ostream_iterator<ContextTreeNode>(outfile, "\n"));
					for ( auto it = cxtTree.begin(); it != cxtTree.end(); ++it ){
						ContextTreeNode *node = *it;
					
						if(RemoveLoop){
							if ( node->isCallSite && node->getUCID()!=0 ){
								outfile<<getParentUCID(node)<<" -> "<<node->getUCID()<<";";
								outfile<<"\n";
							}
							else
								outfile<<"\n";
						}
						else{
							if(node->getUCID() != 0)
								outfile<<(node->getUCID())-(node->getLocID())<<" -> "<<node->getUCID()<<";"; 
							outfile<<"\n";
						}
					}

					if(RemoveLoop){
						outfile<<"}";
						outfile.close();
						std::ofstream supportfile("contextLabel.gv", std::ios::out | std::ofstream::binary);
						for ( auto it = cxtTree.begin(); it != cxtTree.end(); ++it ){
							ContextTreeNode *node = *it;
							if(RemoveLoop){
								if(node->isCallSite){
									supportfile<<node->getUCID()<<"\[shape=box,label=\""<<node->getUCID()<<"\\n"<<node->getCallee()->getName().str()<<"\"];\n";        
								}
							}
						}
						supportfile<<"}";
						supportfile.close();
					}
					else{
						for ( auto it = cxtTree.begin(); it != cxtTree.end(); ++it ){
							ContextTreeNode *node = *it;
							if(node->isCallSite){
								outfile<<node->getUCID()<<"\[shape=box,label=\""<<node->getUCID()<<"\\n"<<node->getCallee()->getName().str()<<"\"];\n";        
							}
						}
						outfile<<"}";
						outfile.close();
					}
}

void ContextTree::addCallSiteInfo(const Instruction *invokeOrCallInst_, Function *f, LocIDMapForCallSite &locIdOf_callSite){
	assert(isCallSite);
	assert(parent);
	invokeOrCallInst = invokeOrCallInst_;
	func = f;
	auto found = locIdOf_callSite.find(invokeOrCallInst);
	if(found != locIdOf_callSite.end()){//found it!
		if(found->second != (LocalContextID)(-1)) // if this is not multiple target indirect call case
			//assert(found->second == locID && "ERROR: LocalContextID mismatch (call site case)!!!!");
			if(found->second != locID){
				errs()<<"I'm dying ..\n";
				errs()<<"Inst: " <<*invokeOrCallInst<<", locID:"<<locID<<", found locID: "<<found->second<<", ucID:"<<ucID<<"\n";
				assert(0);
			}
	}
	else{
		locIdOf_callSite[invokeOrCallInst] = locID;
	}

}
void ContextTree::addLoopInfo(Loop *l, CntxID cntxID_, LocIDMapForLoop &locIdOf_loop){
	assert(!isCallSite);
	assert(parent);
	loop = l;
	cntxID = cntxID_;
	auto found = locIdOf_loop.find(cntxID);
	if(found != locIdOf_loop.end()){//found it!
		assert(found->second == locID && "ERROR: LocalContextID mismatch(Loop case)!!!!");
	}
	else{
		locIdOf_loop[cntxID] = locID;
	}
}

unsigned ContextTree::depth(){
	if(parent == NULL)
		return 0;
	else
		return 1 + parent->depth();
}

unsigned ContextTree::numDuplicationOnPathToRoot(bool isCallSiteCmp, const Instruction *instCmp, CntxID cntxIDCmp){
	if(parent == NULL)
		return 0;
	if(isCallSiteCmp != isCallSite)
		return parent->numDuplicationOnPathToRoot(isCallSiteCmp, instCmp, cntxIDCmp);
	if(isCallSiteCmp == true && isCallSite == true ){
		if(instCmp == invokeOrCallInst)
			return 1 + parent->numDuplicationOnPathToRoot(isCallSiteCmp, instCmp, cntxIDCmp);
		else
			return parent->numDuplicationOnPathToRoot(isCallSiteCmp, instCmp, cntxIDCmp);
	}
	else if(isCallSiteCmp == false && isCallSite == false ){
		if(cntxIDCmp == cntxID)
			return 1 + parent->numDuplicationOnPathToRoot(isCallSiteCmp, instCmp, cntxIDCmp);
		else
			return parent->numDuplicationOnPathToRoot(isCallSiteCmp, instCmp, cntxIDCmp);	
	}
	else
		assert(0&&"WTF????");
}

void ContextTree::printPathToRoot(){
	if(parent == NULL)
		return;
	if(isCallSite)
		errs()<<" ["<<*invokeOrCallInst<<"("<<func->getName()<<")]\n";
	else
		errs()<<" ["<<*loop<<"]\n";
	parent->printPathToRoot();\
}













// //for debug
// errs()<<"############## LoopID cmpare ##############\n";
// for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
// 	Function &F = *fi;
// 	if(F.isDeclaration()) continue;
// 	LoopInfo &li = *loopInfoOf[fi];
// 	std::list<Loop*> all_loops( li.begin(), li.end() );
// 	while(!all_loops.empty()){
// 		Loop* loop = all_loops.front();
// 		//errs()<<"$$[ Loop ID :<"<<loopIdOf[loop]<<">"<<*loop; //for debug
// 		const std::vector<Loop*>& subLoops = loop->getSubLoops();
// 		for (auto i = subLoops.begin(); i != subLoops.end(); ++i){
// 			all_loops.push_back(*i);
// 		}
// 		all_loops.pop_front();
// 	}
// }
// errs()<<"############## LoopID cmpare END ##############\n";
// //for debug END


//TODO:: where to put this useful function?
/* dyn_cast is a function to detect the type if it has the wrong type it will return null */
const Value *getCalledValueOfIndCall(const Instruction* indCall){
	if(const CallInst *callInst = dyn_cast<CallInst>(indCall)){
		return callInst->getCalledValue();
	}
	else if(const InvokeInst *invokeInst = dyn_cast<InvokeInst>(indCall)){
		return invokeInst->getCalledValue();
	}
	else
		assert(0 && "WTF??");
}

//TODO:: where to put this useful function?
Function *getCalledFunction_aux(Instruction* indCall){
	if(CallInst *callInst = dyn_cast<CallInst>(indCall)){
		return callInst->getCalledFunction();
	}
	else if(InvokeInst *invokeInst = dyn_cast<InvokeInst>(indCall)){
		return invokeInst->getCalledFunction();
	}
	else
		assert(0 && "WTF??");
}
