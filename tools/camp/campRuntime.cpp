#include <stdio.h>
#include <iostream>
#include <malloc.h>
#include <unordered_map>

#include <fstream> //to file IO
#include <bitset> //to output binary format
#include <iomanip> //to output hex format
#include <cstring>

#include "campRuntime.h"
#include "ShadowMemory.hpp"

struct sigaction ShadowMemoryManager::segvAction; //Page Fault hooking mechanism

static IterStack globalIterStack;
static int globalIterStackIdx;
static int globalIterStackIdx_overflow;
static CntxID currentCtx;

// Dependence Table
// [Src::Dest] => IterRelation
//static std::unordered_map<DepID, IterRelation> depTable;
static std::unordered_map<DepID, IterRelation> *depTable=NULL;

// static std::unordered_map<DepID, unsigned> *depCnt=NULL; //for Debug 525

static uint16_t disableCxtChange; // enabled when 0

#define isIntra(iter1, iter2) ( ((iter1 & 0x7f) == (iter2 & 0x7f)) | (iter1 & 0x80) | (iter2 & 0x80) )
#define isInter(iter1, iter2) ( ((iter1 & 0x7f) ^ (iter2 & 0x7f)) | (iter1 & 0x80) | (iter2 & 0x80) )

// Initializer/Finalizer
extern "C"
void campInitialize (size_t ldrCnt, size_t strCnt, size_t callCnt, size_t loopCnt, size_t maxLoopDepth) {
	disableCxtChange = 0;
	ShadowMemoryManager::initialize();
	globalIterStackIdx=-1; // null
	globalIterStackIdx_overflow = -1;
	currentCtx = 0;
	for (int i = 0; i < STK_MAX_SIZE_DIV_BY_8; ++i)
		globalIterStack.i64[i] = 0;
	printf("campInitialize!! maxLoopDepth: %zu, sizeof StoreHistoryElem %lu, sizeof IterStack %lu \n", maxLoopDepth, sizeof(StoreHistoryElem), sizeof(IterStack));
	printf("sizeof void* %lu,  IterRelation size:%lu, CampID %lu \n", sizeof(void*), sizeof(IterRelation), sizeof(CampID));
	depTable = new std::unordered_map<DepID, IterRelation>;

	// depCnt = new std::unordered_map<DepID, unsigned>; //for Debug 525
}


extern "C"
void campFinalize () {
	fprintf(stderr, "[full campFinalize]nDeps: %lu\n", depTable->size());//for Debug 525
	dumpDependenceTable();
	delete depTable;

	// uint64_t cntDep=0;//for Debug 525
	// uint32_t maxDup=0;//for Debug 525
	// for(auto e : *depCnt){//for Debug 525
		// CampID srcCampID = (CampID)((e.first>>32) & 0xffffffff);//for Debug 525
		// CampID dstCampID = (CampID)((e.first) & 0x00000000ffffffff);//for Debug 525
		// fprintf(stderr, "RESULT DEPENDENCE IN MAP: %d, UCIDsrc: %d ,InstrIDsrc: %d ==> UCIDdst: %d ,InstrIDdst: %d  \n",//for Debug 525
			// e.second, (CntxID)((srcCampID>>16)&0xffff), (InstrID)((srcCampID)&0xffff), (CntxID)((dstCampID>>16)&0xffff), (InstrID)((dstCampID)&0xffff));//for Debug 525
		// fprintf(stderr, "%d\n", e.second);//for Debug 525
		// cntDep += e.second;//for Debug 525
		// maxDup = maxDup < e.second ? e.second : maxDup;//for Debug 525
	// }//for Debug 525
	// fprintf(stderr, "maxDup:%d, cntDep:%lu \n", maxDup, cntDep);//for Debug 525
}

