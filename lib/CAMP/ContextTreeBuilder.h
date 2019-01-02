#ifndef LLVM_CORELAB_CAMP_COTEXT_TREE_BUILDER_H
#define LLVM_CORELAB_CAMP_COTEXT_TREE_BUILDER_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/LoopInfo.h"
#include "corelab/Metadata/LoadNamer.h"
#include "corelab/Metadata/Metadata.h"
#include "corelab/CAMP/campMeta.h"
#include "corelab/AliasAnalysis/RecursiveFuncAnal.hpp"
#include "corelab/AliasAnalysis/IndirectCallAnal.hpp"
#include "llvm/ADT/DenseMap.h"

//#ifndef DEBUG_TYPE
//  #define DEBUG_TYPE "camp-context-tree-builder"
//#endif

#define TREE_MAX_DEPTH_LIMIT 12000


namespace corelab
{
	using namespace llvm;
	using namespace std;
	using namespace corelab::CAMP;

	class ContextTree;
	typedef ContextTree ContextTreeNode;
	typedef uint32_t UniqueContextID;
	typedef uint16_t LocalContextID;

	typedef CntxID CntxIDorInstrID;//whether (CntxID or InstrID) which is given by corealb::LoadNamer.

	//typedef CntxID LoopID;
	typedef std::unordered_map<CntxID, LocalContextID> LocIDMapForLoop; // CntxID == LoopID;

	// typedef std::unordered_map<InstrID, LocalContextID> LocIDMapForCallSite;// if key is instrID of indirect call, then value is -1
	// typedef std::unordered_map<InstrID, std::vector<LocalContextID>> LocIDMapForIndirectCalls;
	//FIXME
	typedef DenseMap<const Instruction *, LocalContextID> LocIDMapForCallSite;// if key is instrID of indirect call, then value is -1
	typedef DenseMap<const Instruction *, std::vector<std::pair<Function *, LocalContextID>>> LocIDMapForIndirectCalls;

	class ContextTree{
		public:
			ContextTree(bool b, ContextTreeNode *p, UniqueContextID ucID_)
				: isCallSite(b), isRecursiveCallSite(false), done(false), ucID(ucID_), parent(p) {
					locID = ucID-(parent != NULL ? parent->ucID : 0);
				}

			bool isCallSite;
			bool isRecursiveCallSite;
			bool done; //for cache grind
			const Instruction *invokeOrCallInst;
			Function *func;//callee
			
			Loop *loop;
			CntxID cntxID;	//for Loop

			UniqueContextID ucID;
			LocalContextID locID;
			inline LocalContextID getLocID(){return locID;}

			//tree
			ContextTree *parent;
			std::vector<ContextTree *> children;

			//get
			inline ContextTree* getParent(){return parent;}
			inline UniqueContextID getUCID(){return ucID;}
			inline LocalContextID getLocalContextID(){return locID;}
			inline std::vector<ContextTree *> *getChildren(){return &children;}
			inline bool isCallSiteNode(){return isCallSite;}
			inline bool isRecursiveCallSiteNode(){return isRecursiveCallSite;}
			inline const Instruction* getCallInst(){return invokeOrCallInst;}
			inline Function* getCallee(){return func;}
			inline const Loop* getLoop(){return loop;}
			inline CntxID getCntxIDforLoop(){return cntxID;}

			void addChild(ContextTreeNode *c){
				children.push_back(c);
			}
			void markRecursive(){
				assert(isCallSite);
				isRecursiveCallSite = true;
			}
			void addCallSiteInfo(const Instruction *invokeOrCallInst_, Function *f, LocIDMapForCallSite &locIdOf_callSite);
			void addLoopInfo(Loop *l, CntxID cntxID_, LocIDMapForLoop &locIdOf_loop);
			unsigned depth();
			unsigned numDuplicationOnPathToRoot(bool isCallSiteCmp, const Instruction *instCmp, CntxID cntxIDCmp);
			void printPathToRoot();
	};
	

	class ContextTreeBuilder : public ModulePass
	{
		public:
			typedef DenseMap<const Function *, LoopInfo * > LoopInfoOfFunc; 
			typedef DenseMap<const Loop *, CntxID > LoopIdOf; // CntxID == LoopID;
			typedef std::map<CntxID, const Loop *> LoopOfCntxID;//inverse of LoopIdOf

