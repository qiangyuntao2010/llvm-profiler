#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Argument.h"

#include "corelab/AliasAnalysis/IndirectCallAnal.hpp"
#include "corelab/Utilities/StandardLibraryFunctionsName.h"

#include <list>
#include <algorithm>
#include <unordered_map>

#ifndef DEBUG_TYPE
  #define DEBUG_TYPE "indirect-call-anal"
#endif

using namespace llvm;
using namespace corelab;

char IndirectCallAnal::ID = 0;
static RegisterPass<IndirectCallAnal> X("indirect-call-anal", "indirect call site analysis.", false, true);

//TODO:: where to put this useful function?
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

StringRef getNameOfTarget(const Value *calledVal){
	const Value *stripedVal = calledVal->stripPointerCasts();
	//get Name of Target (primitive way of finding) //TODO:: can we do this smartly?
	if(stripedVal->getName().empty()){
		// errs()<<" inst: ["<< *inst <<"]\n";
		// errs()<<" EMPTY called val?? ["<< *calledVal <<"]\n";
		if(const LoadInst *ldFunPtr = dyn_cast<LoadInst>(calledVal)){
			//assert(ldFunPtr->getPointerOperand()->getName().empty() == false && "ERROR: cannot resolve name of target function by this way");	
			return ldFunPtr->getPointerOperand()->getName();
		}
		else{
			//assert(0 && "ERROR: cannot find where this calledValue are originated from!");
			return stripedVal->getName();//return empth string
		}
	}
	else{
		return stripedVal->getName();
	}
}

bool IndirectCallAnal::runOnModule(Module &M) {
	DEBUG(errs() << "\nSTART [IndirectCallAnal::runOnModule] 	#######\n");
	module = &M;

	assert(( indirectCalls.empty() && callCntOf.empty() ) && "runOnModule must be called twice");//maxDepthOf.clear();
	//externalCalls.clear();
	indirectCalls.clear();
	callCntOf.clear();

	utilities::getStandardLibFunNameList(stdLibFunList);

	collectAllIndirectCalls();

	//printAllIndirectCalls();
	buildTypeBasedMatching();//aggressive matching
	//printMatching(typeBasedMatching); //for debug


	DEBUG(errs() << "\nEND [IndirectCallAnal::runOnModule] 	#######\n");
	return false;
}

