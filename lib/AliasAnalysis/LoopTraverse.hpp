#ifndef LLVM_CORELAB_LOOP_TRAVERSE_H
#define LLVM_CORELAB_LOOP_TRAVERSE_H
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"
#include "corelab/AliasAnalysis/RecursiveFuncAnal.hpp"
#include "corelab/AliasAnalysis/IndirectCallAnal.hpp"
#include <unordered_set>
#define UNIMPLEMENTED(s)                     \
do {                                         \
    printf("UNIMPLEMENTED: %s\n", s);        \
    assert(0);                               \
} while (0)


namespace corelab
{
  using namespace llvm;
  class LoopTraverse : public ModulePass {
    public:
      typedef std::vector<const Instruction *> ExternalCallList;
      typedef std::vector<const Instruction *> IndirectCallList;
      typedef std::vector<const Instruction *> RecursiveFunCallList;
      typedef DenseMap<const Function *, unsigned> FunToMaxDepth;
      typedef DenseMap<const Function *, LoopInfo * > LoopInfoOfFunc; 
      //why "DenseMap<const Function *, LoopInfo * >" is impossible to compile?

      typedef const Instruction * IndirectCall;
      typedef std::vector<Function *> CandidateFunctions; 
      typedef std::unordered_map<IndirectCall, CandidateFunctions> IndirectCallMap; // 1:m (one to many matching)

      static char ID;
      LoopTraverse() : ModulePass(ID) {}
      const char *getPassName() const override { return "LoopTraverse"; }
      bool runOnModule(Module &M)  override;

      void getAnalysisUsage(AnalysisUsage &AU) const override {
        AU.addRequired< LoopInfoWrapperPass >();
        AU.addRequired< IndirectCallAnal >();
        AU.addRequired< RecursiveFuncAnal >();
        AU.setPreservesAll();                         // Does not transform code
      }

      //Inter-procedurally Maximum Loop nesting Depth
      void searchMaxLoopDepth();
      unsigned getMaxLoopDepth();
      void printMaxLoopDepthOfFunctions();

    protected:
      unsigned getLoopDepth_aux(Function *func);
      unsigned getLoopDepth(const Loop *loop);
      unsigned testAndTraverse(Instruction *inst);

      Module *module;
      ExternalCallList externalCalls;
      IndirectCallList indirectCallsWithNoTarget;
      RecursiveFunCallList recursiveFunCalls; // call to recursive function
      FunToMaxDepth maxDepthOf;
      std::unordered_set<const Function *> recFuncList;

      IndirectCallMap possibleTargetOf;

      LoopInfoOfFunc loopInfoOf;

  };
}  // End of anonymous namespace

#endif  // LLVM_CORELAB_LOOP_TRAVERSE_H



