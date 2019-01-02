#include <stdio.h>
#include <iostream>
#include <fstream>
#include <assert.h>

#include "objtraceruntime.h"
#include "ShadowMemory.hpp"
#include "x86timer.hpp"

AllocMap *allocIdMap;
LoadStoreMap *loadIdMap;
LoadStoreMap *storeIdMap;
uint64_t lowest_heap;
x86timer t;

void shadowMemorySetting (void *addr, size_t size, FullID fullId){

//	assert ((uint64_t)addr < 0x0000400000000000);
	uint64_t flipAddr = GET_SHADOW_ADDR_MALLOC_MAP(addr);

	for (unsigned int i = 0; i < (size-1)/8 + 1; i++)
	{	
		*( (uint64_t *)(flipAddr + (uint64_t)(i*8)) ) = fullId;
	}
}

void lowestHeapSetting (uint64_t addr){
	if ( lowest_heap > addr )
		lowest_heap = addr;
}

extern "C"
void objTraceInitialize () {
	ShadowMemoryManager::initialize();
	//DEBUG("Obj Trace :: ShadowMemory Initialize Complete\n");
	allocIdMap = new AllocMap();
	loadIdMap = new LoadStoreMap();
	storeIdMap = new LoadStoreMap();
	lowest_heap = 0x0000000040000000;
	t.start();
}

extern "C"
void objTraceFinalize () {
	
	std::ofstream outfile("object_trace.data", std::ios::out | std::ofstream::binary);

	if(outfile.is_open()){
		outfile<<"1.Allocation_Info\n";
		
		//alloc
		for (auto e1 : *allocIdMap)
		{
			outfile<<"Alloc_ID:"<<e1.first<<"\n";
			for (auto e2 : e1.second)
			{
				outfile<<"size:"<<e2<<"\n";
			}
		}	
		outfile<<"\n";

		outfile<<"2.Load_Info\n";
		//Load
		for (auto e3 : *loadIdMap)
		{
			outfile<<"Load_Instr_ID:"<<e3.first<<"\n";
			for (auto e4 : e3.second)
			{
				outfile<<"Object_ID:"<<e4<<"\n";
			}
		}
		outfile<<"\n";

		outfile<<"3.Store_Info\n";
		//Store
		for (auto e3 : *storeIdMap)
		{
			outfile<<"Store_Instr_ID:"<<e3.first<<"\n";
			for (auto e4 : e3.second)
			{
				outfile<<"Object_ID:"<<e4<<"\n";
			}
		}

	outfile << "overhead : " << t.stop() * (1.0e-9);

	}
	outfile.close();
}

extern "C"
void objTraceLoadInstr (void* addr, FullID fullId) {

	if(/* 0x0000000040000000 < (uint64_t)addr &&*/ (uint64_t)addr < 0x0000400000000000 )//TODO :: how to handle data seg access && where is the exact start address of heap space. 
	{
		//DEBUG("access %zx\n", (uint64_t)addr);
		uint64_t flipAddr = GET_SHADOW_ADDR_MALLOC_MAP(addr);
		//DEBUG("get flip ADDR %zx\n", (uint64_t)flipAddr);
		FullID fullAllocId = *( (uint64_t *)(flipAddr) );
		//DEBUG(" here?? %lu\n", fullAllocId);
		if (fullAllocId > 0 )
		{
			LoadStoreMap::iterator it = loadIdMap->find(fullId);
			if (it==loadIdMap->end())
			{
				LoadStoreInfo loadSet;
				loadSet.insert (fullAllocId);
				loadIdMap->insert( std::pair<FullID, LoadStoreInfo> (fullId, loadSet));
			}
			else
			{
				(it->second).insert( fullAllocId );
			}
		}
/*
		AllocMap::iterator allocIt = allocIdMap->find(fullAllocId);

		if ( allocIt != allocIdMap->end() )
		{
			LoadStoreMap::iterator it = loadIdMap->find(fullId);
			if( it == loadIdMap->end() ) 
			{
				LoadStoreInfo loadSet;
				loadSet.insert( std::pair<const uint64_t, FullID> ((uint64_t)addr, fullAllocId) );
				loadIdMap->insert( std::pair<FullID, LoadStoreInfo> ( fullId, loadSet ) );
			}
			else
			{
				(it->second).insert( std::pair<const uint64_t, FullID> ((uint64_t)addr, fullAllocId) );
			}
		}
*/
	}
	
}

