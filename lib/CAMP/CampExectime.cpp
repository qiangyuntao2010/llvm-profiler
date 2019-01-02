#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/CallSite.h"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/IRBuilder.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InstIterator.h"

#include "corelab/Utilities/GlobalCtors.h"
#include "corelab/Metadata/Metadata.h"
#include "corelab/Metadata/LoadNamer.h"
#include "corelab/CAMP/CampExectime.h"

#include <iostream>
#include <vector>
#include <list>
#include <cstdlib>

//#define CAMP_INSTALLER_DEBUG
//#define CAMP_CONTEXT_STACK_APPROACH
#define CAMP_CONTEXT_TREE_APPROACH

using namespace corelab;

char CAMPExectime::ID = 0;
static RegisterPass<CAMPExectime> X("camp-exectime", "Profiling_Context_aware_exectime_data", false, false);


cl::opt<bool> RemoveLoop("remove-loop", cl::init(false), cl::Hidden,
		cl::desc("remove loop node when writing exectime data"));
//Utils
const Value *getCalledValueOfIndCall(const Instruction* indCall);
Instruction *getProloguePosition(Instruction *inst);
Function *getCalledFunction_aux(Instruction* indCall);


void CAMPExectime::setFunctions(Module &M)
{
	LLVMContext &Context = M.getContext();

	campExecInitialize = M.getOrInsertFunction(
			"campExecInitialize",
			Type::getVoidTy(Context),
			Type::getInt64Ty(Context),
			Type::getInt64Ty(Context),
			Type::getInt64Ty(Context),
			Type::getInt64Ty(Context),
			Type::getInt64Ty(Context),
			(Type*)0);

	campExecFinalize = M.getOrInsertFunction(
			"campExecFinalize",
			Type::getVoidTy(Context),
			Type::getInt1Ty(Context),
			(Type*)0);
	
	campExecCallSiteBegin = M.getOrInsertFunction(
			"campExecCallSiteBegin",
			Type::getVoidTy(Context),
			Type::getInt16Ty(Context),
			(Type*)0);

	campExecCallSiteEnd = M.getOrInsertFunction(
			"campExecCallSiteEnd",
			Type::getVoidTy(Context),
			Type::getInt16Ty(Context),
			(Type*)0);

	campExecLoopBegin = M.getOrInsertFunction(
			"campExecLoopBegin",
			Type::getVoidTy(Context),
			Type::getInt16Ty(Context),
			(Type*)0);

	campExecLoopNext = M.getOrInsertFunction( 
			"campExecLoopNext",
			Type::getVoidTy(Context),
			(Type*)0);

	campExecLoopEnd = M.getOrInsertFunction(
			"campExecLoopEnd",
			Type::getVoidTy(Context),
			Type::getInt16Ty(Context),
			(Type*)0);
	
	campExecDisableCtxtChange = M.getOrInsertFunction(
			"campExecDisableCtxtChange",
			Type::getVoidTy(Context),
			(Type*)0);

	campExecEnableCtxtChange = M.getOrInsertFunction(
			"campExecEnableCtxtChange",
			Type::getVoidTy(Context),
			(Type*)0);

	return;
}

