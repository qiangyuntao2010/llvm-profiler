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

#include "corelab/Utilities/InstInsertPt.h"
#include "corelab/Utilities/GlobalCtors.h"
#include "corelab/Metadata/Metadata.h"
#include "corelab/Metadata/LoadNamer.h"
#include "corelab/CAMP/CAMP.h"

#include <iostream>
#include <vector>
#include <list>
#include <cstdlib>

//#define CAMP_INSTALLER_DEBUG
//#define CAMP_CONTEXT_STACK_APPROACH
#define CAMP_CONTEXT_TREE_APPROACH

using namespace corelab;
using namespace corelab::CAMP;

char CAMPInstaller::ID = 0;
static RegisterPass<CAMPInstaller> X("camp", "Check the Context-Aware Memory dependence", false, false);

//Utils
const Value *getCalledValueOfIndCall(const Instruction* indCall);
Instruction *getProloguePosition(Instruction *inst);
Function *getCalledFunction_aux(Instruction* indCall);



void CAMPInstaller::setFunctions(Module &M)
{
	LLVMContext &Context = M.getContext();

	campInitialize = M.getOrInsertFunction(
			"campInitialize",
			Type::getVoidTy(Context),
			Type::getInt64Ty(Context),
			Type::getInt64Ty(Context),
			Type::getInt64Ty(Context),
			Type::getInt64Ty(Context),
			Type::getInt64Ty(Context),
			(Type*)0);

	campFinalize = M.getOrInsertFunction(
			"campFinalize",
			Type::getVoidTy(Context),
			(Type*)0);

	campLoadInstr = M.getOrInsertFunction(
			"campLoadInstr",
			Type::getVoidTy(Context),
			Type::getInt64Ty(Context),
			Type::getInt16Ty(Context),
			(Type*)0);

	campStoreInstr = M.getOrInsertFunction(
			"campStoreInstr",
			Type::getVoidTy(Context),
			Type::getInt64Ty(Context),
			Type::getInt16Ty(Context),
			(Type*)0);

	campCallSiteBegin = M.getOrInsertFunction(
			"campCallSiteBegin",
			Type::getVoidTy(Context),
			Type::getInt16Ty(Context),
			(Type*)0);

	campCallSiteEnd = M.getOrInsertFunction(
			"campCallSiteEnd",
			Type::getVoidTy(Context),
			Type::getInt16Ty(Context),
			(Type*)0);

	campLoopBegin = M.getOrInsertFunction(
			"campLoopBegin",
			Type::getVoidTy(Context),
			Type::getInt16Ty(Context),
			(Type*)0);

	campLoopNext = M.getOrInsertFunction( 
			"campLoopNext",
			Type::getVoidTy(Context),
			(Type*)0);

	campLoopEnd = M.getOrInsertFunction(
			"campLoopEnd",
			Type::getVoidTy(Context),
			Type::getInt16Ty(Context),
			(Type*)0);

	campMalloc = M.getOrInsertFunction(
			"campMalloc",
			Type::getInt8PtrTy(Context),
			Type::getInt64Ty(Context),
			(Type*)0);

	campCalloc = M.getOrInsertFunction(
			"campCalloc",
			Type::getInt8PtrTy(Context),
			Type::getInt64Ty(Context),
			Type::getInt64Ty(Context),
			(Type*)0);

	campRealloc = M.getOrInsertFunction(
			"campRealloc",
			Type::getInt8PtrTy(Context),
			Type::getInt8PtrTy(Context),
			Type::getInt64Ty(Context),
			(Type*)0);

	campFree = M.getOrInsertFunction(
			"campFree",
			Type::getVoidTy(Context),
			Type::getInt8PtrTy(Context),
			(Type*)0);

	campDisableCtxtChange = M.getOrInsertFunction(
			"campDisableCtxtChange",
			Type::getVoidTy(Context),
			(Type*)0);

	campEnableCtxtChange = M.getOrInsertFunction(
			"campEnableCtxtChange",
			Type::getVoidTy(Context),
			(Type*)0);

	// campMemset = M.getOrInsertFunction(
	// 		"campMemset",
	// 		Type::getInt8PtrTy(Context),
	// 		Type::getInt32Ty(Context),
	// 		Type::getInt64Ty(Context), /* size_t type. It could be Int32
	// 																	in 32bits OS */
	// 		(Type*)0);

	// campMemcpy = M.getOrInsertFunction(
	// 		"campMemcpy",
	// 		Type::getInt8PtrTy(Context),
	// 		Type::getInt8PtrTy(Context),
	// 		Type::getInt64Ty(Context), /* size_t type. It could be Int32
	// 																	in 32bits OS */
	// 		(Type*)0);

	// campMemmove = M.getOrInsertFunction(
	// 		"campMemmove",
	// 		Type::getInt8PtrTy(Context),
	// 		Type::getInt8PtrTy(Context),
	// 		Type::getInt64Ty(Context), /* size_t type. It could be Int32
	// 																	in 32bits OS */
	// 		(Type*)0);
	
	// campAllocaInstr = M.getOrInsertFunction(
	// 		"campAllocaInstr",
	// 		Type::getVoidTy(Context),
	// 		Type::getInt64Ty(Context),
	// 		Type::getInt64Ty(Context),
	// 		(Type*)0);
	return;
}