// Normal Context Event
extern "C"
void campLoopBegin (CntxID cntxID) {
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
void campLoopNext () {
	if(disableCxtChange != 0) return;
	globalIterStack.i8[globalIterStackIdx] = (globalIterStack.i8[globalIterStackIdx]+1) & 0x7f;  // 0111 1111b
}

extern "C"
void campLoopEnd (CntxID cntxID) {
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
void campCallSiteBegin (CntxID cntxID) {
	if(disableCxtChange == 0)
	currentCtx += cntxID;
}

extern "C"
void campCallSiteEnd  (CntxID cntxID) {
	if(disableCxtChange == 0)
	currentCtx -= cntxID;
}

static void insertDep(CampID srcCampID, CampID dstCampID, uint8_t* oldIter){
	DepID depId = (((DepID) srcCampID) << 32) | ((DepID) dstCampID) ;
	IterRelation depIter = 0;

	// fprintf(stderr, "_ DEPENDENCE : %lu, UCIDsrc: %d ,InstrIDsrc: %d ==> UCIDdst: %d ,InstrIDdst: %d  \n",
	// depId, (CntxID)((srcCampID>>16)&0xffff), (InstrID)((srcCampID)&0xffff), (CntxID)((dstCampID>>16)&0xffff), (InstrID)((dstCampID)&0xffff));

	// [ iter32 inter intra, iter31 inter intra ... iter0 inter intra ]
	for(int i=0;i<=globalIterStackIdx;i++){
		if(isIntra(oldIter[i], globalIterStack.i8[i])) {
			depIter |= (1<<(i*2));
		}
		if(isInter(oldIter[i], globalIterStack.i8[i])) {
			depIter |= (1<<(i*2+1));
		}
	}

	if((*depTable).find(depId) != (*depTable).end()) {
		(*depTable)[depId] |= depIter; 
		// (*depCnt)[depId]++;//for Debug 525
	} else {
		// fprintf(stderr, "NEW DEPENDENCE ADD: %lu, UCIDsrc: %d ,InstrIDsrc: %d ==> UCIDdst: %d ,InstrIDdst: %d  \n",
		// 	depId, (CntxID)((srcCampID>>16)&0xffff), (InstrID)((srcCampID)&0xffff), (CntxID)((dstCampID>>16)&0xffff), (InstrID)((dstCampID)&0xffff));
		(*depTable)[depId] = depIter;  

		// (*depCnt)[depId]++;//for Debug 525
	}

}

// Memory Event
extern "C"
void campLoadInstr (void* addr, InstrID instrID) {
	CampID campID = currentCtx << 16 | instrID;

	HistoryElem *pHisElem = (HistoryElem *)(GET_SHADOW_ADDR_HISTORY_TB(addr));
	// fprintf(stderr, "LD %p ==> %p\n", addr, (void*)pHisElem);

	if(pHisElem->pLoadMap == NULL) //because we memset(0) for newly allocated page.
		pHisElem->pLoadMap = new LoadHistoryMap;
	LoadHistoryMap &loadHistoryMap = *pHisElem->pLoadMap;

	// update dependence table (shadow memory version)
	if(pHisElem->storeElem.campID != 0)
		insertDep(pHisElem->storeElem.campID, campID, pHisElem->storeElem.iterStack.i8);

	// add history table (shadow memory version)
	if(loadHistoryMap.find(campID) != loadHistoryMap.end()) {
		for(int i=0;i<=globalIterStackIdx;i++)
			if(loadHistoryMap[campID].i8[i] != globalIterStack.i8[i]) 
				loadHistoryMap[campID].i8[i] = globalIterStack.i8[i] | 0x80;
	} 
	else 
		loadHistoryMap[campID] = globalIterStack;

	// dumpLoadHistoryTable();
	// dumpDependenceTable();
}
 
extern "C"
void campStoreInstr (void* addr, InstrID instrID) {
	CampID campID = currentCtx << 16 | instrID;	
	HistoryElem *pHisElem = (HistoryElem *)(GET_SHADOW_ADDR_HISTORY_TB(addr));
	// fprintf(stderr, "ST %p ==> %p\n", addr, (void*)pHisElem);

	// update dependence table (shadow memory version)
	if(pHisElem->pLoadMap != NULL){
		LoadHistoryMap &loadHistoryMap = *pHisElem->pLoadMap;
		for (LoadHistoryMap::iterator ii=loadHistoryMap.begin(); ii!=loadHistoryMap.end(); ++ii)
			insertDep(ii->first, campID, ii->second.i8);
	}
	if(pHisElem->storeElem.campID != 0){
		insertDep(pHisElem->storeElem.campID, campID, pHisElem->storeElem.iterStack.i8);
	}
	

	// update store history table (shadow memory version)
	pHisElem->storeElem.campID = campID;
	pHisElem->storeElem.iterStack = globalIterStack;


	// remove load history element (shadow memory version)
	if(pHisElem->pLoadMap != NULL)
		pHisElem->pLoadMap->clear();

	// dumpStoreHistoryTable();
	// dumpDependenceTable();
}


extern "C" void*
campMalloc (size_t size){
	void* addr = malloc (size);
	size_t *pElemInMallocMap = (size_t *)GET_SHADOW_ADDR_MALLOC_MAP(addr);
	*pElemInMallocMap = size;
	return addr;
}

extern "C" void*
campCalloc (size_t size, size_t num){
	void* addr = calloc (size, num);
	size_t *pElemInMallocMap = (size_t *)GET_SHADOW_ADDR_MALLOC_MAP(addr);
	*pElemInMallocMap = size * num;
	return addr;
}

extern "C" void*
campRealloc (void* addr, size_t size){
	void* paddr = addr;
	void* naddr = NULL;

	size_t *pElemInMallocMap = (size_t *)GET_SHADOW_ADDR_MALLOC_MAP(paddr);
	size_t pSize = *pElemInMallocMap;

	//free
	HistoryElem *pHisElem = (HistoryElem *)(GET_SHADOW_ADDR_HISTORY_TB(paddr));
	for (size_t i = 0; i < pSize; ++i, pHisElem = pHisElem+1){
		if(pHisElem->pLoadMap != NULL)
			pHisElem->pLoadMap->clear();
	}
	HistoryElem *pHisElemStart = (HistoryElem *)(GET_SHADOW_ADDR_HISTORY_TB(paddr));
	memset ((void *)pHisElemStart, 0, SIZE_ELEM * pSize);
	*pElemInMallocMap = 0;

	naddr = realloc (paddr, size);

	size_t *pNewElemInMallocMap = (size_t *)GET_SHADOW_ADDR_MALLOC_MAP(naddr);
	*pNewElemInMallocMap = size;
	return naddr;
}

extern "C" void
campFree (void* addr){
	free (addr);

	size_t *pElemInMallocMap = (size_t *)GET_SHADOW_ADDR_MALLOC_MAP(addr);
	size_t pSize = *pElemInMallocMap;

	HistoryElem *pHisElem = (HistoryElem *)(GET_SHADOW_ADDR_HISTORY_TB(addr));
	for (size_t i = 0; i < pSize; ++i, pHisElem = pHisElem+1){
		if(pHisElem->pLoadMap != NULL)
			pHisElem->pLoadMap->clear();
	}
	HistoryElem *pHisElemStart = (HistoryElem *)(GET_SHADOW_ADDR_HISTORY_TB(addr));
	memset ((void *)pHisElemStart, 0, SIZE_ELEM * pSize);

	*pElemInMallocMap = 0;
}

extern "C" void campDisableCtxtChange(){
	disableCxtChange++;
}

extern "C" void campEnableCtxtChange(){
	disableCxtChange--;
}


// //==================== For Debug ======================//
// extern "C"
// void dumpStoreHistoryTable() {
//   	printf("=-=-=-=-= Store History Table =-=-=-=-=\n");
//   	printf("[Cur Global Stack: ");
// 	for (int i = 0; i < STK_MAX_SIZE; ++i)
// 		printf("%d, ", globalIterStack.i8[i]);
// 	printf("]\n");

// 	for (std::unordered_map<void*, StoreHistoryElem>::iterator it=storeHistory.begin(); it!=storeHistory.end(); ++it) {
// 		// printf("%p => %x, [", it->first, it->second.campID);
// 		printf("? => %x, [", it->second.campID);
// 		for (int i = 0; i < STK_MAX_SIZE; ++i){
// 			printf("%d, ", it->second.iterStack.i8[i]);
// 		}
// 		printf("]\n");
// 	}
// }

// extern "C"
// void dumpLoadHistoryTable() {
// 	printf("=-=-=-=-= Load History Table =-=-=-=-=\n");
// 	printf("[Cur Global Stack: ");
// 	for (int i = 0; i < STK_MAX_SIZE; ++i)
// 		printf("%d, ", globalIterStack.i8[i]);
// 	printf("]\n");
	
// 	for (std::unordered_map<void*, std::map<CampID, IterStack> >::iterator it=loadHistory.begin();
// 			it!=loadHistory.end(); ++it) {
// 		std::map<CampID,IterStack> loadList = it->second;
// 		for (std::map<CampID, IterStack>::iterator ii=loadList.begin();
// 				ii!=loadList.end(); ++ii) {
// 			// printf("%p => %x, [", it->first, ii->first);
// 			printf("? => %x, [", ii->first);
// 			for (int i = 0; i < STK_MAX_SIZE; ++i){
// 				printf("%d, ", ii->second.i8[i]);
// 			}
// 			printf("]\n");
// 		}
// 	}
// }
		
extern "C"
void dumpDependenceTable() {
	// printf("=-=-=-=-= Dependence Table =-=-=-=-=\n");
	// for (std::unordered_map<DepID, IterRelation>::iterator it=(*depTable).begin();
	// 		it!=(*depTable).end(); ++it) {
	// 	CampID src = (CampID) (it->first >> 32);
	// 	CampID dst = (CampID) (it->first & 0xffffffff);
	// 	CntxID srcCtx = (CntxID) (src>>16);
	// 	InstrID srcInst = (InstrID) (src & 0xffff);
	// 	CntxID dstCtx = (CntxID) (dst>>16);
	// 	InstrID dstInst = (InstrID) (dst & 0xffff);
	// 	//human-readable form
	// 	std::cout<<srcCtx<<", "<<srcInst<<" => "<<dstCtx<<", "<<dstInst<<" : ";
	// 	std::cout<<std::bitset<STK_MAX_SIZE*2>(it->second)<<"\n";

	// 	//compressed form
	// 	// std::cout<< std::hex << src << ">" << dst << ":";
	// 	// std::cout<< std::hex << it->second << "\n";
	// }

	// // ########## C fopen style
	FILE *fp;
	fp = fopen("DependenceTable.data", "a+");
	for ( std::unordered_map<DepID, IterRelation>::iterator it = (*depTable).begin(), itE = (*depTable).end(); it != itE ; ++it ){
		CampID src = (CampID) (it->first >> 32);
		CampID dst = (CampID) (it->first & 0xffffffff);
		fprintf(fp, "%x>%x:%x\n", src, dst, it->second);
	}
	fclose(fp);
	//(*depTable).clear();
}