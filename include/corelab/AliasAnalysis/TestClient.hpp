#ifndef LLVM_CORELAB_TEST_AA_CLIENT_H
#define LLVM_CORELAB_TEST_AA_CLIENT_H
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
// #include "corelab/Analysis/LoopAA.h"
#include "corelab/AliasAnalysis/AAQueryContext.hpp"
#include "corelab/ICFG/ICFGBuilder.hpp"
#include "corelab/AliasAnalysis/LoopTraverse.hpp"

#ifndef DEBUG_TYPE
  #define DEBUG_TYPE "test-aa-client"
#endif

#define SET std::set
#define UNIMPLEMENTED(s)                             \
do {                                                 \
    printf("UNIMPLEMENTED: %s\n", s);				 \
    assert(0);                               		 \
} while (0)

namespace corelab 
{	
	using namespace llvm;
	class TestAAClient : public ModulePass
	{
		public:
			static char ID;
			TestAAClient() : ModulePass(ID) {}
			StringRef getPassName() const { return "TestAAClient"; }

			bool runOnModule(Module& M);
			void getAnalysisUsage(AnalysisUsage &AU) const {
			    //AliasAnalysis::getAnalysisUsage(AU)
			    AU.addRequired< LoopInfoWrapperPass >();
			    // AU.addRequired< LoopTraverse >();
			    // AU.addRequired< RecursiveFuncAnal >();
			    // AU.addRequired< ICFGBuilder >();
			    // AU.addRequired< AliasAnalysisWrapper >();
			    // AU.addRequired< AliasAnalysis >();
			    //AU.addRequired< LoopAA >();
			    
			    // AU.setPreservesAll();
			}

		protected:
			std::vector<Value *> ptrInModule;
	};
}

#endif  // LLVM_CORELAB_TEST_AA_CLIENT_H
