#ifndef LLVM_CORELAB_CAMP_INSTALLER_H
#define LLVM_CORELAB_CAMP_INSTALLER_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

namespace corelab
{
	using namespace llvm;
	using namespace std;

	class CAMPPlain : public ModulePass
	{
		public:

			bool runOnModule(Module& M);

			virtual void getAnalysisUsage(AnalysisUsage &AU) const
			{
				AU.setPreservesAll();
			}

			const char *getPassName() const { return "CAMP_Plain"; }

			static char ID;
			CAMPPlain() : ModulePass(ID) {}

			Constant *campInitialize;
			Constant *campFinalize;

			void setFunctions(Module &M);
			void setIniFini(Module &M);


	};
}

#endif