void CAMPInstaller::setIniFini(Module& M)
{
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
	
	CallInst::Create(campInitialize, actuals, "", entry); 
	BranchInst::Create(initBB, entry); 
	ReturnInst::Create(Context, 0, initBB);
	callBeforeMain(initForCtr);
	
	/* finalize */
	Function *finiForDtr = Function::Create(voidFcnVoidType, GlobalValue::InternalLinkage, "__destructor__",&M);
	BasicBlock *finiBB = BasicBlock::Create(Context, "entry", finiForDtr);
	BasicBlock *fini = BasicBlock::Create(Context, "fini", finiForDtr);
	actuals.resize(0);
	CallInst::Create(campFinalize, actuals, "", fini);
	BranchInst::Create(fini, finiBB);
	ReturnInst::Create(Context, 0, fini);
	callAfterMain(finiForDtr);
}

bool CAMPInstaller::runOnModule(Module& M) {
	assert((externalCalls.empty() && indirectCalls.empty())&&"ERROR:: CAMPInstaller::runOnModule runs twice?");
	setFunctions(M);
	pLoadNamer = &getAnalysis< LoadNamer >();
	LoadNamer &loadNamer = *pLoadNamer;
	module = &M;

	DEBUG(errs()<<"############## runOnModule [CAMP] START ##############\n");
	

	#ifdef CAMP_CONTEXT_STACK_APPROACH
	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		Function &F = *fi;
		if (F.isDeclaration()) continue;

		FuncID functionId = loadNamer.getFunctionId(F);
	
		// Handling Loops
		LoopInfo &li = getAnalysis< LoopInfoWrapperPass >(F).getLoopInfo();
		list<Loop*> loops(li.begin(), li.end());
		while(!loops.empty()) {
			Loop *loop = loops.front();
			loops.pop_front();
			runOnLoop(loop, functionId);
		}
	//	runOnFunction(&F); //for debuging
	}
	#endif //CAMP_CONTEXT_STACK_APPROACH

	#ifdef CAMP_CONTEXT_TREE_APPROACH

	cxtTreeBuilder = &getAnalysis< ContextTreeBuilder >();
	pCxtTree = cxtTreeBuilder->getContextTree();
	locIdOf_callSite = cxtTreeBuilder->getLocIDMapForCallSite();
	locIdOf_indCall = cxtTreeBuilder->getLocIDMapForIndirectCalls();
	locIdOf_loop = cxtTreeBuilder->getLocIDMapForLoop();
	ContextTreeBuilder::LoopIdOf *loopIdOf = cxtTreeBuilder->getLoopIDMap();
	ContextTreeBuilder::LoopOfCntxID *loopOfLoopID = cxtTreeBuilder->getLoopMapOfCntxID();

	// ContextTreeNode *rootNode = pCxtTree->front();
	// assert((rootNode->getUCID() == 0 && rootNode->getParent() == NULL) && "ERROR: root is not main!");

	//traverse locIdOf_callSite, locIdOf_indCall, locIdOf_loop not Module or Tree
	//get TreeNode From cxtTree by querying it

	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		Function &F = *fi;
		LLVMContext &Context = M.getContext();
		const DataLayout &dataLayout = module->getDataLayout();
		std::vector<Value*> args(0);
		if (F.isDeclaration()) continue;
		for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I){
			Instruction *instruction = &*I; 
			if(LoadInst *ld = dyn_cast<LoadInst>(instruction)) {
				if(isUseOfGetElementPtrInst(ld) == false){
					args.resize (2);
					Value *addr = ld->getPointerOperand();
					Value *temp = ConstantInt::get(Type::getInt64Ty(Context), 0);
					InstInsertPt out = InstInsertPt::Before(ld);
					addr = castTo(addr, temp, out, &dataLayout); 
					
					InstrID instrId = Namer::getInstrId(instruction);

					Value *instructionId = ConstantInt::get(Type::getInt16Ty(Context), instrId);
					//for debug
					//errs()<<"<"<<instrId<<"> "<<*ld<<"\n";

					#ifdef CAMP_INSTALLER_DEBUG
						fprintf(stderr, "load instruction id %" SCNu16 "\n",instrId); 
					#endif

					args[0] = addr;
					args[1] = instructionId;
					CallInst::Create(campLoadInstr, args, "", ld);	
				}
			}
			// For each store instructions
			else if (StoreInst *st = dyn_cast<StoreInst>(instruction)) {
				args.resize (2);
				Value *addr = st->getPointerOperand();
				Value *temp = ConstantInt::get(Type::getInt64Ty(Context), 0);
				InstInsertPt out = InstInsertPt::Before(st);
				addr = castTo(addr, temp, out, &dataLayout); 
				
				InstrID instrId = Namer::getInstrId(instruction);
				Value *instructionId = ConstantInt::get(Type::getInt16Ty(Context), instrId);
				//for debug
				//errs()<<"<"<<instrId<<"> "<<*st<<"\n";

				#ifdef CAMP_INSTALLER_DEBUG
					fprintf(stderr, "store instruction id %" SCNu16 "\n",instrId);
				#endif

				args[0] = addr;
				args[1] = instructionId;
				CallInst::Create(campStoreInstr, args, "", st);			
			}
		}
	}

	//first, traverse locIdOf_indCall
	for(std::pair<CntxID, const Loop *> loopID : *loopOfLoopID){
		Value *locIDVal = ConstantInt::get(Type::getInt16Ty(M.getContext()), (*locIdOf_loop)[loopID.first]);
		addProfilingCodeForLoop(const_cast<Loop *>(loopID.second), locIDVal);
	}
