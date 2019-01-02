#ifndef LLVM_CORELAB_RECURSIVE_FUNC_ANAL_H
#define LLVM_CORELAB_RECURSIVE_FUNC_ANAL_H
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"
#include "corelab/AliasAnalysis/IndirectCallAnal.hpp"
#include <unordered_set>
#include <unordered_map>
#define UNIMPLEMENTED(s)                     \
do {                                         \
    printf("UNIMPLEMENTED: %s\n", s);        \
    assert(0);                               \
} while (0)


namespace corelab
{
  using namespace llvm;
  //TODO: to make it more formal and acurate, exploit Call Graph and so that general mututally recursive call can be covered
  class RecursiveFuncAnal : public ModulePass {
    public:
      typedef std::vector<const Instruction *> RecursiveCallList;
      typedef std::unordered_set<const Function *> RecursiveFuncList;
      typedef std::unordered_set<const Function *> NonRecursiveFuncList;
      typedef std::vector<const Function *> CallStack; //To fine recursive funcall

      //typedef for IndrectCallAnal
      typedef const Instruction * IndirectCall;
      typedef std::vector<Function *> CandidateFunctions;
      typedef std::unordered_map<IndirectCall, CandidateFunctions> IndirectCallMap; // 1:m (one to many matching)

      typedef RecursiveFuncList::iterator iterator;
      typedef RecursiveFuncList::const_iterator const_iterator;

      typedef RecursiveCallList::iterator reccall_iterator;
      typedef RecursiveCallList::const_iterator const_reccall_iterator;

      static char ID;
      RecursiveFuncAnal() : ModulePass(ID) {}
      const char *getPassName() const override { return "Recursive Function Analysis"; }
      bool runOnModule(Module &M)  override;

      void getAnalysisUsage(AnalysisUsage &AU) const override {
        AU.addRequired< IndirectCallAnal >();
        AU.setPreservesAll();                         // Does not transform code
      }

      //aux function!! DO NOT USE OUTSIDE
      void traverseAndMarkRecCalls(Function *curFun);
      bool checkCallHistoryAndMark(Instruction *invokeOrCallInst, Function *callee);

      //print result
      void printAllRecursiveFunction();
      void printAllTrueRecursiveCall(); // True Recursive Call is call Instruction to itself in same function
                                        // So We can decide a function is recursive by the help of this call.
    protected:
      void findAllRecursiveFunction();//TODO: should be in different class
      void printCallHistory(); //For Debugging

      Module *module;
      IndirectCallMap possibleTargetOf;
      std::vector<IndirectCall> callsWithNoTarget;

      RecursiveCallList recursiveCalls;
      RecursiveFuncList recFuncList;
      NonRecursiveFuncList nonRecFuncList;
      CallStack callHistory; //mutually recursive functions may be represented as one recursive function

    public:
      const RecursiveFuncList &getRecursiveFuncList() const { return recFuncList; }
      RecursiveFuncList &getRecursiveFuncList()       { return recFuncList; }
      iterator                begin()       { return recFuncList.begin(); }
      const_iterator          begin() const { return recFuncList.begin(); }
      iterator                end  ()       { return recFuncList.end();   }
      const_iterator          end  () const { return recFuncList.end();   }
      size_t                  size () const { return recFuncList.size();  }
      bool                    empty() const { return recFuncList.empty(); }

      const RecursiveCallList &getTrueRecursiveCallList() const { return recursiveCalls; }
      RecursiveCallList &getTrueRecursiveCallList()       { return recursiveCalls; }
  };
}  // End of anonymous namespace

#endif  // LLVM_CORELAB_RECURSIVE_FUNC_ANAL_H



