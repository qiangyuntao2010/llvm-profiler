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
#include "corelab/ObjTrace/CtxObjtrace.h"

#include <iostream>
#include <vector>
#include <list>
#include <cstdlib>

//#define CAMP_INSTALLER_DEBUG
//#define CAMP_CONTEXT_STACK_APPROACH
#define CAMP_CONTEXT_TREE_APPROACH

#define GET_INSTR_ID(fullId) ((uint32_t) ((fullId >> 16)& 0xFFFFFFFF))

using namespace corelab;

char CtxObjtrace::ID = 0;
static RegisterPass<CtxObjtrace> X("ctx-objtrace", "Context aware Object Tracer", false, false);

//Utils
static const Value *getCalledValueOfIndCall(const Instruction* indCall);
Instruction *getProloguePosition(Instruction *inst);
static Function *getCalledFunction_aux(Instruction* indCall);


void CtxObjtrace::setFunctions(Module &M)
{
	LLVMContext &Context = M.getContext();

	ctxObjInitialize = M.getOrInsertFunction(
			"ctxObjInitialize",
			Type::getVoidTy(Context),
			(Type*)0);

	ctxObjFinalize = M.getOrInsertFunction(
			"ctxObjFinalize",
			Type::getVoidTy(Context),
			(Type*)0);
	
	ctxObjCallSiteBegin = M.getOrInsertFunction(
			"ctxObjCallSiteBegin",
			Type::getVoidTy(Context),
			Type::getInt16Ty(Context),
			(Type*)0);

	ctxObjCallSiteEnd = M.getOrInsertFunction(
			"ctxObjCallSiteEnd",
			Type::getVoidTy(Context),
			Type::getInt16Ty(Context),
			(Type*)0);

	ctxObjLoopBegin = M.getOrInsertFunction(
			"ctxObjLoopBegin",
			Type::getVoidTy(Context),
			Type::getInt16Ty(Context),
			(Type*)0);

	ctxObjLoopNext = M.getOrInsertFunction( 
			"ctxObjLoopNext",
			Type::getVoidTy(Context),
			(Type*)0);

	ctxObjLoopEnd = M.getOrInsertFunction(
			"ctxObjLoopEnd",
			Type::getVoidTy(Context),
			Type::getInt16Ty(Context),
			(Type*)0);
	
	ctxObjDisableCtxtChange = M.getOrInsertFunction(
			"ctxObjDisableCtxtChange",
			Type::getVoidTy(Context),
			(Type*)0);

	ctxObjEnableCtxtChange = M.getOrInsertFunction(
			"ctxObjEnableCtxtChange",
			Type::getVoidTy(Context),
			(Type*)0);

	ctxObjLoadInstr = M.getOrInsertFunction(
      "ctxObjLoadInstr",
      Type::getVoidTy(Context), /* Return type */
      Type::getInt64Ty(Context), /* Address */
      Type::getInt32Ty(Context), /* Instr ID */
      (Type*)0);

  ctxObjStoreInstr = M.getOrInsertFunction(
      "ctxObjStoreInstr",
      Type::getVoidTy(Context), /* Return type */
      Type::getInt64Ty(Context), /* Address */
      Type::getInt32Ty(Context), /* Instr ID */
      (Type*)0);

  ctxObjMalloc = M.getOrInsertFunction(
      "ctxObjMalloc",
      Type::getInt8PtrTy(Context), /* Return type */
      Type::getInt64Ty(Context), /* allocation size */
      Type::getInt32Ty(Context), /* Instr ID */
      (Type*)0);

  ctxObjCalloc = M.getOrInsertFunction(
      "ctxObjCalloc",
      Type::getInt8PtrTy(Context), /* Return type */
      Type::getInt64Ty(Context), /* Num */
      Type::getInt64Ty(Context), /* allocation size */
      Type::getInt32Ty(Context), /* Instr ID */
      (Type*)0);

  ctxObjRealloc = M.getOrInsertFunction(
      "ctxObjRealloc",
      Type::getInt8PtrTy(Context), /* Return type */
      Type::getInt8PtrTy(Context), /* originally allocated address */
      Type::getInt64Ty(Context), /* allocation size */
      Type::getInt32Ty(Context), /* Instr ID */
      (Type*)0);

	return;
}

