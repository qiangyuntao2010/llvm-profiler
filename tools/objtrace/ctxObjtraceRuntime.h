#ifndef CORELAB_CAMP_RUNTIME_H
#define CORELAB_CAMP_RUNTIME_H
#include <inttypes.h>
#include <map>
#include <set>
#include <algorithm>

#define STK_MAX_SIZE 16
#define STK_MAX_SIZE_DIV_BY_8 2

#if STK_MAX_SIZE == 32
typedef uint64_t IterRelation;
#elif STK_MAX_SIZE == 16
typedef uint32_t IterRelation;
#else /* X != 2 and X != 1*/
#endif 

typedef uint16_t CntxID;
typedef uint32_t InstrID;
typedef uint64_t FullID;

typedef std::set<size_t> AllocInfo;
typedef std::set<FullID> LoadStoreInfo;
typedef std::map<FullID, AllocInfo> AllocMap;
typedef std::map<FullID, LoadStoreInfo> LoadStoreMap;


typedef union IterStack{
	uint8_t i8[STK_MAX_SIZE];
	uint64_t i64[STK_MAX_SIZE_DIV_BY_8];
} IterStack;

// Initializer/Finalizer
extern "C" void ctxObjInitialize ();

extern "C" void ctxObjFinalize ();
		
// Memory Event
extern "C" void ctxObjLoadInstr (void* addr, InstrID instrId);
extern "C" void ctxObjStoreInstr (void* addr, InstrID instrId);
		
// Normal Context Event
extern "C" void ctxObjLoopBegin (CntxID cntxID);
extern "C" void ctxObjLoopNext ();
extern "C" void ctxObjLoopEnd (CntxID cntxID);

extern "C" void ctxObjCallSiteBegin (CntxID cntxID);
extern "C" void ctxObjCallSiteEnd  (CntxID cntxID);


extern "C" void* cxtObjMalloc (size_t size, InstrID instrId);
extern "C" void* cxtObjCalloc (size_t num, size_t size, InstrID instrId);
extern "C" void* cxtObjRealloc (void* addr, size_t size, InstrID instrId);

extern "C" void ctxObjDisableCtxtChange();
extern "C" void ctxObjEnableCtxtChange();


#endif
//CORELAB_CAMP_RUNTIME_H
