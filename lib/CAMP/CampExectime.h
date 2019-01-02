#ifndef LLVM_CORELAB_CAMP_INSTALLER_H
#define LLVM_CORELAB_CAMP_INSTALLER_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

#include "corelab/Utilities/InstInsertPt.h"
#include "corelab/CAMP/campMeta.h"
#include "corelab/CAMP/ContextTreeBuilder.h"
#include "corelab/AliasAnalysis/LoopTraverse.hpp"
#include <fstream>
#include <map>

#ifndef DEBUG_TYPE
  #define DEBUG_TYPE "camp-installer"
#endif


namespace corelab
{
	using namespace llvm;
	using namespace std;

	class CAMPExectime : public ModulePass
	{
		public:
			typedef std::vector<const Instruction *> ExternalCallList;
			typedef std::vector<const Instruction *> IndirectCallList;
			typedef CntxID LoopID;

			bool runOnModule(Module& M);

			// Context Stack approach (baseline)
			//bool runOnFunction(Function *F);
			//bool runOnLoop(Loop *L, FuncID functionId);

			// Context Tree approach
			bool isUseOfGetElementPtrInst(LoadInst *ld);
			void addProfilingCodeForCallSite(Instruction *invokeOrCallInst, Value *locIDVal);
			void addProfilingCodeForLoop(Loop *loop, Value *locIDVal);
			Value *addTargetComparisonCodeForIndCall(const Instruction *invokeOrCallInst, std::vector<std::pair<Function *, LocalContextID>> &targetLocIDs);
			void hookMallocFree();

			// Utility
			Value* castTo(Value* from, Value* to, InstInsertPt &out, const DataLayout *dl);

			virtual void getAnalysisUsage(AnalysisUsage &AU) const
			{
				AU.addRequired< Namer >();
				AU.addRequired< LoopTraverse >();
				AU.addRequired< LoopInfoWrapperPass >();
				AU.addRequired< ContextTreeBuilder >();
				AU.addRequired< LoadNamer >();
				AU.setPreservesAll();
			}

			const char *getPassName() const { return "CAMP"; }

			static char ID;
			CAMPExectime() : ModulePass(ID) {}

			private:
			
			Module *module;
			LoadNamer *pLoadNamer;
			ContextTreeBuilder *cxtTreeBuilder;
			std::vector<ContextTreeNode *> *pCxtTree;
			ExternalCallList externalCalls;
			IndirectCallList indirectCalls;
			

			//### Member variables for Context Tree Approach ###
			//this is given by ContextTreeBuilder pass
			LocIDMapForCallSite *locIdOf_callSite;  // if key is instrID of indirect call, then value is -1
			LocIDMapForIndirectCalls *locIdOf_indCall;
			LocIDMapForLoop *locIdOf_loop;


			/* Functions */

			// initialize, finalize functions
			Constant *campExecInitialize;
			Constant *campExecFinalize;

			// functions for load, store instructions
			//Constant *campLoadInstr;
			//Constant *campStoreInstr;

			// functions for context 
			Constant *campExecCallSiteBegin;
			Constant *campExecCallSiteEnd;
			Constant *campExecLoopBegin;
			Constant *campExecLoopNext;
			Constant *campExecLoopEnd;

			// functio for memory management
			//Constant *campMalloc;
			//Constant *campCalloc;
			//Constant *campRealloc;
			//Constant *campFree;

			Constant *campExecDisableCtxtChange;
			Constant *campExecEnableCtxtChange;


			void setFunctions(Module &M);
			void setIniFini(Module &M);
			bool loadMetadata();

			FuncID getFunctionId(Function &F);
			CntxID getLoopContextId(Loop *L, FuncID functionId);
	};
}

#endif
