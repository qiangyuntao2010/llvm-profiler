#ifndef CORELAB_CAMP_RUNTIME_H
#define CORELAB_CAMP_RUNTIME_H
#include <inttypes.h>
#include <unordered_map>
#include <map>

#define STK_MAX_SIZE 16
#define STK_MAX_SIZE_DIV_BY_8 2

#if STK_MAX_SIZE == 32
typedef uint64_t IterRelation;
#elif STK_MAX_SIZE == 16
typedef uint32_t IterRelation;
#else /* X != 2 and X != 1*/
#endif 

typedef uint16_t CntxID;
typedef uint16_t InstrID;
typedef uint32_t CampID;
typedef uint64_t DepID;

typedef union IterStack{
	uint8_t i8[STK_MAX_SIZE];
	uint64_t i64[STK_MAX_SIZE_DIV_BY_8];
} IterStack;

// for Load History Element
typedef std::map<CampID, IterStack> LoadHistoryMap;

// for Store History Element
typedef struct StoreHistoryElem {
	CampID campID;
	uint32_t padding; //4byte padding to make StoreHistoryElem 24 byte
	IterStack iterStack;
} StoreHistoryElem;

typedef struct HistoryElem {
	LoadHistoryMap *pLoadMap;
	StoreHistoryElem storeElem;
} HistoryElem;

#define SAMPLING_THRESHOLD 8 //AFTER THIS MUCH TIME.. IT DOES NOTHING
typedef std::map<CampID, uint8_t> CampIDCounterMap; //for sampling

// Initializer/Finalizer
extern "C" void campInitialize (size_t ldrCnt, size_t strCnt, size_t callCnt,
		size_t loopCnt, size_t maxLoopDepth);

extern "C" void campFinalize ();
		
// Memory Event
extern "C" void campLoadInstr (void* addr, InstrID instrID);
extern "C" void campStoreInstr (void* addr, InstrID instrID);
		
// Normal Context Event
extern "C" void campLoopBegin (CntxID cntxID);
extern "C" void campLoopNext ();
extern "C" void campLoopEnd (CntxID cntxID);

extern "C" void campCallSiteBegin (CntxID cntxID);
extern "C" void campCallSiteEnd  (CntxID cntxID);


extern "C" void* campMalloc (size_t size);
extern "C" void* campCalloc (size_t num, size_t size);
extern "C" void* campRealloc (void* addr, size_t size);
extern "C" void campFree (void* addr);

extern "C" void campDisableCtxtChange();
extern "C" void campEnableCtxtChange();

// For debug
// extern "C" void dumpStoreHistoryTable();
// extern "C" void dumpLoadHistoryTable();
extern "C" void dumpDependenceTable();



#endif
//CORELAB_CAMP_RUNTIME_H
