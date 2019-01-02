#include <stdio.h>
#include <iostream>
#include <malloc.h>
#include <math.h>
#include <fstream> //to file IO
#include <bitset> //to output binary format
#include <iomanip> //to output hex format
#include <cstring>

#include "ctxObjtraceRuntime.h"
#include "ShadowMemory.hpp"
#include "x86timer.hpp"

static IterStack globalIterStack;
static int globalIterStackIdx;
static int globalIterStackIdx_overflow;
static uint32_t currentCtx;

#define MAX_LINE_SIZE 128

static unsigned nestingDepth;
static uint16_t disableCxtChange; // enabled when 0

AllocMap *ctxAllocIdMap;
LoadStoreMap *ctxLoadIdMap;
LoadStoreMap *ctxStoreIdMap;
x86timer ctx_t;

#define getFullId(instrID) ((uint64_t)currentCtx << 32)|((uint64_t)instrID & 0x00000000FFFFFFFF)
#define getCtxId(fullId) (uint32_t)(fullId >> 32)
#define getInstrId(fullId) (uint32_t)(fullId & 0x00000000FFFFFFFF)
#define GET_BLOCK_ID(instrID) (uint16_t)(instrID >> 16 )
#define GET_INSTR_ID(instrID) (uint16_t)(instrID & 0x0000FFFF)

void ctxObjShadowMemorySetting (void *addr, size_t size, FullID fullId){

//	assert ((uint64_t)addr < 0x0000400000000000);
	uint64_t flipAddr = GET_SHADOW_ADDR_MALLOC_MAP(addr);

	for (unsigned int i = 0; i < (size-1)/8 + 1; i++)
	{	
		*( (uint64_t *)(flipAddr + (uint64_t)(i*8)) ) = fullId;
	}
}

// Initializer/Finalizer
extern "C"
void ctxObjInitialize () {

	globalIterStackIdx=-1; // null
	globalIterStackIdx_overflow = -1;
	disableCxtChange = 0;
	nestingDepth=0;
	currentCtx = 0;
	for (int i = 0; i < STK_MAX_SIZE_DIV_BY_8; ++i)
		globalIterStack.i64[i] = 0;

	//memory setting
	ShadowMemoryManager::initialize();
	ctxAllocIdMap = new AllocMap();
	ctxLoadIdMap = new LoadStoreMap();
	ctxStoreIdMap = new LoadStoreMap();

	ctx_t.start();
}


extern "C"
void ctxObjFinalize () {
	std::ofstream outfile("ctx_object_trace.data", std::ios::out | std::ofstream::binary);

	if(outfile.is_open()){
		outfile<<"1.Allocation_Info\n";
		
		//alloc
		for (auto e1 : *ctxAllocIdMap)
		{
			outfile<<"Alloc_ID:"<<getCtxId(e1.first)<<"_"<<GET_BLOCK_ID(getInstrId(e1.first))<<"_"<<GET_INSTR_ID(getInstrId(e1.first))<<"\n";
			for (auto e2 : e1.second)
			{
				outfile<<"size:"<<e2<<"\n";
			}
		}	
		outfile<<"\n";

		outfile<<"2.Load_Info\n";
		//Load
		for (auto e3 : *ctxLoadIdMap)
		{
			outfile<<"Load_Instr_ID:"<<getCtxId(e3.first)<<"_"<<GET_BLOCK_ID(getInstrId(e3.first))<<"_"<<GET_INSTR_ID(getInstrId(e3.first))<<"\n";
			for (auto e4 : e3.second)
			{
				outfile<<"Object_ID:"<<getCtxId(e4)<<"_"<<GET_BLOCK_ID(getInstrId(e4))<<"_"<<GET_INSTR_ID(getInstrId(e4))<<"\n";
			}
		}
		outfile<<"\n";

		outfile<<"3.Store_Info\n";
		//Store
		for (auto e3 : *ctxStoreIdMap)
		{
			outfile<<"Store_Instr_ID:"<<getCtxId(e3.first)<<"_"<<GET_BLOCK_ID(getInstrId(e3.first))<<"_"<<GET_INSTR_ID(getInstrId(e3.first))<<"\n";
			for (auto e4 : e3.second)
			{
				outfile<<"Object_ID:"<<getCtxId(e4)<<"_"<<GET_BLOCK_ID(getInstrId(e4))<<"_"<<GET_INSTR_ID(getInstrId(e4))<<"\n";
			}
		}

	outfile << "overhead : " << ctx_t.stop() * (1.0e-9);
	}
	outfile.close();

}

// Normal Context Event
extern "C"
void ctxObjLoopBegin (CntxID cntxID) { //arg is LocId
	if(disableCxtChange != 0) return;

	currentCtx += cntxID;

	if(globalIterStackIdx >= STK_MAX_SIZE-1)
		globalIterStackIdx_overflow++;
	else{
		globalIterStackIdx++;
		globalIterStack.i8[globalIterStackIdx] = 0;
	}
}

