#ifndef LLVM_CORELAB_CTX_OBJ_H
#define LLVM_CORELAB_CTX_OBJ_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

#include "corelab/Utilities/InstInsertPt.h"
#include "corelab/CAMP/campMeta.h"
#include "corelab/CAMP/ContextTreeBuilder.h"
#include "corelab/AliasAnalysis/LoopTraverse.hpp"
#include <fstream>
#include <map>


namespace corelab
{
	using namespace llvm;
	using namespace std;

	typedef uint64_t FullID;

	class CtxObjtrace  : public ModulePass
	{
		public:
			typedef std::vector<const Instruction *> ExternalCallList;
			typedef std::vector<const Instruction *> IndirectCallList;
			typedef CntxID LoopID;

			bool runOnModule(Module& M);

			// Context Tree approach
			bool isUseOfGetElementPtrInst(LoadInst *ld);
			void addProfilingCodeForCallSite(Instruction *invokeOrCallInst, Value *locIDVal);
			void addProfilingCodeForLoop(Loop *loop, Value *locIDVal);
			Value *addTargetComparisonCodeForIndCall(const Instruction *invokeOrCallInst, std::vector<std::pair<Function *, LocalContextID>> &targetLocIDs);

			// Utility
			Value* castTo(Value* from, Value* to, InstInsertPt &out, const DataLayout *dl);

			virtual void getAnalysisUsage(AnalysisUsage &AU) const
			{
				AU.addRequired< Namer >();
				AU.addRequired< ContextTreeBuilder >();
				AU.setPreservesAll();
			}

			const char *getPassName() const { return "Cxt-Objtrace"; }

			static char ID;
			CtxObjtrace() : ModulePass(ID) {}

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
			Constant *ctxObjInitialize;
			Constant *ctxObjFinalize;

			// functions for context 
			Constant *ctxObjCallSiteBegin;
			Constant *ctxObjCallSiteEnd;
			Constant *ctxObjLoopBegin;
			Constant *ctxObjLoopNext;
			Constant *ctxObjLoopEnd;

			Constant *ctxObjDisableCtxtChange;
			Constant *ctxObjEnableCtxtChange;

			Constant *ctxObjLoadInstr;
      Constant *ctxObjStoreInstr;

      Constant *ctxObjMalloc;
      Constant *ctxObjCalloc;
      Constant *ctxObjRealloc;


			void setFunctions(Module &M);
			void setIniFini(Module &M);
			bool loadMetadata();
			void hookMallocFree();
      
			void makeMetadata(Instruction* Instruction, uint64_t id);

			FuncID getFunctionId(Function &F);
			CntxID getLoopContextId(Loop *L, FuncID functionId);
	};
}

#endif