void IndirectCallAnal::buildTypeBasedMatching(){
	//first, match based on name
	for(const Instruction *inst : indirectCalls){
		const Value* calledVal = getCalledValueOfIndCall(inst);
		assert(calledVal);

		FunctionType* targetFunType = dyn_cast<FunctionType>(dyn_cast<PointerType>(calledVal->getType())->getElementType());
		assert(targetFunType && "ERROR: not function pointer type");

		StringRef targetName = getNameOfTarget(calledVal);
		//errs()<<"FunType Compare Start for ["<<targetName<<"]\n";

		if (const Function *tarFun = dyn_cast<Function>(calledVal->stripPointerCasts())) {
			if(!tarFun->isDeclaration())
				typeBasedMatching[inst].push_back(const_cast<Function *>(tarFun));//add candidate function!
		}
		else{
			for(Module::iterator fi = module->begin(), fe = module->end(); fi != fe; ++fi) {
				Function &F = *fi;
				if(F.isDeclaration()) continue;
				// Actually, this should be right
				if(isEqual(targetFunType, F.getFunctionType())){					
					if(calledVal->getType() == F.getType())
						typeBasedMatching[inst].push_back(&*fi);//add candidate function!
				}
				// But, I use this.. (Shame on me..)
				// if(targetName == F.getName()){
				// 	typeBasedMatching[inst].push_back(fi);//add candidate function!
				// 	// errs()<<"FunType Compare: "<<"["<<F.getName()<<": "<<*F.getFunctionType() <<"] == ["
				// 	// 	  << targetName <<": "<<*targetFunType<<"]\n";
				// }
			}
		}
		
		//errs()<<"FunType Compare END for ["<<targetName<<"]\n";
	}

	std::list<const Instruction *> notMatched;

	//second, filter out Standard library calls
	for(const Instruction *inst : indirectCalls){
		auto found = typeBasedMatching.find(inst);
		if(found == typeBasedMatching.end()){//not found
			std::string targetName = getNameOfTarget(getCalledValueOfIndCall(inst)).str();
			if(!targetName.empty()){ //has name
				if(stdLibFunList.find(targetName) != stdLibFunList.end())//its std lib fun call
					stdLibFunCalls.push_back(inst);
				else
					notMatched.push_back(inst);
			} 
			else{ //has no name
				notMatched.push_back(inst);
			}
		}
	}

	notMatched.remove_if([this](const Instruction *inst){
		const Value *calledVal = getCalledValueOfIndCall(inst);
		std::string targetName = getNameOfTarget(calledVal).str();
		FunctionType* targetFunType = dyn_cast<FunctionType>(dyn_cast<PointerType>(calledVal->getType())->getElementType());
		assert(targetFunType && "ERROR: not function pointer type..2 ");
		unsigned matchingCnt = 0;
		for(Module::iterator fi = module->begin(), fe = module->end(); fi != fe; ++fi) {
			Function &F = *fi;
			if(F.isDeclaration()) continue;	
			if(isEqual(targetFunType, F.getFunctionType())){
				// errs()<<"FunType Compare: "<<"["<<F.getName()<<": "<<*F.getFunctionType() <<"] == ["
				//  	  << targetName <<": "<<*targetFunType<<"]\n";
				if(calledVal->getType() == F.getType()){
					typeBasedMatching[inst].push_back(&*fi);//add candidate function!
					matchingCnt += 1;
				}
			}
		}
		if(matchingCnt > 0){
			return true; // remove from notMatched.
		}
		else
			return false;
	});

	// errs()<<"List of notMatched Indirect Calls (size: "<<notMatched.size()<<") \n";
	// std::for_each(notMatched.begin(), notMatched.end(), [](const Instruction *i){	errs()<<" ["<<*i<<"]\n";});
	// errs()<<"List of notMatched Indirect Calls END\n";

}

//TODO::make this correctly
bool IndirectCallAnal::isEqual(FunctionType *lhs, FunctionType *rhs){
	if(lhs->getNumParams() != rhs->getNumParams())
		return false;
	if(lhs->getReturnType()->getTypeID() != rhs->getReturnType()->getTypeID())
		return false;

	for (FunctionType::param_iterator I = rhs->param_begin(), E = rhs->param_end(), lhsIt = lhs->param_begin(); I != E; ++I, ++lhsIt){
		Type *rhsArgType = *I;
		Type *lhsArgType = *lhsIt;
		if(lhsArgType->getTypeID() != rhsArgType->getTypeID())
			return false;
	}
	return true;
}

void IndirectCallAnal::collectAllIndirectCalls(){
	for(Module::iterator fi = module->begin(), fe = module->end(); fi != fe; ++fi) {
		Function &F = *fi;
		if(F.isDeclaration()) continue;
		for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I){
			Instruction *inst = &*I;
			Function *callee = NULL;
			if(isa<CallInst>(inst) || isa<InvokeInst>(inst)){
				callee = getCalledFunction_aux(inst);
				if(callee == NULL){ // this means inst is indirect call
					indirectCalls.push_back(inst);

					const Value *calledVal = getCalledValueOfIndCall(inst);
					assert(calledVal);
					const Value *stripedVal = calledVal->stripPointerCasts();
					assert(stripedVal && "ERROR: stripedVal is NULL");
					//assert(stripedVal->getName().empty() == false && "ERROR: stripedVal Name is empty");
					
					std::string targetFunctionName = getNameOfTarget(calledVal).str();
					if(!targetFunctionName.empty()){

						callCntOf[ targetFunctionName ] += 1;
					}


					assert(isa<PointerType>(calledVal->getType()));
					assert(isa<FunctionType>(dyn_cast<PointerType>(calledVal->getType())->getElementType()));
				}
			}
		}
	}
}