void CAMPExectime::setIniFini(Module& M)
{
	if(RemoveLoop)
		cout <<"suc in exec\n";

	LLVMContext &Context = M.getContext();
	LoadNamer &loadNamer = getAnalysis< LoadNamer >();
	uint64_t maxLoopDepth = (uint64_t)getAnalysis< LoopTraverse >().getMaxLoopDepth();
	std::vector<Type*> formals(0);
	std::vector<Value*> actuals(0);
	FunctionType *voidFcnVoidType = FunctionType::get(Type::getVoidTy(Context), formals, false);

	/* initialize */
	Function *initForCtr = Function::Create( 
			voidFcnVoidType, GlobalValue::InternalLinkage, "__constructor__", &M); 
	BasicBlock *entry = BasicBlock::Create(Context,"entry", initForCtr); 
	BasicBlock *initBB = BasicBlock::Create(Context, "init", initForCtr); 
	actuals.resize(5);

	Value *loadCnt = ConstantInt::get(Type::getInt64Ty(Context), loadNamer.numLoads);
	Value *storeCnt = ConstantInt::get(Type::getInt64Ty(Context), loadNamer.numStores);
	Value *callCnt = ConstantInt::get(Type::getInt64Ty(Context), loadNamer.numCalls);
	Value *loopCnt = ConstantInt::get(Type::getInt64Ty(Context), loadNamer.numLoops);
	Value *maxLoopDepthVal = ConstantInt::get(Type::getInt64Ty(Context), maxLoopDepth);
	actuals[0] = loadCnt;
	actuals[1] = storeCnt;
	actuals[2] = callCnt;
	actuals[3] = loopCnt;
	actuals[4] = maxLoopDepthVal;
	
	CallInst::Create(campExecInitialize, actuals, "", entry); 
	BranchInst::Create(initBB, entry); 
	ReturnInst::Create(Context, 0, initBB);
	callBeforeMain(initForCtr);
	
	/* finalize */
	Function *finiForDtr = Function::Create(voidFcnVoidType, GlobalValue::InternalLinkage, "__destructor__",&M);
	BasicBlock *finiBB = BasicBlock::Create(Context, "entry", finiForDtr);
	BasicBlock *fini = BasicBlock::Create(Context, "fini", finiForDtr);
	
	actuals.resize(1);
	Type *boolean = Type::getInt1Ty(Context);
	Value *removeLoop = RemoveLoop ? ConstantInt::getTrue(Context) : ConstantInt::getFalse(Context);
	actuals[0] = removeLoop;

	CallInst::Create(campExecFinalize, actuals, "", fini);
	BranchInst::Create(fini, finiBB);
	ReturnInst::Create(Context, 0, fini);
	callAfterMain(finiForDtr);
}

bool CAMPExectime::runOnModule(Module& M) {
	assert((externalCalls.empty() && indirectCalls.empty())&&"ERROR:: CAMPInstaller::runOnModule runs twice?");
	

	setFunctions(M);
	pLoadNamer = &getAnalysis< LoadNamer >();
	LoadNamer &loadNamer = *pLoadNamer;
	module = &M;

	DEBUG(errs()<<"############## runOnModule [CAMP] START ##############\n");
	
	#ifdef CAMP_CONTEXT_TREE_APPROACH

	cxtTreeBuilder = &getAnalysis< ContextTreeBuilder >();


	pCxtTree = cxtTreeBuilder->getContextTree();
	locIdOf_callSite = cxtTreeBuilder->getLocIDMapForCallSite();
	locIdOf_indCall = cxtTreeBuilder->getLocIDMapForIndirectCalls();
	locIdOf_loop = cxtTreeBuilder->getLocIDMapForLoop();
	ContextTreeBuilder::LoopIdOf *loopIdOf = cxtTreeBuilder->getLoopIDMap();
	ContextTreeBuilder::LoopOfCntxID *loopOfLoopID = cxtTreeBuilder->getLoopMapOfCntxID();


	//first, traverse locIdOf_indCall
	for(std::pair<CntxID, const Loop *> loopID : *loopOfLoopID){
		Value *locIDVal = ConstantInt::get(Type::getInt16Ty(M.getContext()), (*locIdOf_loop)[loopID.first]);
		addProfilingCodeForLoop(const_cast<Loop *>(loopID.second), locIDVal);
	}



	for( std::pair<const Instruction *, std::vector<std::pair<Function *, LocalContextID>>> &indCall : *locIdOf_indCall){
		Value *locIDVal = addTargetComparisonCodeForIndCall(indCall.first, indCall.second);
		addProfilingCodeForCallSite(const_cast<Instruction *>(indCall.first), locIDVal);
	}


	for( std::pair<const Instruction *, LocalContextID> &normalCallSite : *locIdOf_callSite ){
		if (normalCallSite.second != (LocalContextID)(-1) ){
		Value *locIDVal = ConstantInt::get(Type::getInt16Ty(M.getContext()), normalCallSite.second);
		addProfilingCodeForCallSite(const_cast<Instruction *>(normalCallSite.first), locIDVal);
		}
	}
	
	
	#endif //CAMP_CONTEXT_TREE_APPROACH

	DEBUG(errs()<<"############## runOnModule [CAMP] END ##############\n");
	setIniFini(M);

	return false;
}

