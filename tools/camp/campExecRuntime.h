#ifndef CORELAB_CAMP_RUNTIME_H
#define CORELAB_CAMP_RUNTIME_H
#include <inttypes.h>
#include <unordered_map>
#include <map>

typedef uint16_t CntxID;
typedef uint16_t InstrID;
typedef uint32_t CampID;
typedef uint64_t DepID;

// Initializer/Finalizer
extern "C" void campExecInitialize (size_t ldrCnt, size_t strCnt, size_t callCnt,
		size_t loopCnt, size_t maxLoopDepth);

extern "C" void campExecFinalize (bool removeLoop);
		
// Normal Context Event
extern "C" void campExecLoopBegin (CntxID cntxID);
extern "C" void campExecLoopNext ();
extern "C" void campExecLoopEnd (CntxID cntxID);

extern "C" void campExecCallSiteBegin (CntxID cntxID);
extern "C" void campExecCallSiteEnd  (CntxID cntxID);

extern "C" void campExecDisableCtxtChange();
extern "C" void campExecEnableCtxtChange();


#endif
//CORELAB_CAMP_RUNTIME_H