/*
	for( std::pair<const Instruction *, std::vector<std::pair<Function *, LocalContextID>>> &indCall : *locIdOf_indCall){
		Value *locIDVal = addTargetComparisonCodeForIndCall(indCall.first, indCall.second);
		addProfilingCodeForCallSite(const_cast<Instruction *>(indCall.first), locIDVal);
	}

	for( std::pair<const Instruction *, LocalContextID> &normalCallSite : *locIdOf_callSite ){
		Value *locIDVal = ConstantInt::get(Type::getInt16Ty(getGlobalContext()), normalCallSite.second);
		addProfilingCodeForCallSite(const_cast<Instruction *>(normalCallSite.first), locIDVal);
	}
*/
	hookMallocFree();

	//cxtTreeBuilder->printContextTree();
	
	#endif //CAMP_CONTEXT_TREE_APPROACH

	DEBUG(errs()<<"############## runOnModule [CAMP] END ##############\n");
	setIniFini(M);
	return false;
}

void CAMPInstaller::hookMallocFree(){
	for(Module::iterator fi = module->begin(), fe = module->end(); fi != fe; ++fi) {
		Function &F = *fi;
		std::vector<Value*> args(0);

		if (F.isDeclaration()) continue;
		for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I){
			Instruction *instruction = &*I;
			bool wasBitCasted = false;
			Type *ty;
			IRBuilder<> Builder(instruction);
			if(isa<InvokeInst>(instruction) || isa<CallInst>(instruction)){
				Function *callee = getCalledFunction_aux(instruction);
				if(!callee){
					const Value *calledVal = getCalledValueOfIndCall(instruction);
					if(const Function *tarFun = dyn_cast<Function>(calledVal->stripPointerCasts())){
						wasBitCasted = true;
						ty = calledVal->getType();
						callee = const_cast<Function *>(tarFun);
					}
				}
				if(!callee) continue;
				if(callee->isDeclaration() == false) continue;

				if(CallInst *callInst = dyn_cast<CallInst>(instruction)){
					if(callee->getName() == "malloc"){
						if(wasBitCasted){
							Value * changeTo = Builder.CreateBitCast(campMalloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(campMalloc);
						}
					}
					else if(callee->getName() == "calloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(campCalloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(campCalloc);
						}
					}
					else if(callee->getName() == "realloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(campRealloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(campRealloc);
						}
					}
					else if(callee->getName() == "free"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(campFree, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(campFree);
						}
					}
				}
				else if(InvokeInst *callInst = dyn_cast<InvokeInst>(instruction)){
					if(callee->getName() == "malloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(campMalloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(campMalloc);
						}
					}
					else if(callee->getName() == "calloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(campCalloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(campCalloc);
						}
					}
					else if(callee->getName() == "realloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(campRealloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(campRealloc);
						}
					}
					else if(callee->getName() == "free"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(campFree, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(campFree);
						}
					}
				}
				else
					assert(0&&"ERROR!!");
			}
		}
	}
}

