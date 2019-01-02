#include "corelab/AliasAnalysis/LoopTraverse.hpp"
#include <algorithm>
#include <climits>

#ifndef DEBUG_TYPE
  #define DEBUG_TYPE "loop-traverse"
#endif

using namespace llvm;
using namespace corelab;

char LoopTraverse::ID = 0;
static RegisterPass<LoopTraverse> X("loop-traverse", "loop traverse.", false, true);

//future plan to fix
//1. reduce every getAnalysis<> call to minimum
//2. find first point of disappearing. 

Function *getCalledFunction_aux(Instruction* indCall);

bool LoopTraverse::runOnModule(Module &M) {
	DEBUG(errs() << "\nSTART [LoopTraverse::runOnModule] 	#######\n");
	module = &M;

	assert(( maxDepthOf.empty() && externalCalls.empty()) && "runOnModule must be called twice");//maxDepthOf.clear();
	//externalCalls.clear();
	//indirectCalls.clear();
	//maxDepthOf.clear();
	loopInfoOf.clear();

	IndirectCallAnal &indCallAnal = getAnalysis<IndirectCallAnal>();
	indCallAnal.getTypeBasedMatching(possibleTargetOf);

	RecursiveFuncAnal &recFunAnal = getAnalysis<RecursiveFuncAnal>();

	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		Function &F = *fi;
		if(F.isDeclaration()) continue;
		LoopInfo *li = new LoopInfo(std::move(getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo()));
		//*li = getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
		loopInfoOf.insert( std::pair<const Function *, LoopInfo * >(&F, li) );
		//loopInfoOf[fi] = &getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
	}

	recFuncList.insert(recFunAnal.begin(), recFunAnal.end());
	
	// DEBUGGING CODE for checking if getAnalysis Data for Function Pass is stored properly.
	// for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
	// 	Function &F = *fi;
	// 	if(F.isDeclaration()) continue;
	// 	std::vector<Loop*> all_loops( loopInfoOf[fi]->begin(), loopInfoOf[fi]->end() );
	// 	errs()<<" ======= [Loop list of <"<< fi->getName() << "> ]  =======\n";
	// 	std::for_each(all_loops.begin(), all_loops.end(), [](Loop *l){l->dump();});
	// 	errs()<<" ======= [Loop list of <"<< fi->getName() << "> ] END ======= \n\n";
	// }	
	
	searchMaxLoopDepth();
	//printMaxLoopDepthOfFunctions();
	
	DEBUG(errs() << "\nEND [LoopTraverse::runOnModule] 	#######\n");
	return false;
}

unsigned LoopTraverse::getLoopDepth(const Loop *loop) {

	unsigned maxDepth = 0;
	const std::vector<Loop*>& subLoops = loop->getSubLoops();
	unsigned size_debug = subLoops.size();
	const std::vector<BasicBlock*>& blocks = loop->getBlocks();

	//traverse all functions called in blocksInCurDepth
	//for(std::vector<BasicBlock*>::const_iterator bbi = loop->block_begin(), bbe = loop->block_end(); bbi != bbe ; ++bbi){
	for(BasicBlock* bbi : blocks){ //loop->blocks()
		if(size_debug != 0){
			assert(subLoops.size() == size_debug && "ERROR: subLoops or loop changed");
			if(std::any_of(subLoops.begin(), subLoops.end(), [bbi](Loop *loop){return loop->contains(bbi);})){
				continue;
			}
		}
		assert(isa<BasicBlock>(bbi));
		
		for(BasicBlock::iterator ii=(bbi)->begin(), ie=(bbi)->end(); ii!=ie; ++ii){
		//for (Instruction &i : bbi->getInstList()){
			assert(isa<Instruction>(ii));
			Instruction *instPtr = &*ii;//&i;
			if(isa<CallInst>(ii) || isa<InvokeInst>(ii)){
				maxDepth = std::max(testAndTraverse(&*ii), maxDepth);
			}
		}
	}

	//traverse all subloops
	if(size_debug != 0 ){
		for(Loop *subloop : subLoops){
			maxDepth = std::max(getLoopDepth(subloop), maxDepth);
		}
	}
	//errs()<<"[ returning " << maxDepth <<", name:"<<" ] \n";
	//errs()<<"[ returning " << maxDepth <<", name:"<< *loop <<" ] \n";
	return maxDepth+1;
}