extern "C"
void objTraceStoreInstr (void* addr, FullID fullId) {

	if(/*(uint64_t)addr > 0x0000000040000000 &&*/ (uint64_t)addr < 0x0000400000000000 )//TODO :: how to handle data seg access
	{
		uint64_t flipAddr = GET_SHADOW_ADDR_MALLOC_MAP(addr);
		FullID fullAllocId = *( (uint64_t *)(flipAddr) );

		if (fullAllocId > 0 )
		{
			LoadStoreMap::iterator it = storeIdMap->find(fullId);
			if (it==storeIdMap->end())
			{
				LoadStoreInfo storeSet;
				storeSet.insert (fullAllocId);
				storeIdMap->insert( std::pair<FullID, LoadStoreInfo> (fullId, storeSet));
			}
			else
			{
				(it->second).insert( fullAllocId );
			}
		}

/*
		AllocMap::iterator allocIt = allocIdMap->find(fullAllocId);

		if ( allocIt != allocIdMap->end() )
		{
			LoadStoreMap::iterator it = storeIdMap->find(fullId);
			if( it == storeIdMap->end() ) 
			{
				LoadStoreInfo storeSet;
				storeSet.insert( std::pair<const uint64_t, FullID> ((uint64_t)addr, fullAllocId) );
				storeIdMap->insert( std::pair<FullID, LoadStoreInfo> ( fullId, storeSet ) );
			}
			else
			{
				(it->second).insert( std::pair<const uint64_t, FullID> ((uint64_t)addr, fullAllocId) );
			}
		}
*/
	}

}

extern "C" void*
objTraceMalloc (size_t size, FullID fullId){

	void *addr = malloc (size);
	const uint64_t addr_ = (uint64_t)addr;

	AllocMap::iterator it = allocIdMap->find(fullId);
	if( it == allocIdMap->end() ) // first alloc by this instruction
	{
		AllocInfo allocSet;
		//allocSet.insert( std::pair<const uint64_t, size_t> (addr_, size) );
		allocSet.insert(size);
		allocIdMap->insert( std::pair<FullID, AllocInfo> ( fullId, allocSet ) );
	}
	else
	{
		//DEBUG("twice alloc by same ID (%lu)\n", fullId);
		//(it->second).insert( std::pair<uint64_t, size_t> (addr_, size) );
		(it->second).insert(size);
	}
	
	shadowMemorySetting ( addr, size, fullId );

	return addr;
}

extern "C" void*
objTraceCalloc (size_t num, size_t size, FullID fullId){
  void* addr = calloc (num, size);
	const uint64_t addr_ = (uint64_t)addr;

	AllocMap::iterator it = allocIdMap->find(fullId);
	if( it == allocIdMap->end() )
	{
		AllocInfo allocSet;
		//allocSet.insert( std::pair<const uint64_t, size_t> (addr_, (size*num)) );
		allocSet.insert(size*num);
		allocIdMap->insert( std::pair<FullID, AllocInfo> (fullId, allocSet) );
	}
	else
	{
		//DEBUG("twice alloc by same ID (%lu)\n", fullId);
		//(it->second).insert( std::pair<uint64_t, size_t> (addr_, (size*num)) );
		(it->second).insert(size*num);
	}

	shadowMemorySetting ( addr, (size*num), fullId );

  return addr;
}

extern "C" void*
objTraceRealloc (void* addr, size_t size, FullID fullId){
  void* naddr = NULL;
  naddr = realloc (addr, size);
	const uint64_t addr_ = (uint64_t)naddr;

	AllocMap::iterator it = allocIdMap->find(fullId);
	if( it == allocIdMap->end() )
	{
		AllocInfo allocSet;
		//allocSet.insert( std::pair<const uint64_t, size_t> (addr_, size) );
		allocSet.insert(size);
		allocIdMap->insert( std::pair<FullID, AllocInfo> (fullId, allocSet) );
	}
	else
	{
		//DEBUG("twice alloc by same ID (%lu)\n", fullId);
		//(it->second).insert( std::pair<uint64_t, size_t> (addr_, size) );
		(it->second).insert(size);
	}

	shadowMemorySetting ( naddr, size, fullId );

  return naddr;
}

extern "C" void
objTraceFree (void* addr, FullID fullId){
  free (addr);
}