void CtxObjtrace::setIniFini(Module& M)
{

	LLVMContext &Context = M.getContext();
	std::vector<Type*> formals(0);
	std::vector<Value*> actuals(0);
	FunctionType *voidFcnVoidType = FunctionType::get(Type::getVoidTy(Context), formals, false);

	/* initialize */
	Function *initForCtr = Function::Create( 
			voidFcnVoidType, GlobalValue::InternalLinkage, "__constructor__", &M); 
	BasicBlock *entry = BasicBlock::Create(Context,"entry", initForCtr); 
	BasicBlock *initBB = BasicBlock::Create(Context, "init", initForCtr); 
	actuals.resize(0);
	
	CallInst::Create(ctxObjInitialize, actuals, "", entry); 
	BranchInst::Create(initBB, entry); 
	ReturnInst::Create(Context, 0, initBB);
	callBeforeMain(initForCtr);
	
	/* finalize */
	Function *finiForDtr = Function::Create(voidFcnVoidType, GlobalValue::InternalLinkage, "__destructor__",&M);
	BasicBlock *finiBB = BasicBlock::Create(Context, "entry", finiForDtr);
	BasicBlock *fini = BasicBlock::Create(Context, "fini", finiForDtr);
	
	actuals.resize(0);

	CallInst::Create(ctxObjFinalize, actuals, "", fini);
	BranchInst::Create(fini, finiBB);
	ReturnInst::Create(Context, 0, fini);
	callAfterMain(finiForDtr);
}

bool CtxObjtrace::runOnModule(Module& M) {
	assert((externalCalls.empty() && indirectCalls.empty())&&"ERROR:: CAMPInstaller::runOnModule runs twice?");
	
	setFunctions(M);
	module = &M;
	LLVMContext &Context = M.getContext();
/*	
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
*/
  for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
    Function &F = *fi;
    const DataLayout &dataLayout = module->getDataLayout();
    std::vector<Value*> args(0);
    if (F.isDeclaration()) continue;	// there is no definition then skip this function
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I){
      Instruction *instruction = &*I;
      // For each load instructions
      if(LoadInst *ld = dyn_cast<LoadInst>(instruction)) {
        if(isUseOfGetElementPtrInst(ld) == false){
          args.resize (2);
          Value *addr = ld->getPointerOperand();
          Value *temp = ConstantInt::get(Type::getInt64Ty(Context), 0);
          InstInsertPt out = InstInsertPt::Before(ld);
          addr = castTo(addr, temp, out, &dataLayout);

          FullID fullId = Namer::getFullId(instruction);
					uint32_t instrId = GET_INSTR_ID(fullId);
          Value *instrId_ = ConstantInt::get(Type::getInt32Ty(Context), instrId);
					

          args[0] = addr;
          args[1] = instrId_;
          CallInst::Create(ctxObjLoadInstr, args, "", ld);
        }
      }
      // For each store instructions
      else if (StoreInst *st = dyn_cast<StoreInst>(instruction)) {
        args.resize (2);
        Value *addr = st->getPointerOperand();
        Value *temp = ConstantInt::get(Type::getInt64Ty(Context), 0);
        InstInsertPt out = InstInsertPt::Before(st);
        addr = castTo(addr, temp, out, &dataLayout);

        FullID fullId = Namer::getFullId(instruction);
				uint32_t instrId = GET_INSTR_ID(fullId);
        Value *instrId_ = ConstantInt::get(Type::getInt32Ty(Context), instrId);

        args[0] = addr;
        args[1] = instrId_;
        CallInst::Create(ctxObjStoreInstr, args, "", st);
      }
		}
	}

	hookMallocFree();
	
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

	setIniFini(M);

	return false;
}

void CtxObjtrace::addProfilingCodeForLoop(Loop *L, Value *locIDVal){
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
	CallInst::Create(ctxObjLoopBegin, args, "", preHeader->getTerminator());


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
					CallInst::Create(ctxObjLoopEnd, args, "", inst->getNextNode());
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
					CallInst::Create(ctxObjLoopEnd, args, "", inst);
				}// if exitBB is not LandingPad
			}
		}
	}
	// call beginIteration at the loop header
	// it will increase the iteration count of the loop
	args.resize(0);
	CallInst::Create(ctxObjLoopNext, args, "", header->getFirstNonPHI()); 
}