unsigned LoopTraverse::testAndTraverse(Instruction *inst){
	assert(isa<CallInst>(inst) || isa<InvokeInst>(inst));
	unsigned maxDepth = 0;

	Function *callee = getCalledFunction_aux(inst);
	if(!callee){
		unsigned numTargetCandidate = possibleTargetOf[inst].size();
		if(numTargetCandidate == 0){
			DEBUG(errs()<<"indirectCallsWithNoTarget.push_back(something we cannot know in compile time);\n");
			indirectCallsWithNoTarget.push_back(inst);	
		}
		else{
			for(Function *oneOfCallee : possibleTargetOf[inst]){
				if( oneOfCallee->isDeclaration() == false && recFuncList.find(oneOfCallee) == recFuncList.end() )
					maxDepth = std::max(getLoopDepth_aux(oneOfCallee), maxDepth);
			}
		}
	}
	else if (callee->isDeclaration()){ //external function definition
    	DEBUG(errs()<<"externalCalls.push_back("<<callee->getName()<<");\n");
		externalCalls.push_back(inst);
	}
	else if (recFuncList.find(callee) != recFuncList.end()){
		DEBUG(errs()<<"recursiveFunCalls.push_back("<<callee->getName()<<");\n");
		recursiveFunCalls.push_back(inst);
	}
    else 
    	maxDepth = std::max(getLoopDepth_aux(callee), maxDepth);

    return maxDepth;
}

unsigned LoopTraverse::getLoopDepth_aux(Function *func) {
	auto it = maxDepthOf.find(func);
	assert(recFuncList.find(func) == recFuncList.end() && "ERROR: try to know recursive function's max loop depth!!");
	if(it != maxDepthOf.end()){
		DEBUG(errs()<<"&& GETCHA! : "<<func->getName()<<", MaxDepth:"<<it->second<< " &&\n");
		return (it->second);
	}

	unsigned maxDepth = 0;
	Function &F = *func;
	assert((!F.isDeclaration()) && "ERROR: func is declaration!!");

	//LoopInfo &li = getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
	LoopInfo &li = *loopInfoOf[func];
	std::vector<Loop*> all_loops( li.begin(), li.end() );
	for(Loop *loop : all_loops){
		maxDepth = std::max(getLoopDepth(loop), maxDepth);
	}

	std::vector<BasicBlock*> blocksInCurDepth;
	for(Function::iterator bbi=F.begin(), be=F.end(); bbi!=be; ++bbi) { blocksInCurDepth.push_back(&*bbi); }

	//subtract nesting loop's basic block from "blocksInCurDepth"
	for(Loop *loop : all_loops){
		//errs()<<"&&& DUBUG "<< func->getName()<<" &&&\n";
		auto it = std::remove_if (blocksInCurDepth.begin(), blocksInCurDepth.end(),
			[loop](BasicBlock *bb){
				auto slbi = find (loop->block_begin(), loop->block_end(), bb);
				return slbi != loop->block_end();
			} );
		blocksInCurDepth.assign(blocksInCurDepth.begin(), it);
	}

	for(auto bbi=blocksInCurDepth.begin(), be=blocksInCurDepth.end(); bbi!=be; ++bbi){
		BasicBlock *bbPtr = *bbi;

		// if(std::any_of(all_loops.begin(), all_loops.end(), [bbi](Loop *loop){return loop->contains(bbi);}))
		// 	continue;
		for(BasicBlock::iterator ii=bbPtr->begin(), ie=bbPtr->end(); ii!=ie; ++ii){
			if(isa<CallInst>(ii) || isa<InvokeInst>(ii))
				maxDepth = std::max(testAndTraverse(&*ii), maxDepth);
		}
	}

	DEBUG(errs()<<"&& SAVING! : "<<func->getName()<<", MaxDepth:"<<maxDepth<<" &&\n");
	maxDepthOf[func] = maxDepth;
	return maxDepth;
}


