#ifndef __FIND_FUNCTION_EXIT_BB_HPP
#define __FIND_FUNCTION_EXIT_BB_HPP

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"

#include <vector>
#include <unordered_map>


using namespace llvm;

namespace corelab {
	struct FindFunctionExitBB: public ModulePass{
		public:
			typedef std::pair<llvm::ReturnInst *, std::vector<UnreachableInst *>> ExitBBs;
			typedef std::unordered_map<Function *, ExitBBs> ExitBBMap;

			static char ID;
			FindFunctionExitBB(): ModulePass(ID) { }
			virtual bool runOnModule(Module &M);

			virtual void getAnalysisUsage(AnalysisUsage &AU) const {
				AU.setPreservesAll();
			}

			BasicBlock *getExitBBOfFunctionWithNoRetInst(Function *fun);
			ExitBBs &getExitBB(Function *fun);
			void transform(Function *f);

		private:
			void findExitBBs(Function *fun);
			ExitBBMap exitBBof;
	};
}

#endif //__FIND_FUNCTION_EXIT_BB_HPP