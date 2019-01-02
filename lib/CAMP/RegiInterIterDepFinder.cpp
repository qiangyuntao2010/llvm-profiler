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

#include "RegiInterIterDepFinder.h"

#include <iostream>
#include <fstream>
#include <algorithm>    // std::for_each
#include <iterator>

using namespace corelab;
//using namespace corelab::RegiInterIterDepFinder;

char RegiInterIterDepFinder::ID = 0;
static RegisterPass<RegiInterIterDepFinder> X("regi-inter-iter-dep-finder", "Find Inter Iteration dependence by register.", false, false);


bool RegiInterIterDepFinder::runOnModule(Module& M) {
	module = &M;
	DEBUG(errs()<<"############## runOnModule [RegiInterIterDepFinder] START ##############\n");
	cxtTreeBuilder = &getAnalysis< ContextTreeBuilder >();
	loopIdOf = cxtTreeBuilder->getLoopIDMap();
	loopOfLoopID = cxtTreeBuilder->getLoopMapOfCntxID();

	errs()<<" # of static Loop: "<<loopOfLoopID->size()<<", \n";
	//ensure that all loop are simplfied.
	for(std::pair<CntxID, const Loop *> loopID : *loopOfLoopID) assert(loopID.second->isLoopSimplifyForm());

	unsigned nNoCanonIndvarL = 0;
	
	for(std::pair<CntxID, const Loop *> loopID : *loopOfLoopID){
		PHINode *canonIndVar = loopID.second->getCanonicalInductionVariable();
		unsigned nPhiNode = 0;
		unsigned nLoopCarriedDep = 0;
		if(!canonIndVar)
			nNoCanonIndvarL++;

		BasicBlock *header = loopID.second->getHeader();
		for(BasicBlock::iterator ii=header->begin(), ie=header->end(); ii!=ie; ++ii){
			if(isa<PHINode>(ii) == false) break;
			nPhiNode++;

			PHINode *phi = dyn_cast<PHINode>(ii);
			if(phi == canonIndVar) continue;
			assert(phi);
			assert(phi->getNumIncomingValues() == 2);
			bool isFstOperIn = loopID.second->contains(phi->getIncomingBlock(0));
			bool isSndOperIn = loopID.second->contains(phi->getIncomingBlock(1));
			assert((isFstOperIn && isSndOperIn) == false);
			assert((isFstOperIn || isSndOperIn) == true);

			Value *inValue = isFstOperIn ? phi->getIncomingValue(0) : phi->getIncomingValue(1);
			
			if(loopID.second->isLoopInvariant(inValue) == false){
				std::vector<Value *> orginalValVec;
				orginalValVec.push_back(inValue);
				if(isIndVar(inValue, orginalValVec, loopID.second) == false){
					nLoopCarriedDepMap[loopID.first] += 1;
					nLoopCarriedDep++;
					// errs()<<"[["<<*inValue<<"]]\n";
				}
			}
			
		}

		// if(nPhiNode != 0){
		// 	if(canonIndVar==NULL)
		// 		errs()<<"Loop ID: "<<loopID.first<< " NO CAN" << ", # phi: "<<nPhiNode<<", nLoopCarriedDep: "<<nLoopCarriedDep<<"\n";
		// 	else
		// 		errs()<<"Loop ID: "<<loopID.first<< "YESCAN["<<*canonIndVar<< "], # phi: "<<nPhiNode<<", nLoopCarriedDep: "<<nLoopCarriedDep<<"\n";
		// }
	}

	// errs()<<"# of NoCanonIndvarL:"<<nNoCanonIndvarL<<", # of canonIndVar:"<<loopOfLoopID->size()-nNoCanonIndvarL<<"\n";
	errs()<<"# of nNonDOALLable Loop:"<<nLoopCarriedDepMap.size()<<"\n";
	// unsigned nNonDOALLable = 0;
	// for(auto p : nLoopCarriedDepMap){
	// 	if(p.second>0){
	// 		// errs()<<"LoopID :"<<p.first<<", nLoopCarriedDep:"<<p.second<<"\n";
	// 		nNonDOALLable++;
	// 	}
	// }
	// errs()<<"# of nNonDOALLable: "<<nNonDOALLable<<"\n";

	printResult(std::string("nNonDOALLableLoopByRegi.data"));
	
	DEBUG(errs()<<"############## runOnModule [RegiInterIterDepFinder] END ##############\n");
	return false;
}

