#include "llvm/IR/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Constants.h"

#include "corelab/Utilities/FindFunctionExitBB.hpp"

#define DO_NOTHING 0

using namespace corelab;
using namespace llvm;

static inline bool isRetInst(const Instruction *I) {
	return isa<ReturnInst>(I) || isa<ResumeInst>(I);
}

bool FindFunctionExitBB::runOnModule(Module &M) {
	// errs() << "\nSTART [FindFunctionExitBB::runOnModule]  #######\n";	

	//First, Find Every Funtion who doesn't have ReturnInst
	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		Function &F = *fi;
		if (F.isDeclaration()) continue;

		findExitBBs(&F);

		// if(F.end() == std::find_if(F.begin(), F.end(), [](BasicBlock &bb){
		// 	return std::any_of(bb.begin(), bb.end(), [](Instruction &I){return isRetInst(&I);});})){
		// 	transform(&F);
		// }
	}

	// //TEST
	// Function *mainFcn = &*std::find_if(M.begin(), M.end(), [](Function &fi){return (fi.getName() == "main" && !fi.hasLocalLinkage());});
	// BasicBlock *exitBB = getExitBBOfFunctionWithNoRetInst(mainFcn);
	// errs()<<"exitBB: "<<*exitBB<<"\n";
	// errs() << "\nEND [FindFunctionExitBB::runOnModule]  #######\n";

	return false;
}

void FindFunctionExitBB::findExitBBs(Function *fun){
	assert(exitBBof.find(fun) == exitBBof.end() && "ERROR:: Finding ExitBB occur twice!");

	int retCnt = 0;
	for (inst_iterator I = inst_begin(*fun), E = inst_end(*fun); I != E; ++I){
		if(ReturnInst *ret = dyn_cast<ReturnInst>(&*I)){
			exitBBof[fun] = std::make_pair(ret, std::vector<UnreachableInst *>());
			retCnt++;
		}
	}
	if(retCnt == 0){
		exitBBof[fun] = std::make_pair(nullptr, std::vector<UnreachableInst *>());
	}
	assert((retCnt == 0 || retCnt == 1) && "ERROR");

	for (inst_iterator I = inst_begin(*fun), E = inst_end(*fun); I != E; ++I){
		if(UnreachableInst *unreachableInst = dyn_cast<UnreachableInst>(&*I))
			exitBBof[fun].second.push_back(unreachableInst);
	}
}

FindFunctionExitBB::ExitBBs& FindFunctionExitBB::getExitBB(Function *fun){
	return exitBBof[fun];
}

void FindFunctionExitBB::transform(Function *fun){
	Function &F = *fun;
	LLVMContext &Context = fun->getParent()->getContext();
	std::vector<Value*> actuals(1);

	Constant* Exit = fun->getParent()->getOrInsertFunction(
		"exit",
		Type::getVoidTy(Context),
		Type::getInt32Ty(Context),
		(Type*)0);

	errs()<<" ==>"<<F<<"\n";

	Value* zero = ConstantInt::get(Type::getInt32Ty(Context), 0);
	AllocaInst* alloca = new AllocaInst(Type::getInt32Ty(Context), zero, "", fun->begin()->getFirstNonPHI());

	//Insert uniqueExit BB
	errs()<<"Insert uniqueExit BB\n";
	BasicBlock* uniqExit = BasicBlock::Create(Context, "uniqueExit", fun);
	actuals[0] = new LoadInst(alloca, "",uniqExit);
	CallInst::Create(Exit, actuals, "", uniqExit);
	new UnreachableInst(Context, uniqExit);
	//BranchInst::Create(uniqExit, uniqExit); //should be fixed
	
	//remove call_exit, unreachable & insert Branch_uniqExit
	 for(Function::iterator bbi=F.begin(), be=F.end(); bbi!=be; ++bbi){
	 	BasicBlock &BB = *bbi;
	 	if(BB.getName() != "uniqueExit")		//call_exit in uniqueExit must not be removed
	 		for(BasicBlock::iterator ii=BB.begin(), ie=BB.end(); ii!=ie; ++ii){
		 		//Instruction &I = *ii;
		 		if(const CallInst *CI = dyn_cast<CallInst>(&*ii)) {
		 			Function* F = CI->getCalledFunction();
		 			if(F && (F->getName() == "exit")) {
		 				Instruction *I = &*ii;
		 				//errs()<<"call to exit find!!!\n";

		 				Value* argValue = CI->getArgOperand(0);
		 				new StoreInst(argValue, alloca, I);
		 				BranchInst::Create(uniqExit, I);

		 				(&*(++ii))->eraseFromParent();
	 					I->eraseFromParent();
	 					break;
	 				}
	 			}
	 		}
	 }
	 


}


BasicBlock *FindFunctionExitBB::getExitBBOfFunctionWithNoRetInst(Function *fun){
	assert (fun->end() == std::find_if(fun->begin(), fun->end(), [](BasicBlock &bb){
		return std::any_of(bb.begin(), bb.end(), [](Instruction &I){return isRetInst(&I);});
	}) && "ERROR: In getExitBBOfFunctionWithNoRetInst, Function DOES have Return Instruction.");
	unsigned nUnreachableInst = 0;
	BasicBlock* retBB = NULL;

	assert(std::any_of(inst_begin(fun), inst_end(fun), [](Instruction &I){return isa<UnreachableInst>(I);}) && "FATAL ERROR 1");

	for(Function::iterator bbi=fun->begin(), be=fun->end(); bbi!=be; ++bbi){
		BasicBlock &BB = *bbi;
		for(BasicBlock::iterator ii= BB.begin(), ie=BB.end(); ii!=ie; ++ii) {
			if(const UnreachableInst *UI = dyn_cast<UnreachableInst>(&*ii)) {
				retBB = &BB;
				break;
			}
		}
	}

	for (inst_iterator I = inst_begin(fun), E = inst_end(fun); I != E; ++I){
		Instruction *inst = &*I;
		if(isa<UnreachableInst>(inst)) nUnreachableInst++;
	}
	

	errs()<<"nUnreachableInst:"<<nUnreachableInst<<"\n";
	//assert(0 && "NOT IMPLEMENTED : UnreachableInst .,..");

	return retBB;
}


static RegisterPass<FindFunctionExitBB> X("find-function-exit-bb", "find function exit BB", false, true);
char FindFunctionExitBB::ID = 0;