bool CAMPInstaller::isUseOfGetElementPtrInst(LoadInst *ld){
	// is only Used by GetElementPtrInst ?
	return std::all_of(ld->user_begin(), ld->user_end(), [](User *user){return isa<GetElementPtrInst>(user);});
}

void CAMPInstaller::addProfilingCodeForLoop(Loop *L, Value *locIDVal){
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
	CallInst::Create(campLoopBegin, args, "", preHeader->getTerminator());

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
					CallInst::Create(campLoopEnd, args, "", inst->getNextNode());
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
					CallInst::Create(campLoopEnd, args, "", inst);
				}// if exitBB is not LandingPad
			}
		}
	}
	// call beginIteration at the loop header
	// it will increase the iteration count of the loop
	args.resize(0);
	CallInst::Create(campLoopNext, args, "", header->getFirstNonPHI()); 
}

Value *CAMPInstaller::addTargetComparisonCodeForIndCall(const Instruction *invokeOrCallInst, std::vector<std::pair<Function *, LocalContextID>> &targetLocIDs){
	assert(isa<InvokeInst>(invokeOrCallInst)||isa<CallInst>(invokeOrCallInst));
	assert((*locIdOf_callSite)[invokeOrCallInst] == (LocalContextID)(-1));

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

		BasicBlock *notMatchedBB=insertCmpBeforehere->getParent()->splitBasicBlock(insertCmpBeforehere, "NotMatched."+locIDAddr->getName()+"."+callee->getName());

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

void CAMPInstaller::addProfilingCodeForCallSite(Instruction *invokeOrCallInst, Value *locIDVal){
	std::vector<Value*> args(0);
	args.resize(1);
	args[0] = locIDVal;

	Constant* ContextBegin = campCallSiteBegin;
	Constant* ContextEnd = campCallSiteEnd;

	CallInst::Create(ContextBegin, args, "", invokeOrCallInst);

	auto found = std::find_if(pCxtTree->begin(), pCxtTree->end(), [invokeOrCallInst](ContextTreeNode *p){return p->getCallInst() == invokeOrCallInst;});
	assert(found != pCxtTree->end());
	if((*found)->isRecursiveCallSiteNode()){
		std::vector<Value*> argsNone(0);
		argsNone.resize(0);
		CallInst::Create(campDisableCtxtChange, argsNone, "", invokeOrCallInst);
	}

	assert((isa<InvokeInst>(invokeOrCallInst) && (!isa<TerminatorInst>(invokeOrCallInst))) == false && "WTF??");
	if(TerminatorInst *termInst = dyn_cast<TerminatorInst>(invokeOrCallInst)){

		if(isa<CallInst>(invokeOrCallInst)){
			CallInst *ci = CallInst::Create(ContextEnd, args, "", invokeOrCallInst->getParent());
			if((*found)->isRecursiveCallSiteNode()){
				std::vector<Value*> argsNone(0);
				argsNone.resize(0);
				CallInst::Create(campEnableCtxtChange, argsNone, "", ci);
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
				CallInst::Create(campEnableCtxtChange, argsNone, "", ci1);
				CallInst::Create(campEnableCtxtChange, argsNone, "", ci2);
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
			CallInst::Create(campEnableCtxtChange, argsNone, "", ci);
		}

	}



}



// Context Stack approach (baseline)
// Get Loop begin & end
// Call Runtime function -> deliver loop nest info & loop ID to the runtime system
bool CAMPInstaller::runOnLoop(Loop *L, FuncID functionId)
{
	//LoadNamer &loadNamer = getAnalysis< LoadNamer >();
	LoadNamer &loadNamer = *pLoadNamer;
	CntxID loopContextId = loadNamer.getLoopContextId(L, functionId);

	#ifdef CAMP_INSTALLER_DEBUG
		fprintf(stderr, "function id %" SCNu16 " loop id %" SCNu16 "\n",
				functionId, loopContextId); 
	#endif

	// get the pre-header, header and exitblocks
	BasicBlock *header = L->getHeader();
	BasicBlock *preHeader = L->getLoopPreheader();
	SmallVector<BasicBlock*, 1> exitingBlocks;
	L->getExitingBlocks(exitingBlocks);
	
	// set loopID as an argument
	std::vector<Value*> args(1); 
	args.resize(1);

	// call beginLoop at the loop preheader
	// it will push the conext
	args.resize(1);
	Value *loopId =	ConstantInt::get(Type::getInt16Ty(module->getContext()), loopContextId);
	args[0] = loopId; 
	CallInst::Create(campLoopBegin, args, "", preHeader->getTerminator());
	//for debug
	//errs()<<"{"<<loopContextId<<"}"<<*L;

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
					args[0] = loopId;	
					Instruction *inst = exitBB->getFirstNonPHI();
					assert(exitBB->getLandingPadInst() == inst && "ERROR:: FirstNonPHI of LandingPad BB is not LandingPadinst!");
					assert(inst->getNextNode() && "ERROR:: next inst of LandingPadinst doesnt exists.");
					CallInst::Create(campLoopEnd, args, "", inst->getNextNode());
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
					args[0] = loopId;	
					Instruction *inst = edgeBB->getFirstNonPHI();
					CallInst::Create(campLoopEnd, args, "", inst);
				}// if exitBB is not LandingPad
			}
		}
	}
	// #ifdef CAMP_INSTALLER_DEBUG
	// SmallVector<BasicBlock*, 1> exitBlocks;
	// L->getExitBlocks(exitBlocks);
	// 	for (unsigned i = 0; i < exitBlocks.size(); ++i)
	// 	{
	// 		// for debug
	// 		BasicBlock *exitBB = exitBlocks[i];
	// 		errs()<<exitBB->getName().data()<<" ";
	// 		for (pred_iterator PI = pred_begin(exitBB), E =
	// 				pred_end(exitBB); PI
	// 				!= E; ++PI) {
	// 			  BasicBlock *Pred = *PI;
	// 				errs()<< Pred->getName().data()<<", ";
	// 		}
	// 		errs()<<"\n";
	// 	}
	// #endif	

	// call beginIteration at the loop header
	// it will increase the iteration count of the loop
	args.resize(1);
	args[0] = loopId;
	CallInst::Create(campLoopNext, args, "", header->getFirstNonPHI()); 

	// for each subloops, call runOnLoop function recursively. 
	std::list<Loop*> subloops(L->getSubLoops().begin(),	L->getSubLoops().end());
	while (!subloops.empty()) {
		Loop *loop = subloops.front();
		subloops.pop_front();
		runOnLoop(loop, functionId);
	}
	return false;
}