void CAMPExectime::addProfilingCodeForLoop(Loop *L, Value *locIDVal){
	// get the pre-header, header and exitblocks
	BasicBlock *header = L->getHeader();
	BasicBlock *preHeader = L->getLoopPreheader();
	SmallVector<BasicBlock*, 1> exitingBlocks;
	L->getExitingBlocks(exitingBlocks);

	
	// set loopID as an argument
	std::vector<Value*> args(1); 

	// call beginLoop at the loop preheader
	// it will push the conext
	args.resize(1);
	args[0] = locIDVal; 
	assert(!preHeader->empty() && "ERROR: preheader of loop doesnt have TerminatorInst");
	CallInst::Create(campExecLoopBegin, args, "", preHeader->getTerminator());


	// call endLoop at the exit blocks
	// it will pop the context
	int edgenum = 0;
	char edgeId[50];
	for (unsigned i = 0; i < exitingBlocks.size(); ++i)
	{
		BasicBlock *exitingBB = exitingBlocks[i];
		TerminatorInst *exitingTerm = exitingBB->getTerminator();
		unsigned int exitNum = exitingTerm->getNumSuccessors();
		unsigned int realExit = 0;
		for (unsigned int exit_i = 0; exit_i < exitNum; ++exit_i)
		{
			bool find = true;
			for (Loop::block_iterator bi = L->block_begin(), 
					be = L->block_end(); bi != be; ++bi)
			{
				BasicBlock *compare = (*bi);
				BasicBlock *target = exitingTerm->getSuccessor(exit_i);
				if (target == compare)
					find = false;
			}
			if(find)
			{
				++realExit;
				sprintf(edgeId, "edge%d", ++edgenum);

				BasicBlock *exitBB = exitingTerm->getSuccessor(exit_i);
				if(exitBB->isLandingPad())
				{
					bool isDominatedByLoop = std::all_of(pred_begin(exitBB), pred_end(exitBB), [L](BasicBlock *bb){return L->isLoopExiting(bb);});
					assert(isDominatedByLoop && "ERROR:: exitBB doesn't dominated by loop!");
					args.resize(1);
					args[0] = locIDVal;	
					Instruction *inst = exitBB->getFirstNonPHI();
					assert(exitBB->getLandingPadInst() == inst && "ERROR:: FirstNonPHI of LandingPad BB is not LandingPadinst!");
					assert(inst->getNextNode() && "ERROR:: next inst of LandingPadinst doesnt exists.");
					CallInst::Create(campExecLoopEnd, args, "", inst->getNextNode());
				}
				else
				{
					BasicBlock *edgeBB = BasicBlock::Create(module->getContext(), edgeId,exitingBB->getParent());
					exitingTerm->setSuccessor(exit_i, edgeBB);
					BranchInst::Create(exitBB, edgeBB);
				
					for(BasicBlock::iterator ii = exitBB->begin(), 
							ie = exitBB->end(); ii != ie; ++ii) {
						Instruction *inst = &(*ii);
						if (inst->getOpcode()==Instruction::PHI) {
							PHINode *phiNode = (PHINode *)inst;
							for(unsigned int bi = 0; bi < phiNode->getNumIncomingValues();
									++bi) {
								if(phiNode->getIncomingBlock(bi) == exitingBB)
								{
									phiNode->setIncomingBlock(bi, edgeBB);
									break;
								}
							}
						}
					}
					args.resize(1);
					args[0] = locIDVal;	
					Instruction *inst = edgeBB->getFirstNonPHI();
					CallInst::Create(campExecLoopEnd, args, "", inst);
				}// if exitBB is not LandingPad
			}
		}
	}
	// call beginIteration at the loop header
	// it will increase the iteration count of the loop
	args.resize(0);
	CallInst::Create(campExecLoopNext, args, "", header->getFirstNonPHI()); 
}

