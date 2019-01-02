// #ifndef LLVM_CORELAB_TEST_AA_H
// #define LLVM_CORELAB_TEST_AA_H
// #include "llvm/Support/Debug.h"
// #include "llvm/Support/raw_ostream.h"
// #include "llvm/Analysis/AliasAnalysis.h"
// #include "llvm/Pass.h"
// #include "llvm/IR/Module.h"

// #include "corelab/Analysis/LoopAA.h"

// #define UNIMPLEMENTED(s)                             \
// do {                                                 \
//     printf("UNIMPLEMENTED: %s\n", s);        \
//     assert(0);                                   \
// } while (0)


// namespace corelab
// {
// 	using namespace llvm;
// 	using namespace corelab;
// 	class AliasAnalysisWrapper;

// 	class AliasInfo {
// 	public:
// 		AliasInfo(AliasResult a) : ar(a) {};
// 		AliasResult ar;
// 	};

// 	//let's make this pure virtual for now.
// 	class AAQueryContext {
// 	public:
// 		AAQueryContext(){};
// 		virtual AliasInfo aliasStrategy(const MemoryLocation &LocA, const MemoryLocation &LocB, AliasAnalysisWrapper &AAWraper) = 0;
// 	};

// 	//Intra-procedural for now// (i.e. USE CFG of LLVM not ICFG)
// 	class FlowSensitiveCxt : public AAQueryContext {
// 	public:
// 		FlowSensitiveCxt(Module &M, Instruction &I): module(M), progPoint(I) {};
// 		AliasInfo aliasStrategy(const MemoryLocation &LocA, const MemoryLocation &LocB, AliasAnalysisWrapper &AAWraper) override;
// 	protected:
// 		Module &module;
// 		Instruction &progPoint; //should be Abstract later..
// 	};

// 	class LoopAwareCxt : public FlowSensitiveCxt {
// 	public:
// 		LoopAwareCxt(Module &M, Instruction &I, Loop &L): FlowSensitiveCxt(M, I), loop(L) {};
// 		AliasInfo aliasStrategy(const MemoryLocation &LocA, const MemoryLocation &LocB, AliasAnalysisWrapper &AAWraper) override;
// 	private:
// 		Loop &loop;
// 	};

// 	class AliasAnalysisWrapper : public ModulePass {
// 	public:
// 		static char ID;
// 		AliasAnalysisWrapper() : ModulePass(ID), AA(nullptr), loopAA(nullptr) {}
// 		const char *getPassName() const { return "AliasAnalysisWrapper"; }
// 		bool runOnModule(Module& M);
// 		void getAnalysisUsage(AnalysisUsage &AU) const;
// 		AliasAnalysis *getAA(){return AA;}
// 		LoopAA *getLoopAA(){return loopAA->getTopAA();}

// 		AliasInfo alias(const MemoryLocation &LocA, const MemoryLocation &LocB, AAQueryContext &qc){
// 			return qc.aliasStrategy(LocA, LocB, *this);
// 		}

// 		AliasInfo alias(const Value *V1, const Value *V2, AAQueryContext &qc){
// 			return alias(MemoryLocation(V1, MemoryLocation::UnknownSize), MemoryLocation(V2, MemoryLocation::UnknownSize), qc);
// 		}
// 	private:
// 		AliasAnalysis *AA;
// 		LoopAA *loopAA;
// 	};
// }  // End of anonymous namespace


// #endif  // LLVM_CORELAB_TEST_AA_H