bool RegiInterIterDepFinder::isIndVar(Value *inVal, std::vector<Value *> orginalVal, const Loop *loop){
	assert(orginalVal.size()<60);
	if(orginalVal.size() > 50){
		// errs()<<"TOO BIG["<<*inVal<<"]\n";
		// errs()<<*loop<<"\n";
		// std::for_each(orginalVal.begin(), orginalVal.end(), [](Value *v){errs()<<*v<<"\n";});
		// errs()<<"\n";
		assert(0&&"TOO BIG...");
		return false;
	}

	if(loop->isLoopInvariant(inVal))
		return false;
	if(loop->getCanonicalInductionVariable() == inVal)
		return true;

	if(PHINode *phi = dyn_cast<PHINode>(inVal)){
		// errs()<<"PHI["<<*inVal<<"]";
		// std::for_each(orginalVal.begin(), orginalVal.end(), [](Value *v){errs()<<v->getName()<<", ";});
		// errs()<<"\n";

		if(loop->getHeader() != phi->getParent()) return false;
		assert(phi->getNumIncomingValues() == 2);

		bool isFstOperIn = loop->contains(phi->getIncomingBlock(0));
		bool isSndOperIn = loop->contains(phi->getIncomingBlock(1));
		assert((isFstOperIn || isSndOperIn) == true && "WTF???");
		if((isFstOperIn && isSndOperIn))
			return false;
		if(isFstOperIn){
			if(orginalVal.end() != std::find(orginalVal.begin(), orginalVal.end(), phi->getIncomingValue(0)))return true;
			else{
				orginalVal.push_back(phi->getIncomingValue(0));
				bool res = isIndVar(phi->getIncomingValue(0), orginalVal, loop);
				assert(orginalVal.back() == phi->getIncomingValue(0));
				orginalVal.pop_back();
				return res;
			}
		}
		if(isSndOperIn){
			if(orginalVal.end() != std::find(orginalVal.begin(), orginalVal.end(), phi->getIncomingValue(1))) return true;
			else{
				orginalVal.push_back(phi->getIncomingValue(1));
				bool res = isIndVar(phi->getIncomingValue(1), orginalVal, loop);
				assert(orginalVal.back() == phi->getIncomingValue(1));
				orginalVal.pop_back();
				return res;
			}
		}
	}
	
	if(BinaryOperator *binOp = dyn_cast<BinaryOperator>(inVal)){
		// errs()<<"BinaryOperator: ["<<*binOp<<"]\n";
		assert(binOp->getNumOperands() == 2);
		// errs()<<"BIN["<<*inVal<<"]\n";
		
		if(binOp->getOpcode() == Instruction::Add){
		}
		else if(binOp->getOpcode() == Instruction::FAdd){
		}
		else if(binOp->getOpcode() == Instruction::Sub){
		}
		else if(binOp->getOpcode() == Instruction::FSub){
		}
		else if(binOp->getOpcode() == Instruction::Mul){
		}
		else if(binOp->getOpcode() == Instruction::FMul){
		}
		else if(binOp->getOpcode() == Instruction::Shl){
		}
		else{
			return false;
		}
		
		bool isFstInvari = loop->isLoopInvariant(binOp->getOperand(0));
		bool isFstIndVar = false; 
		if(!isFstInvari){
			orginalVal.push_back(binOp->getOperand(0));
			isFstIndVar = isIndVar(binOp->getOperand(0), orginalVal , loop);
			assert(orginalVal.back() == binOp->getOperand(0));
			orginalVal.pop_back();
		}
		
		bool isSndInvari = loop->isLoopInvariant(binOp->getOperand(1));
		bool isSndIndVar = false;
		if(!isSndIndVar){
			orginalVal.push_back(binOp->getOperand(1));
			isSndIndVar = isIndVar(binOp->getOperand(1), orginalVal , loop);
			assert(orginalVal.back() == binOp->getOperand(1));
			orginalVal.pop_back();
		}
		
		// errs()<<"!!==="<<*inVal<<"] ("<<isFstIndVar<<", "<<isFstInvari<<")"<<", ("<<isSndIndVar<<", "<<isSndInvari<<")\n";

		if(!(isFstIndVar || isFstInvari))
			return false;
		if(!(isSndIndVar || isSndInvari))
			return false;

		if(isFstIndVar && isSndIndVar){
			// errs()<<"INVAR!! 1 TRUE:"<<*inVal<<"\n";
			return true;
		}
		if((isFstIndVar && isSndInvari)||(isFstInvari && isSndIndVar)){
			// errs()<<"INVAR!! 2 TRUE:"<<*inVal<<"\n";
			return true;
		}
		if(isFstInvari && isSndInvari)
			return false;
	}

	if(CastInst *castOp = dyn_cast<CastInst>(inVal)){
		assert(castOp->getNumOperands() == 1);
		if(loop->isLoopInvariant(castOp->getOperand(0)))
			return false;
		orginalVal.push_back(castOp->getOperand(0));
		bool res = isIndVar(castOp->getOperand(0), orginalVal , loop);
		assert(orginalVal.back() == castOp->getOperand(0));
		orginalVal.pop_back();
		return res;
	}

	return false;
}


void RegiInterIterDepFinder::printResult(std::string path){
	std::ofstream outfile(path, std::ios::out | std::ofstream::binary);

	if(outfile.is_open()){
		outfile<<"$$$$$ NON DOALLABLE BY REGISTER $$$$$\n";
		outfile<<nLoopCarriedDepMap.size()<<"\n";
		//std::copy (cxtTree.begin(), cxtTree.end(),std::ostream_iterator<ContextTreeNode>(outfile, "\n"));
		for ( auto it = nLoopCarriedDepMap.begin(); it != nLoopCarriedDepMap.end(); ++it ){
			outfile<<it->first<<"\n";
			// outfile<<it->first<<":"<<it->second<<"\n";
		}
		outfile<<"$$$$$ NON DOALLABLE BY REGISTER END $$$$$\n";
	}
	outfile.close();


}