void LoopTraverse::searchMaxLoopDepth(){
	Module &M = *module;
	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		Function &F = *fi;
		if (F.isDeclaration()) continue;
		if(recFuncList.find(&*fi) != recFuncList.end()) //skip analyzing recursive call
			maxDepthOf[&*fi] = INT_MAX;
		else{
			getLoopDepth_aux(&*fi); // doesn't need to take return value because "maxDepthOf" will take it as side-effect
		}
	}
}

unsigned LoopTraverse::getMaxLoopDepth(){
	Module &M = *module;
	unsigned maxDepth = 0;
	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi){
		unsigned d = (recFuncList.find(&*fi) != recFuncList.end()) ? 0 : maxDepthOf[&*fi];//shouldn't compare with recursive function.
		maxDepth = std::max(d , maxDepth);
	}
	return maxDepth;
}

void LoopTraverse::printMaxLoopDepthOfFunctions()
{
	Module &M = *module;
	errs()<<"[==== List of All MaxDepth of functions ============]\n";
	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi){
		if(recFuncList.find(&*fi) != recFuncList.end())
			errs()<<" ["<<fi->getName()<<" : Recursive Function ],\n";
		else
			errs()<<" ["<<fi->getName()<<" : "<< maxDepthOf[&*fi] <<"],\n";
	}
	errs()<<"[==== List of All MaxDepth of functions END ========]\n";
}


//TODO:: where to put this useful function?
// Function *getCalledFunction_aux(Instruction* indCall){
// 	if(CallInst *callInst = dyn_cast<CallInst>(indCall)){
// 		return callInst->getCalledFunction();
// 	}
// 	else if(InvokeInst *invokeInst = dyn_cast<InvokeInst>(indCall)){
// 		return invokeInst->getCalledFunction();
// 	}
// 	else
// 		assert(0 && "WTF??");
// }



// unsigned LoopTraverse::getLoopDepth(Loop *loop) {
// 	//unsigned depth = loop->getLoopDepth();
// 	unsigned maxDepth = 0;
// 	const std::vector<Loop*>& subLoops = loop->getSubLoops();
// 	std::vector<BasicBlock*> blocksInCurDepth(loop->block_begin(), loop->block_end());
	
// 	//subtract nesting loop's basic block from "blocksInCurDepth"
// 	for(Loop *subloop : subLoops){
// 		auto it = std::remove_if (blocksInCurDepth.begin(), blocksInCurDepth.end(),
// 			[subloop](BasicBlock *bb){
// 				auto slbi = find (subloop->block_begin(), subloop->block_end(), bb);
// 				return slbi != subloop->block_end();
// 			} );
// 		blocksInCurDepth.assign(blocksInCurDepth.begin(), it);
// 	}

// 	//traverse all subloops
// 	for(Loop *subloop : subLoops){
// 		errs()<<"!!! subloop:"<< *subloop <<" !!! \n";
// 		maxDepth = std::max(getLoopDepth(subloop), maxDepth);
// 	}

// 	//traverse all functions called in blocksInCurDepth
// 	for(BasicBlock *curDepthBB : blocksInCurDepth){
// 		BasicBlock &BB = *curDepthBB;
// 		for(BasicBlock::iterator ii=BB.begin(), ie=BB.end(); ii!=ie; ++ii)
// 			maxDepth = std::max(testAndTraverse(ii), maxDepth);
// 	}

// 	//errs()<<"[ returning " << maxDepth <<", name:"<<" ] \n";
// 	//errs()<<"[ returning " << maxDepth <<", name:"<< *loop <<" ] \n";
// 	return maxDepth+1;
// }