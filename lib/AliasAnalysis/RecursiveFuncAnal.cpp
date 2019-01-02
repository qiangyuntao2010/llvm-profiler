#include "corelab/AliasAnalysis/RecursiveFuncAnal.hpp"
#include <algorithm>
#include <climits>
#ifndef DEBUG_TYPE
  #define DEBUG_TYPE "recursive-func-anal"
#endif

using namespace llvm;
using namespace corelab;

#define DO_NOTHING 0

char RecursiveFuncAnal::ID = 0;
static RegisterPass<RecursiveFuncAnal> X("recursive-func-anal", "Find all recursive functions and analyze them.", false, false);

const Value *getCalledValueOfIndCall(const Instruction* indCall);
Function *getCalledFunction_aux(Instruction* indCall);

//TODO: to make it more formal and acurate, exploit Call Graph and so that general mututally recursive call can be covered
bool RecursiveFuncAnal::runOnModule(Module &M) {
	DEBUG(errs() << "\nSTART [RecursiveFuncAnal::runOnModule] 	#######\n");
	module = &M;

	assert( (recFuncList.empty() && nonRecFuncList.empty()) && "runOnModule must be called twice");
	recursiveCalls.clear();
	recFuncList.clear();
	nonRecFuncList.clear();
	callHistory.clear();

	IndirectCallAnal &indCallAnal = getAnalysis<IndirectCallAnal>();
	indCallAnal.getTypeBasedMatching(possibleTargetOf);

	findAllRecursiveFunction();
	DEBUG(errs() << "\nEND [RecursiveFuncAnal::runOnModule] 	#######\n");
	return false;
}

void RecursiveFuncAnal::traverseAndMarkRecCalls(Function *curFun){
	//errs()<<"("<<curFun->getName();
	auto impossible = std::find(callHistory.begin(), callHistory.end(), curFun);
	assert(impossible == callHistory.end());
	callHistory.push_back(curFun);

	Function &F = *curFun;
	for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I){
		Instruction *inst = &*I;
		if(isa<CallInst>(inst) || isa<InvokeInst>(inst)){
			Function *callee = getCalledFunction_aux(inst); //defined in IndirectCallAnal.cpp
			if(!callee){ //indirect call
				//indirectCalls.push_back(call);
				int numTargetCandidate = possibleTargetOf[inst].size();

				assert(possibleTargetOf.end() != possibleTargetOf.find(inst) && "ERROR: NOT FOUND in IndirectCallAnal pass!!");

				if(numTargetCandidate == 0){
					callsWithNoTarget.push_back(inst);
				}
				else{
					//multiple target candidate
					//handle as if parent sequentially call every cadidate by this indirect call.

					// //for debug
					// errs()<<"multiple target candidate case [";
					// for_each(possibleTargetOf[inst].begin(), possibleTargetOf[inst].end(), [](Function *f){errs()<<"("<<f->getName()<<"), ";});
					// errs()<<"]\n";

					for(Function *oneOfCallees : possibleTargetOf[inst]){
						if(oneOfCallees->isDeclaration() == false
						   && recFuncList.find(oneOfCallees) == recFuncList.end() 
						   && nonRecFuncList.find(oneOfCallees) == nonRecFuncList.end()){
							bool hasDuplicationInCallHistory = checkCallHistoryAndMark(inst, oneOfCallees);
							if(!hasDuplicationInCallHistory)
					    		traverseAndMarkRecCalls(oneOfCallees);	
						}
					}
				}
			}
			else if (callee->isDeclaration()) //external function definition
				DO_NOTHING;//externalCalls.push_back(call);
			else if (recFuncList.find(callee) != recFuncList.end())
				DO_NOTHING;
			else if (nonRecFuncList.find(callee) != nonRecFuncList.end())
				DO_NOTHING;
			else{
				bool hasDuplicationInCallHistory = checkCallHistoryAndMark(inst, callee);
				if(!hasDuplicationInCallHistory)
		    		traverseAndMarkRecCalls(callee);
		    }
		}
	}
	if (recFuncList.find(curFun) == recFuncList.end())
			nonRecFuncList.insert(curFun);
	assert(callHistory.back() == curFun && "ERROR: back of callHistory are corrupted!");
	callHistory.pop_back();
	//errs()<<")";
}

//return true if it found recursive call(duplication of call) in Call History
bool RecursiveFuncAnal::checkCallHistoryAndMark(Instruction *invokeOrCallInst, Function *callee){
	auto chIt = std::find(callHistory.begin(), callHistory.end(), callee);
	bool found = (chIt != callHistory.end());
	if(found){
		DEBUG(errs()<< callee->getName() <<" $$$$$ Rec ["<< *invokeOrCallInst <<" ] \n");
		recursiveCalls.push_back(invokeOrCallInst);//printCallHistory();
		//recFuncList.insert(callee);
		std::for_each(chIt, callHistory.end(), [this](const Function *f){recFuncList.insert(f);});

	}
	return found;
}

void RecursiveFuncAnal::findAllRecursiveFunction()
{
	for(Module::iterator fi = module->begin(), fe = module->end(); fi != fe; ++fi) {
		Function &F = *fi;
		if (F.isDeclaration()) continue;
		callHistory.clear();
		traverseAndMarkRecCalls(&*fi);
	}
}

void RecursiveFuncAnal::printAllRecursiveFunction(){
	errs()<<"List of Recursive Functions (size: "<<recFuncList.size()<<") \n";
	std::for_each(recFuncList.begin(), recFuncList.end(), [](const Function *f){errs()<<" ["<<f->getName()<<"], ";});
	errs()<<"\nList of Recursive Functions END\n";
}

void RecursiveFuncAnal::printAllTrueRecursiveCall(){
	errs()<<"List of Recursive Calls\n";
	std::for_each(recursiveCalls.begin(), recursiveCalls.end(), [](const Instruction *i){errs()<<" ["<<*i<<"]\n";});
	errs()<<"List of Recursive Calls END\n";
}

void RecursiveFuncAnal::printCallHistory(){
	errs()<<"[=== Call History ===]\n";
	std::for_each(callHistory.begin(), callHistory.end(), [](const Function *f){errs()<<" ["<<f->getName()<<"]\n";});
	errs()<<"[=== Call History === END]\n";
}