// Context Stack approach (baseline)
bool CAMPInstaller::runOnFunction(Function *F)
{
	LLVMContext &Context = F->getParent()->getContext();
	//DataLayout &dataLayout = getAnalysis< DataLayout >();
	const DataLayout &dataLayout = F->getParent()->getDataLayout();
	//LoadNamer &loadNamer = getAnalysis< LoadNamer >();
	LoadNamer &loadNamer = *pLoadNamer;
	std::vector<Value*> args(0);

	// Set up the call Instructions for each instructions
	for (inst_iterator instIter = inst_begin(F), E = inst_end(F); instIter != E; ++instIter) {
		Instruction *instruction = &*instIter;

		// For each load instructions
		if(LoadInst *ld = dyn_cast<LoadInst>(instruction)) {
			args.resize (2);
			Value *addr = ld->getPointerOperand();
			Value *temp = ConstantInt::get(Type::getInt64Ty(Context), 0);
			InstInsertPt out = InstInsertPt::Before(ld);
			addr = castTo(addr, temp, out, &dataLayout); 
			
			InstrID instrId = Namer::getInstrId(instruction);
			Value *instructionId = ConstantInt::get(Type::getInt16Ty(Context), instrId);
			//for debug
			//errs()<<"<"<<instrId<<"> "<<*ld<<"\n";

			#ifdef CAMP_INSTALLER_DEBUG
				fprintf(stderr, "load instruction id %" SCNu16 "\n",instrId); 
			#endif

			args[0] = addr;
			args[1] = instructionId;
			CallInst::Create(campLoadInstr, args, "", ld);
		}
		// For each store instructions
		else if (StoreInst *st = dyn_cast<StoreInst>(instruction)) {
			args.resize (2);
			Value *addr = st->getPointerOperand();
			Value *temp = ConstantInt::get(Type::getInt64Ty(Context), 0);
			InstInsertPt out = InstInsertPt::Before(st);
			addr = castTo(addr, temp, out, &dataLayout); 
			
			InstrID instrId = Namer::getInstrId(instruction);
			Value *instructionId = ConstantInt::get(Type::getInt16Ty(Context), instrId);
			//for debug
			//errs()<<"<"<<instrId<<"> "<<*st<<"\n";

			#ifdef CAMP_INSTALLER_DEBUG
				fprintf(stderr, "store instruction id %" SCNu16 "\n",instrId);
			#endif

			args[0] = addr;
			args[1] = instructionId;
			CallInst::Create(campStoreInstr, args, "", st);			
		}
		else if(InvokeInst *invokeInst = dyn_cast<InvokeInst>(instruction)){
			Function* callee = invokeInst->getCalledFunction();
			//for each call instructions add the context id
			if(callee == NULL)//Indirect call
				indirectCalls.push_back(invokeInst); //add to external function call list.
			else if(callee->isDeclaration())
				externalCalls.push_back(invokeInst); //add to external function call list.	
			else
			{
				FuncID funId = loadNamer.getFunctionId(*callee);
				assert(funId != 0 && "ERROR:: funId == 0");
				InstrID invokeInstID = Namer::getInstrId(invokeInst);
				args.resize(1);
				Value *invokeInstIDVal = ConstantInt::get(Type::getInt16Ty(Context), invokeInstID);
				args[0] = invokeInstIDVal;

				Constant* ContextBegin = campCallSiteBegin;
				Constant* ContextEnd = campCallSiteEnd;

				CallInst::Create(ContextBegin, args, "", invokeInst);
				//for debug
				//errs()<<"{"<<invokeInstID<<"}"<<*invokeInst<<"\n";

				// find the instructions after invokeInst, in order to add the
				// context after function.
				BasicBlock *bb = invokeInst->getParent();
				Instruction *cdAfter = NULL;
				bool find = false;
				for(BasicBlock::iterator ii = bb->begin(), ie = bb->end();
						ii != ie; ++ii) {
					if(find) {
						cdAfter = &(*ii);
						break;
					}
					if (&(*ii) == instruction)
						find = true;
				}

				//Instead of this
				//CallInst::Create(ContextEnd, args, "", cdAfter);
				//insert ConextEnd to two successors of invokeInst (normal, exception)
				assert(find && "ERROR:: Cannot find target invokeInst! why?");
				assert(invokeInst->getNumSuccessors()==2 && "ERROR:: InvokeInst has more than 2 successors!");

				BasicBlock* normalDestBB = invokeInst->getNormalDest();
				CallInst::Create(ContextEnd, args, "", normalDestBB->getFirstNonPHI());

				BasicBlock* unwindDestBB = invokeInst->getUnwindDest();
				Instruction *landingPadInst = unwindDestBB->getFirstNonPHI();
				assert(landingPadInst == invokeInst->getLandingPadInst());
				assert(landingPadInst->getNextNode() && "ERROR:: next inst of landingPadInst doesn't exist.");
				//if this landingPadBB is exitBB of some Loop, insert ContextEnd before ContextEnd of Loop
				//because function exit(ret) happens first.
				CallInst::Create(ContextEnd, args, "", landingPadInst->getNextNode());

			}
		} //InvokeInst
		
		// change the memory function calls to camp version. //WHY?
		else if (CallInst *callInst = dyn_cast<CallInst>(instruction))
		{
			Function* callee = callInst->getCalledFunction();
			//for each call instructions add the context id
			if(callee == NULL)//Indirect call
				indirectCalls.push_back(callInst); //add to external function call list.
			else if(callee->isDeclaration())
				externalCalls.push_back(callInst); //add to external function call list.	
			else
			{
				FuncID funId = loadNamer.getFunctionId(*callee);
				assert(funId != 0 && "ERROR:: funId == 0");
				InstrID callInstID = Namer::getInstrId(callInst);
				args.resize(1);
				Value *callInstIDVal = ConstantInt::get(Type::getInt16Ty(Context), callInstID);
				args[0] = callInstIDVal;

				Constant* ContextBegin = campCallSiteBegin;
				Constant* ContextEnd = campCallSiteEnd;

				CallInst::Create(ContextBegin, args, "", callInst);
				//for debug
				//errs()<<"{"<<callInstID<<"}"<<*callInst<<"\n";

				// find the instructions after callInst, in order to add the
				// context after function.
				BasicBlock *bb = callInst->getParent();
				Instruction *cdAfter = NULL;
				bool find = false;
				for(BasicBlock::iterator ii = bb->begin(), ie = bb->end();
						ii != ie; ++ii) {
					if(find) {
						cdAfter = &(*ii);
						break;
					}
					if (&(*ii) == instruction)
						find = true;
				}
				if (find)
					CallInst::Create(ContextEnd, args, "", cdAfter);	
				else
					CallInst::Create(ContextEnd, args, "", bb);
			}
		} //inst == CallInst
	}

	return false;
}

