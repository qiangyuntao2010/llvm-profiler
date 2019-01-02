#ifndef LLVM_CORELAB_INDIRECT_CALL_ANAL_H
#define LLVM_CORELAB_INDIRECT_CALL_ANAL_H
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/DerivedTypes.h"
#include <unordered_map>
#include <map>
#include <unordered_set>
#define UNIMPLEMENTED(s)                     \
do {                                         \
    printf("UNIMPLEMENTED: %s\n", s);        \
    assert(0);                               \
} while (0)


namespace corelab
{
	using namespace llvm;
	class IndirectCallAnal : public ModulePass {
		public:
			//typedef std::vector<const Instruction *> ExternalCallList;
			//TODO:qyt note IndirectCall is a type of Instruction
			typedef const Instruction * IndirectCall;
			typedef std::vector<IndirectCall> IndirectCallList;
			typedef std::string FunctionName; //Still, i can't trust llvm::StringRef
			typedef std::map<FunctionName, unsigned> CallCountOf;//for Statistics
			typedef std::vector<Function *> CandidateFunctions;
			typedef std::unordered_map<IndirectCall, CandidateFunctions> Matching; // 1:m (one to many matching)


			static char ID;
			IndirectCallAnal() : ModulePass(ID) {}
			const char *getPassName() const override { return "Indirect Call Analysis"; }
			bool runOnModule(Module &M)  override;

			void getAnalysisUsage(AnalysisUsage &AU) const override {
				AU.setPreservesAll(); // Does not transform code
			}

			void collectAllIndirectCalls();
			
			void buildTypeBasedMatching(); //matched by Function prototype (Function return type, and formal argument types)
			bool isEqual(FunctionType *lhs, FunctionType *rhs);//TODO:: move this functionality to other pass (this is not what this ought to do)

			void getTypeBasedMatching(Matching& matching){matching = typeBasedMatching;}

			void printAllIndirectCalls();
			void printAllTargetCandidate(); // print all candidiate target functions of Indirect calls, And it's Call count.
			void printMatching(Matching &matching);

		protected:
			Module *module;
			//ExternalCallList externalCalls;
			IndirectCallList indirectCalls;
			CallCountOf callCntOf; //for Statistics, # of indirect call count for eeach target Candidate functions.	
			std::unordered_set<std::string> stdLibFunList;

			// Matching indirect calls to Possible Target Functions 
			Matching typeBasedMatching; //matched by Function prototype (Function return type, and formal argument types)

			IndirectCallList stdLibFunCalls;
			

	};		
}  // End of anonymous namespace

#endif  // LLVM_CORELAB_INDIRECT_CALL_ANAL_H