			typedef std::vector<const Instruction *> RecursiveFunCallList;
			typedef const Instruction * IndirectCall;
			typedef std::vector<Function *> CandidateFunctions;
			typedef std::unordered_map<IndirectCall, CandidateFunctions> IndirectCallMap; // 1:m (one to many matching)

			bool runOnModule(Module& M);
			bool functionNodeTraverse(ContextTreeNode *callSiteNode);
			bool LoopNodeTraverse(ContextTreeNode *loopNode);
			void recordLoopIDforEachLoop();

			virtual void getAnalysisUsage(AnalysisUsage &AU) const
			{
				AU.addRequired< Namer >();
				AU.addRequired< LoopInfoWrapperPass >();
				AU.addRequired< LoadNamer >();
				AU.addRequired< RecursiveFuncAnal >();
				AU.addRequired< IndirectCallAnal >();
				AU.setPreservesAll();
			}

			const char *getPassName() const { return "CAMP-context-tree-builder"; }

			static char ID;
			ContextTreeBuilder() : ModulePass(ID) { assignerUcID = 0; maxDepth = 0; nCallSiteNode=0; nLoopNode=0;}

			void makeNodeOfCallSite(Instruction *invokeOrCallinst, ContextTreeNode *parent);
			ContextTreeNode * makeNodeOfCallSite_aux(Function *callee, Instruction *invokeOrCallinst, ContextTreeNode *parent);

			void makeNodeOfLoop(Loop *loop, ContextTreeNode *parent);
			
			void printContextTree();
			void printCallsWithNoTarget();
			void contextTreeDumpToFile(std::string path);
			void contextTreeDumpToGvfile(std::string path);

			std::vector<ContextTreeNode *> *getContextTree() {return &cxtTree;}
			ContextTreeNode *queryTreeNodeForCallSite(const Instruction *inst){
				auto found = std::find_if(cxtTree.begin(), cxtTree.end(), [inst](ContextTreeNode *n){return (n->isCallSite && n->invokeOrCallInst == inst);});
				return (found != cxtTree.end()) ? *found : NULL;
			}

			LocIDMapForCallSite *getLocIDMapForCallSite(){return &locIdOf_callSite;}
			LocIDMapForIndirectCalls *getLocIDMapForIndirectCalls(){return &locIdOf_indCall;}
			LocIDMapForLoop *getLocIDMapForLoop(){return &locIdOf_loop;}
			LoopIdOf *getLoopIDMap(){return &loopIdOf;}
			LoopOfCntxID *getLoopMapOfCntxID(){return &loopOfCntxID;}
			LoopInfoOfFunc *getLoopInfo(){ return &loopInfoOf;}

			bool verifyUCID();// if all UCID of tree nodes are unique, then return true;

			UniqueContextID getParentUCID (ContextTreeNode *myNode) 
			{ return cxtTree.at(myNode->getUCID() - myNode->getLocalContextID())->isCallSite ?
				cxtTree.at(myNode->getUCID() - myNode->getLocalContextID())->getUCID() :
				getParentUCID (cxtTree.at(myNode->getUCID() - myNode->getLocalContextID())); }

		private:
			//Analysis Pass info caching
			Module *module;
			LoadNamer *pLoadNamer;
			LoopInfoOfFunc loopInfoOf;
			std::unordered_set<const Function *> recFuncList;
			LoopIdOf loopIdOf;
			LoopOfCntxID loopOfCntxID;
			IndirectCallMap possibleTargetOf;

			std::vector<IndirectCall> callsWithNoTarget;

			UniqueContextID assignerUcID;
			LocIDMapForCallSite locIdOf_callSite;  // if key is instrID of indirect call, then value is -1
			LocIDMapForIndirectCalls locIdOf_indCall;
			LocIDMapForLoop locIdOf_loop;

			RecursiveFunCallList recursiveFunCalls; // call to recursive function
			std::vector<ContextTreeNode *> cxtTree;
			unsigned maxDepth;
			unsigned nCallSiteNode;
			unsigned nLoopNode;
	};
}

#endif