extern "C"
void ctxObjLoopNext () {
	if(disableCxtChange != 0) return;
	globalIterStack.i8[globalIterStackIdx] = (globalIterStack.i8[globalIterStackIdx]+1) & 0x7f;  // 0111 1111b
}

extern "C"
void ctxObjLoopEnd (CntxID cntxID) {
	if(disableCxtChange!= 0) return;

	currentCtx -= cntxID;

	if(globalIterStackIdx_overflow > -1)
		globalIterStackIdx_overflow--;
	else{
		globalIterStack.i8[globalIterStackIdx] = 0;
		globalIterStackIdx--;
	}

}

extern "C"
void ctxObjCallSiteBegin (CntxID cntxID) {
	if(disableCxtChange == 0)
	currentCtx += cntxID;	
}

extern "C"
void ctxObjCallSiteEnd  (CntxID cntxID) {
	if(disableCxtChange == 0)
        	currentCtx -= cntxID;
}

extern "C" void ctxObjDisableCtxtChange(){
	disableCxtChange++;
}

extern "C" void ctxObjEnableCtxtChange(){
	disableCxtChange--;
}


extern "C"
void ctxObjLoadInstr (void* addr, InstrID instrId) {

	if(/* 0x0000000040000000 < (uint64_t)addr &&*/ (uint64_t)addr < 0x0000400000000000 )//TODO :: how to handle data seg access && where is the exact start address of heap space. 
	{
		uint64_t flipAddr = GET_SHADOW_ADDR_MALLOC_MAP(addr);
		FullID fullAllocId = *( (uint64_t *)(flipAddr) );
		if (fullAllocId > 0 )
		{
			FullID fullId = getFullId(instrId);
			LoadStoreMap::iterator it = ctxLoadIdMap->find(fullId);
			if (it==ctxLoadIdMap->end())
			{
				LoadStoreInfo loadSet;
				loadSet.insert (fullAllocId);
				ctxLoadIdMap->insert( std::pair<FullID, LoadStoreInfo> (fullId, loadSet));
			}
			else
			{
				(it->second).insert( fullAllocId );
			}
		}
	}	
}

extern "C"
void ctxObjStoreInstr (void* addr, InstrID instrId) {

	if(/*(uint64_t)addr > 0x0000000040000000 &&*/ (uint64_t)addr < 0x0000400000000000 )//TODO :: how to handle data seg access
	{
		uint64_t flipAddr = GET_SHADOW_ADDR_MALLOC_MAP(addr);
		FullID fullAllocId = *( (uint64_t *)(flipAddr) );

		if (fullAllocId > 0 )
		{
			FullID fullId = getFullId(instrId);
			LoadStoreMap::iterator it = ctxStoreIdMap->find(fullId);
			if (it==ctxStoreIdMap->end())
			{
				LoadStoreInfo storeSet;
				storeSet.insert (fullAllocId);
				ctxStoreIdMap->insert( std::pair<FullID, LoadStoreInfo> (fullId, storeSet));
			}
			else
			{
				(it->second).insert( fullAllocId );
			}
		}

	}

}

extern "C" 
void* ctxObjMalloc (size_t size, InstrID instrId){

	void *addr = malloc (size);
	const uint64_t addr_ = (uint64_t)addr;
	FullID fullId = getFullId(instrId);

	AllocMap::iterator it = ctxAllocIdMap->find(fullId);
	if( it == ctxAllocIdMap->end() ) // first alloc by this instruction
	{
		AllocInfo allocSet;
		allocSet.insert(size);
		ctxAllocIdMap->insert( std::pair<FullID, AllocInfo> ( fullId, allocSet ) );
	}
	else
	{
		(it->second).insert(size);
	}
	
	ctxObjShadowMemorySetting ( addr, size, fullId );

	return addr;
}

extern "C" 
void* ctxObjCalloc (size_t num, size_t size, InstrID instrId){
  void* addr = calloc (num, size);
	const uint64_t addr_ = (uint64_t)addr;
	FullID fullId = getFullId(instrId);

	AllocMap::iterator it = ctxAllocIdMap->find(fullId);
	if( it == ctxAllocIdMap->end() )
	{
		AllocInfo allocSet;
		allocSet.insert(size*num);
		ctxAllocIdMap->insert( std::pair<FullID, AllocInfo> (fullId, allocSet) );
	}
	else
	{
		(it->second).insert(size*num);
	}

	ctxObjShadowMemorySetting ( addr, (size*num), fullId );

  return addr;
}

extern "C" 
void* ctxObjRealloc (void* addr, size_t size, InstrID instrId){
  void* naddr = NULL;
  naddr = realloc (addr, size);
	const uint64_t addr_ = (uint64_t)naddr;
	FullID fullId = getFullId(instrId);

	AllocMap::iterator it = ctxAllocIdMap->find(fullId);
	if( it == ctxAllocIdMap->end() )
	{
		AllocInfo allocSet;
		allocSet.insert(size);
		ctxAllocIdMap->insert( std::pair<FullID, AllocInfo> (fullId, allocSet) );
	}
	else
	{
		(it->second).insert(size);
	}

	ctxObjShadowMemorySetting ( naddr, size, fullId );

  return naddr;
}