Value *CtxObjtrace::addTargetComparisonCodeForIndCall(const Instruction *invokeOrCallInst, std::vector<std::pair<Function *, LocalContextID>> &targetLocIDs){
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

void CtxObjtrace::addProfilingCodeForCallSite(Instruction *invokeOrCallInst, Value *locIDVal){
	std::vector<Value*> args(0);
	args.resize(1);
	args[0] = locIDVal;

	Constant* ContextBegin = ctxObjCallSiteBegin;
	Constant* ContextEnd = ctxObjCallSiteEnd;

	CallInst::Create(ContextBegin, args, "", invokeOrCallInst);

	auto found = std::find_if(pCxtTree->begin(), pCxtTree->end(), [invokeOrCallInst](ContextTreeNode *p){return p->getCallInst() == invokeOrCallInst;});
	assert(found != pCxtTree->end());
	if((*found)->isRecursiveCallSiteNode()){
		std::vector<Value*> argsNone(0);
		argsNone.resize(0);
		CallInst::Create(ctxObjDisableCtxtChange, argsNone, "", invokeOrCallInst);
	}

	assert((isa<InvokeInst>(invokeOrCallInst) && (!isa<TerminatorInst>(invokeOrCallInst))) == false && "WTF??");
	if(TerminatorInst *termInst = dyn_cast<TerminatorInst>(invokeOrCallInst)){

		if(isa<CallInst>(invokeOrCallInst)){
			CallInst *ci = CallInst::Create(ContextEnd, args, "", invokeOrCallInst->getParent());
			if((*found)->isRecursiveCallSiteNode()){
				std::vector<Value*> argsNone(0);
				argsNone.resize(0);
				CallInst::Create(ctxObjEnableCtxtChange, argsNone, "", ci);
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
				CallInst::Create(ctxObjEnableCtxtChange, argsNone, "", ci1);
				CallInst::Create(ctxObjEnableCtxtChange, argsNone, "", ci2);
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
			CallInst::Create(ctxObjEnableCtxtChange, argsNone, "", ci);
		}

	}
}

void CtxObjtrace::hookMallocFree(){
  LLVMContext &Context = module->getContext();
  //const DataLayout &dataLayout = module->getDataLayout();
  std::list<Instruction*> listOfInstsToBeErased;
  for(Module::iterator fi = module->begin(), fe = module->end(); fi != fe; ++fi) {
    Function &F = *fi;
    std::vector<Value*> args(0);
    if (F.isDeclaration()) continue;
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I){
      Instruction *instruction = &*I;
      bool wasBitCasted = false;
      Type *ty;
      IRBuilder<> Builder(instruction);//for changing instruction to my instruction
      if(isa<InvokeInst>(instruction) || isa<CallInst>(instruction)){
        Function *callee = getCalledFunction_aux(instruction);
        if(!callee){
          const Value *calledVal = getCalledValueOfIndCall(instruction);
          if(const Function *tarFun = dyn_cast<Function>(calledVal->stripPointerCasts())){//when only one target
            wasBitCasted = true;
            ty = calledVal->getType();
            callee = const_cast<Function *>(tarFun);
          }
        }
        if(!callee) continue;
        if(callee->isDeclaration() == false) continue;


        if(CallInst *callInst = dyn_cast<CallInst>(instruction)){
          if(callee->getName() == "malloc"){
            FullID fullId = Namer::getFullId(instruction);
						uint32_t instrId = GET_INSTR_ID(fullId);
            Value *instrId_ = ConstantInt::get(Type::getInt32Ty(Context), instrId);
            
						args.resize(2);
            args[0] = instruction->getOperand(0);
            args[1] = instrId_;
            if(wasBitCasted){// for indirect call bitcast malloc
              Value * changeTo = Builder.CreateBitCast(ctxObjMalloc, ty);
              CallInst *newCallInst = Builder.CreateCall(changeTo, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            } else {
              CallInst *newCallInst = Builder.CreateCall(ctxObjMalloc, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            }
          }
          else if(callee->getName() == "calloc"){
            FullID fullId = Namer::getFullId(instruction);
						uint32_t instrId = GET_INSTR_ID(fullId);
            Value *instrId_ = ConstantInt::get(Type::getInt32Ty(Context), instrId);
            
						args.resize(3);
            args[0] = instruction->getOperand(0);
            args[1] = instruction->getOperand(1);
            args[2] = instrId_;
            if(wasBitCasted){
              Value *changeTo = Builder.CreateBitCast(ctxObjCalloc, ty);
              CallInst *newCallInst = Builder.CreateCall(changeTo, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            } else {
              CallInst *newCallInst = Builder.CreateCall(ctxObjCalloc, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            }
          }
          else if(callee->getName() == "realloc"){
            FullID fullId = Namer::getFullId(instruction);
						uint32_t instrId = GET_INSTR_ID(fullId);
            Value *instrId_ = ConstantInt::get(Type::getInt32Ty(Context), instrId);
            
						args.resize(3);
            args[0] = instruction->getOperand(0);
            args[1] = instruction->getOperand(1);
            args[2] = instrId_;
            if(wasBitCasted){
              Value *changeTo = Builder.CreateBitCast(ctxObjRealloc, ty);
              CallInst *newCallInst = Builder.CreateCall(changeTo, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            } else {
              CallInst *newCallInst = Builder.CreateCall(ctxObjRealloc, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            }
          }
        }
        else if(InvokeInst *callInst = dyn_cast<InvokeInst>(instruction)){
          if(callee->getName() == "malloc"){
            FullID fullId = Namer::getFullId(instruction);
						uint32_t instrId = GET_INSTR_ID(fullId);
            Value *instrId_ = ConstantInt::get(Type::getInt32Ty(Context), instrId);

            args.resize(2);
            args[0] = instruction->getOperand(0);
            args[1] = instrId_;
            if(wasBitCasted){
              Value *changeTo = Builder.CreateBitCast(ctxObjMalloc, ty);
              CallInst *newCallInst = Builder.CreateCall(changeTo, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            } else {
              CallInst *newCallInst = Builder.CreateCall(ctxObjMalloc, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            }
          }
          else if(callee->getName() == "calloc"){
            FullID fullId = Namer::getFullId(instruction);
						uint32_t instrId = GET_INSTR_ID(fullId);
            Value *instrId_ = ConstantInt::get(Type::getInt32Ty(Context), instrId);

            args.resize(3);
            args[0] = instruction->getOperand(0);
            args[1] = instruction->getOperand(1);
            args[2] = instrId_;
            if(wasBitCasted){
              Value *changeTo = Builder.CreateBitCast(ctxObjCalloc, ty);
              CallInst *newCallInst = Builder.CreateCall(changeTo, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            } else {
              CallInst *newCallInst = Builder.CreateCall(ctxObjCalloc, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            }
          }
          else if(callee->getName() == "realloc"){
            FullID fullId = Namer::getFullId(instruction);
						uint32_t instrId = GET_INSTR_ID(fullId);
            Value *instrId_ = ConstantInt::get(Type::getInt32Ty(Context), instrId);

            args.resize(3);
            args[0] = instruction->getOperand(0);
            args[1] = instruction->getOperand(1);
            args[2] = instrId_;
            if(wasBitCasted){
              Value *changeTo = Builder.CreateBitCast(ctxObjRealloc, ty);
              CallInst *newCallInst = Builder.CreateCall(changeTo, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            } else {
              CallInst *newCallInst = Builder.CreateCall(ctxObjRealloc, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            }
          }
        }
        else
          assert(0&&"ERROR!!");
      }// closing bracket for "if(isa<InvokeInst>(instruction) || isa<CallInst>(instruction)){"
    } // for bb
  } // for ff
  for(auto I: listOfInstsToBeErased) {
    I->eraseFromParent();
  }
}

void CtxObjtrace::makeMetadata(Instruction* instruction, uint64_t Id) {
  LLVMContext &context = module->getContext();
  Constant* IdV = ConstantInt::get(Type::getInt64Ty(context), Id);
  Metadata* IdM = (Metadata*)ConstantAsMetadata::get(IdV);
  Metadata* valuesArray[] = {IdM};
  ArrayRef<Metadata *> values(valuesArray, 1);
  MDNode* mdNode = MDNode::get(context, values);
  NamedMDNode *namedMDNode = module->getOrInsertNamedMetadata("corelab.namer");
  namedMDNode->addOperand(mdNode);
  instruction->setMetadata("namer", mdNode);
  return;
}

bool CtxObjtrace::isUseOfGetElementPtrInst(LoadInst *ld){
  // is only Used by GetElementPtrInst ?
  return std::all_of(ld->user_begin(), ld->user_end(), [](User *user){return isa<GetElementPtrInst>(user);});
}

Value* CtxObjtrace::castTo(Value* from, Value* to, InstInsertPt &out, const DataLayout *dl)
{
  LLVMContext &Context = module->getContext();
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

static const Value *getCalledValueOfIndCall(const Instruction* indCall){
  if(const CallInst *callInst = dyn_cast<CallInst>(indCall)){
    return callInst->getCalledValue();
  }
  else if(const InvokeInst *invokeInst = dyn_cast<InvokeInst>(indCall)){
    return invokeInst->getCalledValue();
  }
  else
    assert(0 && "WTF??");
}

static Function *getCalledFunction_aux(Instruction* indCall){
  if(CallInst *callInst = dyn_cast<CallInst>(indCall)){
    return callInst->getCalledFunction();
  }
  else if(InvokeInst *invokeInst = dyn_cast<InvokeInst>(indCall)){
    return invokeInst->getCalledFunction();
  }
  else
    assert(0 && "WTF??");
}