void IndirectCallAnal::printMatching(Matching &matching){
	unsigned maxNumTarget=1;

	for(const Instruction *indCall : indirectCalls){
		auto found = matching.find(indCall);
		if(found == matching.end()){//not found
			std::string targetName = getNameOfTarget(getCalledValueOfIndCall(indCall)).str();
			if(stdLibFunList.find(targetName) == stdLibFunList.end())
				errs()<<"["<<*indCall<<"]: NO candidate target!\n";	
		}
		else{
			//errs()<<"["<<*found->first<<"]: "<<found->second.size()<<" \n";
			if(found->second.size() > 1){
				maxNumTarget = std::max((unsigned)(found->second.size()), maxNumTarget);
				//errs()<<"["<<*found->first<<"]: "<<found->second.size()<<" \n";
				//std::for_each(found->second.begin(), found->second.end(), [](Function *f){errs()<<"   "<<f->getName()<<"\n";});
			}
		}
	}

	errs()<<"## Print Matching (size: "<<matching.size()
		  <<"), (size of total indirect calls:"<<indirectCalls.size()
		  <<"), (diff:"<< indirectCalls.size() - matching.size() 
		  <<"), (maxNumTarget: "<<maxNumTarget<<") \n";
}

void IndirectCallAnal::printAllIndirectCalls(){
	errs()<<"List of Indirect Calls (size: "<<indirectCalls.size()<<") \n";
	std::for_each(indirectCalls.begin(), indirectCalls.end(), [](const Instruction *i){	errs()<<" ["<<*i<<"]\n";});
	errs()<<"List of Indirect Calls END\n";
}

void IndirectCallAnal::printAllTargetCandidate(){
	errs()<<"List of Target Candidate Function of Indirect calls (size: "<<callCntOf.size()<<") \n";
	for (auto setIter = callCntOf.begin(), setIterE = callCntOf.end(); setIter != setIterE ; ++setIter){
		errs()<<"["<<setIter->first<<"] :"<<setIter->second<<"\n";
	}
	errs()<<"List of Target Candidate Function of Indirect calls END\n";
}












// if(calledVal->getType() != F.getType()){
// 	errs()<<"2TYPE MISMATCH: \n";
// 	errs()<<" 1 "<<*calledVal<<"\n";
// 	errs()<<" 2 "<<F<<"\n";
// 	errs()<<"TYPE MISMATCH END\n";
// 	assert(0);
// }






// notMatched.remove_if([this](const Instruction *inst){
// 	const Value *calledVal = getCalledValueOfIndCall(inst);
// 	assert(calledVal);
// 	std::string targetName = getNameOfTarget(calledVal).str();

// 	if(targetName.empty()){
// 		FunctionType* targetFunType = dyn_cast<FunctionType>(dyn_cast<PointerType>(calledVal->getType())->getElementType());
// 		assert(targetFunType && "ERROR: not function pointer type..2 ");
// 		unsigned matchingCnt = 0;
// 		for(Module::iterator fi = module->begin(), fe = module->end(); fi != fe; ++fi) {
// 			Function &F = *fi;
// 			if(F.isDeclaration()) continue;	
// 			if(isEqual(targetFunType, F.getFunctionType())){
// 				// errs()<<"FunType Compare: "<<"["<<F.getName()<<": "<<*F.getFunctionType() <<"] == ["
// 				//  	  << targetName <<": "<<*targetFunType<<"]\n";
				
// 				typeBasedMatching[inst].push_back(fi);//add candidate function!
// 				matchingCnt += 1;
// 			}
// 		}
// 		if(matchingCnt > 0)
// 			return true; // remove from notMatched.
// 		else
// 			return false;
// 	}
// 	else{
// 		errs()<<"@@@@ has name:"<<targetName<<"\n";
// 		return false;
// 	}
// });






// errs()<< "matching Size:"<<matching.size()<<"\n";
// for (auto setIter = matching.begin(), setIterE = matching.end(); setIter != setIterE ; ++setIter){
// 	errs()<<"["<<*setIter->first<<"] :\n";
// 	std::for_each(setIter->second.begin(), setIter->second.end(), [](const Function *f){	errs()<<"   ["<<f->getName()<<"]\n";});
// }