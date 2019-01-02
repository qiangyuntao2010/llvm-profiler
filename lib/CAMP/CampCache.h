#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "corelab/CAMP/campMeta.h"
#include "corelab/CAMP/ContextTreeBuilder.h"
#include "corelab/AliasAnalysis/LoopTraverse.hpp"
#include <fstream>
#include <map>


namespace corelab
{
	using namespace llvm;
	using namespace std;
	


	class CAMPCache: public ModulePass
	{
		public:

			bool runOnModule(Module& M);

			virtual void getAnalysisUsage(AnalysisUsage &AU) const
			{
				//AU.addRequiredID(createLoopExtractorPasis()->getPassID());
				AU.addRequired<ContextTreeBuilder>();
				AU.setPreservesAll();
			}

			const char *getPassName() const { return "CAMP"; }

			static char ID;
			CAMPCache() : ModulePass(ID) {}

			bool recurDuplicateFunction ( ContextTreeNode *node, int iterator, Instruction* targetInst);
			UniqueContextID getParentUCID ( ContextTreeNode *node );
			private:
			
			Module *module;
			ContextTreeBuilder *cxtTreeBuilder;
			std::vector<ContextTreeNode*> *pCxtTree;
			LocIDMapForCallSite *locIdOf_callSite;
			LocIDMapForIndirectCalls *locIdOf_indCall;
			
			int Iterator;
			//std::vector<ContextTreeNode*>::iterator Iterator;

	};
}

