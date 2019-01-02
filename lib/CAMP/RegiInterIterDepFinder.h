#ifndef LLVM_CORELAB_REGI_INTER_ITER_DEP_FINDER_H
#define LLVM_CORELAB_REGI_INTER_ITER_DEP_FINDER_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

#include "corelab/CAMP/ContextTreeBuilder.h"

#ifndef DEBUG_TYPE
  #define DEBUG_TYPE "regi-inter-iter-dep-finder"
#endif


namespace corelab
{
	using namespace llvm;
	using namespace std;
	using namespace corelab::CAMP;

	typedef CntxID LoopID;
	//typedef uint16_t LoopID;
	typedef std::map<LoopID, unsigned> NumLoopCarriedDepMap;//inverse of LoopIdOf

	class RegiInterIterDepFinder : public ModulePass
	{
		public:

			bool runOnModule(Module& M);

			virtual void getAnalysisUsage(AnalysisUsage &AU) const
			{
				AU.addRequired< ContextTreeBuilder >();
				AU.addRequired< LoopInfoWrapperPass >();
				AU.setPreservesAll();
			}

			const char *getPassName() const { return "regi-inter-iter-dep-finder"; }

			static char ID;
			RegiInterIterDepFinder() : ModulePass(ID) {}



			void printResult(std::string path);

		private:
			
			Module *module;
			ContextTreeBuilder *cxtTreeBuilder;
			ContextTreeBuilder::LoopIdOf *loopIdOf;
			ContextTreeBuilder::LoopOfCntxID *loopOfLoopID;

			NumLoopCarriedDepMap nLoopCarriedDepMap;

			bool isIndVar(Value *inVal, std::vector<Value *> orginalVal, const Loop *loop);
	};
}

#endif
