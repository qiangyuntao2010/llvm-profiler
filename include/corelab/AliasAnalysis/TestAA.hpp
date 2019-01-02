// #ifndef LLVM_CORELAB_TEST_AA_H
// #define LLVM_CORELAB_TEST_AA_H
// #include "llvm/Pass.h"
// #include "llvm/IR/Module.h"
// #include "llvm/ADT/SCCIterator.h"
// #include "llvm/ADT/DenseSet.h"
// #include "llvm/Analysis/CallGraph.h"
// #include "llvm/Support/Debug.h"
// #include "llvm/IR/InstIterator.h"
// #include "llvm/Support/raw_ostream.h"

// #include "corelab/Analysis/ClassicLoopAA.h"
// #include "corelab/Analysis/LoopAA.h"

// #define UNIMPLEMENTED(s)                             \
// do {                                                 \
//     printf("UNIMPLEMENTED: %s\n", s);        \
//     assert(0);                                   \
// } while (0)


// namespace corelab
// {
//   using namespace llvm;
//   class TestAA : public ModulePass, public corelab::ClassicLoopAA {
//     private:

//     public:
//       static char ID;
//       TestAA() : ModulePass(ID) {}
//       bool runOnModule(Module &M);

//       virtual AliasResult aliasCheck(const Pointer &P1, TemporalRelation Rel, const Pointer &P2, const Loop *L);

//       const char *getLoopAAName() const {
//         return "test-aa";
//       }

//       void getAnalysisUsage(AnalysisUsage &AU) const {
//         LoopAA::getAnalysisUsage(AU);
//         //AU.addRequired<CallGraphWrapperPass>();
//         AU.setPreservesAll();                         // Does not transform code
//       }

//       /// getAdjustedAnalysisPointer - This method is used when a pass implements
//       /// an analysis interface through multiple inheritance.  If needed, it
//       /// should override this to adjust the this pointer as needed for the
//       /// specified pass info.
//       virtual void *getAdjustedAnalysisPointer(AnalysisID PI) {
//         if (PI == &LoopAA::ID)
//           return (LoopAA*)this;
//         return this;
//       }
//   };
// }  // End of anonymous namespace

// #endif  // LLVM_CORELAB_TEST_AA_H








//   // class TestAA : public ModulePass, public corelab::ClassicLoopAA {
//   //   public:
//   //     static char ID; // Class identification, replacement for typeinfo
//   //     TestAA() : ModulePass(ID) {
//   //       //initializeTestAAPass(*PassRegistry::getPassRegistry());
//   //     }
//   //     const char *getPassName() const override { return "TestAA"; }
//   //     bool runOnModule(Module& M) override;
//   //     void getAnalysisUsage(AnalysisUsage &AU) const override {
//   //       AliasAnalysis::getAnalysisUsage(AU);
//   //       AU.addRequired<AliasAnalysis>();
//   //       AU.setPreservesAll();
//   //     }

//   //     AliasResult alias(const MemoryLocation &LocA,
//   //                       const MemoryLocation &LocB) override {
//   //       errs()<<"alias query\n";
//   //       return MayAlias;
//   //     }

//   //     FunctionModRefBehavior getModRefBehavior(ImmutableCallSite CS) override {
//   //       return FMRB_UnknownModRefBehavior;
//   //     }
//   //     FunctionModRefBehavior getModRefBehavior(const Function *F) override {
//   //       return FMRB_UnknownModRefBehavior;
//   //     }

//   //     bool pointsToConstantMemory(const MemoryLocation &Loc,
//   //                                 bool OrLocal) override {
//   //       return false;
//   //     }
//   //     ModRefInfo getArgModRefInfo(ImmutableCallSite CS,
//   //                                 unsigned ArgIdx) override {
//   //       return MRI_ModRef;
//   //     }

//   //     ModRefInfo getModRefInfo(ImmutableCallSite CS,
//   //                              const MemoryLocation &Loc) override {
//   //       return MRI_ModRef;
//   //     }
//   //     ModRefInfo getModRefInfo(ImmutableCallSite CS1,
//   //                              ImmutableCallSite CS2) override {
//   //       return MRI_ModRef;
//   //     }

//   //     /// getAdjustedAnalysisPointer - This method is used when a pass implements
//   //     /// an analysis interface through multiple inheritance.  If needed, it
//   //     /// should override this to adjust the this pointer as needed for the
//   //     /// specified pass info.
//   //     void *getAdjustedAnalysisPointer(const void *ID) override {
//   //       if (ID == &AliasAnalysis::ID)
//   //         return (AliasAnalysis*)this;
//   //       return this;
//   //     }
//   // };