//Utility
Value* CAMPInstaller::castTo(Value* from, Value* to, InstInsertPt &out, const DataLayout *dl)
{
	LLVMContext &Context = from->getContext();
	const size_t fromSize = dl->getTypeSizeInBits( from->getType() );
	const size_t toSize = dl->getTypeSizeInBits( to->getType() );

	// First make it an integer
	if( ! from->getType()->isIntegerTy() ) {
		// cast to integer of same size of bits
		Type *integer = IntegerType::get(Context, fromSize);
		Instruction *cast;
		if( from->getType()->getTypeID() == Type::PointerTyID )
			cast = new PtrToIntInst(from, integer);
		else {
			cast = new BitCastInst(from, integer);
		}
		out << cast;
		from = cast;
	} 

	// Next, make it have the same size
	if( fromSize < toSize ) {
		Type *integer = IntegerType::get(Context, toSize);
		Instruction *cast = new ZExtInst(from, integer);
		out << cast;
		from = cast;
	} else if ( fromSize > toSize ) {
		Type *integer = IntegerType::get(Context, toSize);
		Instruction *cast = new TruncInst(from, integer);
		out << cast;
		from = cast;
	}

	// possibly bitcast it to the approriate type
	if( to->getType() != from->getType() ) {
		Instruction *cast;
		if( to->getType()->getTypeID() == Type::PointerTyID )
			cast = new IntToPtrInst(from, to->getType() );
		else {
			cast = new BitCastInst(from, to->getType() );
		}

		out << cast;
		from = cast;
	}

	return from;
}