Value *CAMPExectime::addTargetComparisonCodeForIndCall(const Instruction *invokeOrCallInst, std::vector<std::pair<Function *, LocalContextID>> &targetLocIDs){
	assert(isa<InvokeInst>(invokeOrCallInst)||isa<CallInst>(invokeOrCallInst));
	assert((*locIdOf_callSite)[invokeOrCallInst] == (LocalContextID)(-1));
	/*
	// FIXME
	if((*locIdOf_callSite)[invokeOrCallInst] == (LocalContextID)(-1)) {
			cout << "IndCall ctxID is -1\n";
			invokeOrCallInst->dump();
	}
	*/
	LLVMContext &llvmContext = invokeOrCallInst->getModule()->getContext();
	const DataLayout &dataLayout = module->getDataLayout();
	Instruction *pInvokeOrCallInst = const_cast<Instruction *>(invokeOrCallInst);

	AllocaInst *locIDAddr = new AllocaInst(Type::getInt16Ty(llvmContext), "locID", getProloguePosition(const_cast<Instruction *>(invokeOrCallInst)));
	locIDAddr->setAlignment(sizeof(LocalContextID));

	BasicBlock* jumpHereAfterAssign =pInvokeOrCallInst->getParent()->splitBasicBlock(pInvokeOrCallInst, "AfterAssign."+locIDAddr->getName());

	TerminatorInst* insertCmpBeforehere = jumpHereAfterAssign->getSinglePredecessor()->getTerminator();
	pInvokeOrCallInst = jumpHereAfterAssign->getFirstNonPHI();

	Value *locIDVal = new LoadInst(locIDAddr, "Loaded."+locIDAddr->getName(), pInvokeOrCallInst);

	assert((locIDAddr && jumpHereAfterAssign && insertCmpBeforehere && locIDVal) && "ERROR: locIDAddrOf Map Error!!");

	//compare with this
	const Value *calledVal = getCalledValueOfIndCall(invokeOrCallInst);//same definition in ContextTreeBuilder.cpp

	//this loop will iterate by number of target candidate and make compare code(icmp, branch. basicblock)
	for(std::pair<Function *, LocalContextID> &targetLocID : targetLocIDs){
		Function *callee = targetLocID.first;
		Value *constLocID = ConstantInt::get(Type::getInt16Ty(llvmContext), targetLocID.second);

		//Amazingly, We can just compare (Function *) type with calledVal (may be Function pointer but how?)
		assert(const_cast<Value *>(calledVal)->getType() == callee->getType());
		ICmpInst* icmpInst = new ICmpInst(insertCmpBeforehere, ICmpInst::ICMP_EQ, const_cast<Value *>(calledVal), callee);

		BasicBlock *notMatchedBB=insertCmpBeforehere->getParent()->splitBasicBlock(insertCmpBeforehere, "NotMatched."+locIDAddr->getName()+"."+callee->getName()); // error when it happens

		BasicBlock *locIDAssignBB = BasicBlock::Create(invokeOrCallInst->getModule()->getContext(), "locIDResolved."+locIDAddr->getName()+"."+callee->getName(), pInvokeOrCallInst->getParent()->getParent());
		new StoreInst(constLocID, locIDAddr, locIDAssignBB);
		BranchInst::Create(jumpHereAfterAssign, locIDAssignBB);

		//Replace unconditional branch to conditional branch
		assert(notMatchedBB->getSinglePredecessor()->getTerminator() == icmpInst->getNextNode() && "ERROR: next instruction of icmp is not BranchInst! ..1");
		assert(isa<BranchInst>(icmpInst->getNextNode()) && "ERROR: next instruction of icmp is not BranchInst! ..2");
		BasicBlock* branchInsertAtEnd = icmpInst->getNextNode()->getParent();
		assert(branchInsertAtEnd == notMatchedBB->getSinglePredecessor());
		icmpInst->getNextNode()->eraseFromParent();
		BranchInst::Create(locIDAssignBB, notMatchedBB, icmpInst, branchInsertAtEnd);
	}
	//addProfilingCodeForCallSite(pInvokeOrCallInst, locIDVal);
	return locIDVal;
}

