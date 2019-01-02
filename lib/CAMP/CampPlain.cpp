#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/CallSite.h"

#include "corelab/Utilities/GlobalCtors.h"
#include "corelab/Metadata/Metadata.h"
#include "corelab/Metadata/LoadNamer.h"

#include "corelab/CAMP/CampPlain.h"

using namespace corelab;

char CAMPPlain::ID = 0;
static RegisterPass<CAMPPlain> X("camp-plain", "execution time check for plain exe", false, false);

void CAMPPlain::setFunctions(Module &M)
{
	LLVMContext &Context = M.getContext();

	campInitialize = M.getOrInsertFunction(
			"plainInitialize",
			Type::getVoidTy(Context),
			(Type*)0);

	campFinalize = M.getOrInsertFunction(
			"plainFinalize",
			Type::getVoidTy(Context),
			(Type*)0);
}

void CAMPPlain::setIniFini(Module& M)
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

bool CAMPPlain::runOnModule(Module& M) {
	setFunctions(M);
	setIniFini(M);

	return false;
}