Instruction *getProloguePosition(Instruction *inst){
	return inst->getParent()->getParent()->front().getFirstNonPHI();
}





// //TODO:: where to put this useful function?
// const Value *getCalledValueOfIndCall(const Instruction* indCall){
// 	if(const CallInst *callInst = dyn_cast<CallInst>(indCall)){
// 		return callInst->getCalledValue();
// 	}
// 	else if(const InvokeInst *invokeInst = dyn_cast<InvokeInst>(indCall)){
// 		return invokeInst->getCalledValue();
// 	}
// 	else
// 		assert(0 && "WTF??");
// }




























// StringRef ma("malloc");
// StringRef fr("free");
// StringRef ca("calloc");
// StringRef re("realloc");

// Function* f = cd->getCalledFunction();
// if (f != NULL)
// {
// 	if (f->getName().equals(ma))
// 		cd->setCalledFunction(campMalloc);
// 	else if (f->getName().equals(fr))
// 		cd->setCalledFunction(campFree);	
// 	else if (f->getName().equals(ca))
// 		cd->setCalledFunction(campCalloc);	
// 	else if (f->getName().equals(re))
// 		cd->setCalledFunction(campRealloc);	
// }

// else if (instruction->getOpcode() == Instruction::Alloca) {
// 	AllocaInst* ac = (AllocaInst*)instruction;
// 	BasicBlock::iterator ni = ii;
// 	Instruction* next = *&(++ni);
	
// 	// take the address of Alloca
// 	Value* addr = (Value*) ac;
// 	Value *temp = ConstantInt::get(Type::getInt64Ty(Context), 0);
// 	InstInsertPt out = InstInsertPt::After(ac);
// 	addr = castTo(addr, temp, out, &dataLayout); 

// 	// take the size of Alloca
// 	Value* size = ac->getArraySize();
// 	uint64_t arraySize = ((ConstantInt*)size)->getZExtValue();
// 	Type* type = ac->getAllocatedType();
// 	uint64_t typeSize = dataLayout.getTypeSizeInBits(type) >> 3;
// 	uint64_t totalSize = typeSize * arraySize;
// 	Value* TotalSize = 
// 		ConstantInt::get(Type::getInt64Ty(Context), totalSize);

// 	Args.resize (2);
// 	Args[0] = addr;
// 	Args[1] = TotalSize;
// 	CallInst::Create(campAllocaInstr, Args, "", next); 


// 	fprintf(stderr, "alloca %lu %lu \n",
// 			((ConstantInt*)addr)->getZExtValue(), totalSize);
// }