void CAMPExectime::addProfilingCodeForCallSite(Instruction *invokeOrCallInst, Value *locIDVal){
	std::vector<Value*> args(0);
	args.resize(1);
	args[0] = locIDVal;

	Constant* ContextBegin = campExecCallSiteBegin;
	Constant* ContextEnd = campExecCallSiteEnd;

	CallInst::Create(ContextBegin, args, "", invokeOrCallInst);

	auto found = std::find_if(pCxtTree->begin(), pCxtTree->end(), [invokeOrCallInst](ContextTreeNode *p){return p->getCallInst() == invokeOrCallInst;});
	assert(found != pCxtTree->end());
	if((*found)->isRecursiveCallSiteNode()){
		std::vector<Value*> argsNone(0);
		argsNone.resize(0);
		CallInst::Create(campExecDisableCtxtChange, argsNone, "", invokeOrCallInst);
	}

	assert((isa<InvokeInst>(invokeOrCallInst) && (!isa<TerminatorInst>(invokeOrCallInst))) == false && "WTF??");
	if(TerminatorInst *termInst = dyn_cast<TerminatorInst>(invokeOrCallInst)){

		if(isa<CallInst>(invokeOrCallInst)){
			CallInst *ci = CallInst::Create(ContextEnd, args, "", invokeOrCallInst->getParent());
			if((*found)->isRecursiveCallSiteNode()){
				std::vector<Value*> argsNone(0);
				argsNone.resize(0);
				CallInst::Create(campExecEnableCtxtChange, argsNone, "", ci);
			}
		}
		else if(InvokeInst *invokeInst = dyn_cast<InvokeInst>(invokeOrCallInst)){
			//insert ConextEnd to two successors of invokeInst (normal, exception)
			assert(invokeInst->getNumSuccessors()==2 && "ERROR:: InvokeInst has more than 2 successors!");

			BasicBlock* normalDestBB = invokeInst->getNormalDest();
			CallInst *ci1 = CallInst::Create(ContextEnd, args, "", normalDestBB->getFirstNonPHI());

			BasicBlock* unwindDestBB = invokeInst->getUnwindDest();
			Instruction *landingPadInst = unwindDestBB->getFirstNonPHI();
			assert(landingPadInst == invokeInst->getLandingPadInst());
			assert(landingPadInst->getNextNode() && "ERROR:: next inst of landingPadInst doesn't exist.");
			//if this landingPadBB is exitBB of some Loop, insert ContextEnd before ContextEnd of Loop
			//because function exit(ret) happens first.
			CallInst *ci2 = CallInst::Create(ContextEnd, args, "", landingPadInst->getNextNode());
			if((*found)->isRecursiveCallSiteNode()){
				std::vector<Value*> argsNone(0);
				argsNone.resize(0);
				CallInst::Create(campExecEnableCtxtChange, argsNone, "", ci1);
				CallInst::Create(campExecEnableCtxtChange, argsNone, "", ci2);
			}
		}
		else
			assert(0&&"WTF?????");
	}
	else{
		assert(invokeOrCallInst->getNextNode() != NULL && "ERROR: next instruction of Non-TerminatorInst doesn't exist.");
		CallInst *ci = CallInst::Create(ContextEnd, args, "", invokeOrCallInst->getNextNode());

		if((*found)->isRecursiveCallSiteNode()){
			std::vector<Value*> argsNone(0);
			argsNone.resize(0);
			CallInst::Create(campExecEnableCtxtChange, argsNone, "", ci);
		}

	}



}
