// #include <stdio.h>
// #include "campRuntime.h"

// void foo(int* testVar) { // ctx 2
// 	campLoopBegin(1);
// 	for(int i = 0; i<10; i++) { // ctx 3
// 		campLoopNext();
// 		campLoadInstr(&testVar[i], 2);
// 		testVar[i]++; // 2, 3
// 		campStoreInstr(&testVar[i], 3);
// 	}
// 	campLoopEnd(1);
// }


// int main () { // ctx 0
// 	campInitialize(10, 10, 10, 10, 8);

// 	int testVar[10];

// 	campLoopBegin(1); 
// 	for(int i = 0; i<10; i++) {  // ctx 1
// 		campLoopNext();
// 		campStoreInstr(&testVar[i], 1);
// 		testVar[i] = 0; // 1
// 	}
// 	campLoopEnd(1);

// 	dumpStoreHistoryTable();
// 	dumpLoadHistoryTable();


// 	campCallSiteBegin(2);
// 	foo(testVar);
// 	campCallSiteEnd(2);

// 	dumpStoreHistoryTable();
// 	dumpLoadHistoryTable();

// 	campLoopBegin(3);
// 	for(int i = 0; i<10; i++) {
// 		campLoopNext();
// 		campLoopBegin(1);
// 		for(int j = 0; j<3; j++) {
// 			campLoopNext();
// 			campLoadInstr(&testVar[i], 4);
// 			printf("%d ", testVar[i]); // 4
// 		}
// 		campLoopEnd(1);
// 	}
// 	campLoopEnd(3);
// 	printf("\n");
// 	dumpLoadHistoryTable();

// 	dumpDependenceTable();
// 	campFinalize();
// }
