#include <stdio.h>
#include <iostream>
#include <malloc.h>
#include <unordered_map>
#include <math.h>
#include <fstream> //to file IO
#include <bitset> //to output binary format
#include <iomanip> //to output hex format
#include <cstring>

#include "campExecRuntime.h"

static CntxID exec_currentCtx;
// profit test
#include <unordered_map>
#include <cassert>

#define _X86_
//#define _ARM_ 0

#ifdef _X86_
#include "x86timer.hpp"
#endif

#ifdef _ARM_
#include "armtimer.hpp"
#endif

#define MAX_LINE_SIZE 128

static unsigned nestingDepth;

#ifdef _X86_
static double totalSometime = 0.0;
static double tmpSometime = 0.0;
#endif
#ifdef _ARM_
static unsigned int totalSometime = 0.0;
static unsigned int tmpSometime = 0.0;
#endif

static double totalLooptime = 0.0;
static double totalfunctime = 0.0;
static int recurCount = 0;

#ifdef _X86_
std::unordered_map<CntxID, double> *execTimeOfContext;
std::unordered_map<CntxID, double> *tmpTimeOfContext;
#endif
#ifdef _ARM_
std::unordered_map<CntxID, unsigned int> *execTimeOfContext;
std::unordered_map<CntxID, unsigned int> *tmpTimeOfContext;
#endif



static uint16_t exec_disableCxtChange; // enabled when 0

static uint64_t start_total_time;

#ifdef _X86_
x86timer t;
#endif
#ifdef _ARM_
armtimer t;
#endif


// Initializer/Finalizer
extern "C"
void campExecInitialize (size_t ldrCnt, size_t strCnt, size_t callCnt, size_t loopCnt, size_t maxLoopDepth) {

	printf("first of all\n");	

	exec_disableCxtChange = 0;
	nestingDepth=0;
	totalSometime=0.0;
	tmpSometime=0.0;
	exec_currentCtx = 0;

	unsigned nContext = 0;

	char line[MAX_LINE_SIZE];
	std::ifstream fpSometime ("ContextTree.data", std::ifstream::binary);
	assert(fpSometime);
	fpSometime.getline(line, MAX_LINE_SIZE);
	std::string startMsg2("$$$$$ CONTEXT TREE IN PREORDER $$$$$");
	assert(startMsg2.compare(line) == 0);
	fpSometime.getline(line, MAX_LINE_SIZE);
	nContext = (unsigned)(atoi(line));

	#ifdef _X86_
//	printf("why??\n");
	execTimeOfContext = new std::unordered_map<CntxID, double>();
	tmpTimeOfContext = new std::unordered_map<CntxID, double>();
	#endif
	#ifdef _ARM_
	execTimeOfContext = new std::unordered_map<CntxID, unsigned int>();
        tmpTimeOfContext = new std::unordered_map<CntxID, unsigned int>();
//	printf("makesure\n");
	#endif

//	printf("second part\n");

	for (unsigned i = 0; i < nContext; ++i){ //
		(*execTimeOfContext)[i] = 0.0;
		(*tmpTimeOfContext)[i] = 0.0;
	}

//	printf("?????????\n");
	
	assert(nContext == execTimeOfContext->size()); //
	assert(nContext == tmpTimeOfContext->size()); //
	fpSometime.close();

//	printf("!!!!!!!!!!!!!!\n");
	
	#ifdef _X86_
	t.start();
	#endif
	#ifdef _ARM_
	t.init_perfcounter(1,1);
	#endif

//	printf("end of init\n");

}


extern "C"
void campExecFinalize (bool removeLoop) {	// edit later

//	printf("first part of finalize\n");

	std::ofstream execfile("ContextExecTime.data", std::ios::out | std::ofstream::binary);

	char line[MAX_LINE_SIZE];
	//Timer
	//double elapsedFromLast = corelab::Timer::ElapsedFromLast();
	//double elapsedFromBase = corelab::Timer::ElapsedFromBase();
	//double total_time = (double)(start_total_time - timer_start());
	#ifdef _X86_
	double total_time = t.stop() * (1.0e-9);
	#endif
	#ifdef _ARM_
	t.stop_perfcounter();
	double total_time = t.get_result_time();
	#endif	

	#ifdef _X86_
		(*execTimeOfContext)[0] = total_time; //elapsedFromBase;
	#endif
	#ifdef _ARM_
		(*execTimeOfContext)[0] = t.get_result_count();
		double time_per_count = total_time/((double)t.get_result_count());
	#endif


//	printf("middle of finalize\n");

	//fprintf(stderr, "CAMP_PROFIT@:>> Runtime terminated: %.3lfs (ACCM: %.3lfs)\n", 
	//				 elapsedFromLast, elapsedFromBase);
	// corelab::Timer::GetTimeInSec();

	execfile << "$$$$$ Context Exec Time $$$$$\n";

	execfile << "Total Exec Time :" << round(total_time*10000)/10000 << "s\n";
	#ifdef _X86_
	execfile << "Total Loop Time :" << round( ((double)totalSometime)*10000)/10000 << "s\n";
	#endif
	#ifdef _ARM_
	execfile << "Total Loop Time :" << round( ((double)totalSometime * time_per_count )*10000)/10000 << "s\n";
	#endif
	//execfile << "Total Func Time :" << totalfunctime << "\n";

	int nContext = 0;

	std::ifstream fpSometime ("ContextTree.data", std::ifstream::binary);
        assert(fpSometime);
        fpSometime.getline(line, MAX_LINE_SIZE);
        std::string startMsg2("$$$$$ CONTEXT TREE IN PREORDER $$$$$");
        assert(startMsg2.compare(line) == 0);
        fpSometime.getline(line, MAX_LINE_SIZE);		
				nContext = (unsigned)(atoi(line));

//	printf("before write down somthing\n");

	#ifdef _X86_
	for (unsigned i = 0; i < execTimeOfContext->size(); i++){
		fpSometime.getline(line, MAX_LINE_SIZE);
		//double percent = (round( ((((*execTimeOfContext)[i])/elapsedFromBase) * 100)*100)/100);
		double percent = (round( ((((*execTimeOfContext)[i])/total_time) * 100)*10000)/10000);
		if (percent > 30)
			execfile << line << "\t" << round((*execTimeOfContext)[i] * 10000) / 10000 << "s\t" << percent   << "% \[Critical\]\n";		
		else
			execfile << line << "\t" << round((*execTimeOfContext)[i] * 10000) / 10000 << "s\t" << percent   << "%\n";

	}
	#endif
	#ifdef _ARM_
	for (unsigned i = 0; i < execTimeOfContext->size(); i++){
                fpSometime.getline(line, MAX_LINE_SIZE);
                //double percent = (round( ((((*execTimeOfContext)[i])/elapsedFromBase) * 100)*100)/100);
                double percent = (round( ((( (double)(*execTimeOfContext)[i] * time_per_count)/total_time) * 100)*10000)/10000);
                if (percent > 30)
                        execfile << line << "\t" << round (( (double)(*execTimeOfContext)[i] * time_per_count) * 10000) / 10000 << "s\t" << percent   << "% \[Critical\]\n"; 
                else
                        execfile << line << "\t" << round (( (double)(*execTimeOfContext)[i] * time_per_count) * 10000) / 10000 << "s\t" << percent   << "%\n";

        }
	#endif

//	printf("after write down somthing\n");

	execfile << "$$$$$ Context Exec Time End $$$$$";
	execfile.close();
	fpSometime.close();

	printf("end\n");

#ifdef _ARM_
	std::ofstream gvfile("ContextExecTime.gv", std::ios::out | std::ofstream::binary);
	std::ifstream fpSometime2 ("ContextTree.gv", std::ifstream::binary);
	assert(fpSometime2);
	fpSometime2.getline(line, MAX_LINE_SIZE);
	gvfile << line;

	for (unsigned i = 0; i < execTimeOfContext->size(); i++){
		fpSometime2.getline(line, MAX_LINE_SIZE);
		gvfile << line << "\n";
	}

	for (unsigned i = 0; i < execTimeOfContext->size(); i++){
		//double percent = (round( ((((*execTimeOfContext)[i])/elapsedFromBase) * 100)*100)/100);
		double percent = (round( ((  (double)((*execTimeOfContext)[i])*time_per_count /total_time ) * 100)*10000)/10000);
		if (percent > 30)
			gvfile << i << "\[color=red, label=\"" << i << " \\n " << round( (double)(*execTimeOfContext)[i] * time_per_count * 10000) / 10000 << "s " << percent   << "%\"\]\;\n";
		else
			gvfile << i << "\[label=\"" << i << " \\n " << round( (double)(*execTimeOfContext)[i] * time_per_count * 10000) / 10000 << "s " << percent   << "%\"\]\;\n";
	}
	gvfile << "}\n";
	gvfile.close();
	fpSometime2.close();
#endif


#ifdef _X86_
	std::ofstream gvfile("ContextExecTime.gv", std::ios::out | std::ofstream::binary);
	std::ifstream fpSometime2 ("ContextTree.gv", std::ifstream::binary);
	assert(fpSometime2);
	fpSometime2.getline(line, MAX_LINE_SIZE);
	gvfile << line;

	int d,j,t;
	char b[MAX_LINE_SIZE];
	char c[MAX_LINE_SIZE];

	// insert dependency
	for (unsigned i = 0; i < execTimeOfContext->size() -1 ; i++){
		fpSometime2.getline(line, MAX_LINE_SIZE);
		for (d = 0; d < MAX_LINE_SIZE; d++){
			if ( line[d] == ' '){
				for (j=d+4; line[j]!=';'; j++)
					c[j-d-4]=line[j];
				c[j]='\0';
				break;
			}
		}
		memset(b,0,MAX_LINE_SIZE);
		memcpy(b,line, d);
		int k = atoi(b);
		int sec_num = atoi(c);

		if ( ( ((*execTimeOfContext)[k]/total_time) > 0.005 && ( ((*execTimeOfContext)[sec_num]/total_time) > 0.005 )  ) || (line[0] == '0') || nContext < 300 ){
			gvfile << line << "\n";
		}

	}

	if(removeLoop){
		fpSometime2.close();
		std::ifstream fpSometime3 ("contextLabel.gv", std::ifstream::binary);
		for(unsigned i =0; i< execTimeOfContext->size(); i++){
			fpSometime3.getline(line, MAX_LINE_SIZE);
			if (line[0] == '}')
				break;
			memset(c,0,MAX_LINE_SIZE);
			for(j=0;j<MAX_LINE_SIZE;j++){
				if(line[j]=='[')
					break;
				c[j]=line[j];
			}
			c[j]='\0';	//get UCID from .gvfile
			int k = atoi(c);	// type cast

			memset(b,0,MAX_LINE_SIZE);

			for(unsigned n=0;n<MAX_LINE_SIZE;n++){
				if(line[n]=='\"'){
					for (d=n+1; line[d]!='\"'; d++)
						b[d-n-1]=line[d];
					b[d-n-1]='\0';// get label from .gvfile
					break;
				}
			}

			if ( ((*execTimeOfContext)[k]/total_time) > 0.005 || nContext < 300 ){
				double percent =  (round( ((((*execTimeOfContext)[k])/total_time) * 100)*10000)/10000);
				if (percent > 20)
					gvfile << k << "[color=red, label=\"" << b <<"\\n" << round((*execTimeOfContext)[k] * 10000) / 10000 << "s  " << percent   << "%\"];\n";
				else
					gvfile << k << "[label=\"" << b <<"\\n" << round((*execTimeOfContext)[k] * 10000) / 10000 << "s  " << percent   << "%\"];\n";

			}
		}
		fpSometime2.close();
	}
	else{
		fpSometime2.close();
		std::ifstream fpSometime3 ("ContextTree.data", std::ifstream::binary);
		fpSometime3.getline(line, MAX_LINE_SIZE);
		fpSometime3.getline(line, MAX_LINE_SIZE);	
		// insert label
		for (unsigned i = 0; i < execTimeOfContext->size(); i++){
			fpSometime3.getline(line, MAX_LINE_SIZE);

			if ( ((*execTimeOfContext)[i]/total_time) > 0.005 || nContext < 300 ){
				memset(c,0,MAX_LINE_SIZE);
				j=0;
				for(d=0;d<MAX_LINE_SIZE;d++){
					if(j==4){
						for(t=0;line[t+d]!='\0';t++)
							c[t]=line[d+t];
						c[t]='\0';
						break;
					}
					if(line[d]==' ')
						j+=1;
				}

				double percent = (round( ((((*execTimeOfContext)[i])/total_time) * 100)*10000)/10000);
				if (percent > 20)
					gvfile << i << "\[color=red, label=\""<<i<<"\\n" << c << " \\n " << round((*execTimeOfContext)[i] * 10000) / 10000 << "s " << percent   << "%\"\]\;\n";
				else
					gvfile << i << "\[label=\""<<i<<"\\n" << c << " \\n " << round((*execTimeOfContext)[i] * 10000) / 10000 << "s " << percent   << "%\"\]\;\n";
			}
		}
		fpSometime3.close();
	}
	gvfile << "}\n";
	gvfile.close();
#endif

	

/*
	double sum = 0.0;
	//test
	fprintf(stderr, "size: %zu\n", execTimeOfSometimesDOALLableCtxt->size());
	for(auto e : *execTimeOfSometimesDOALLableCtxt){
		if(e.second != 0.0)
			fprintf(stderr, "%d:%lf\n", e.first, e.second);
		sum+=e.second;
	}
	fprintf(stderr, "CAMP_PROFIT_TEST: # of sometimesDOALLable ctxt: %zu, sometimesDOALLable total: %lf, ratio: %lf (percent) \n"
		, execTimeOfSometimesDOALLableCtxt->size(), totalSometime, (100.0*totalSometime)/elapsedFromBase);
	fprintf(stderr, "%lf\n", sum);
*/
}

// Normal Context Event
extern "C"
void campExecLoopBegin (CntxID cntxID) { //arg is LocId
	if(exec_disableCxtChange != 0) return;

	exec_currentCtx += cntxID;

	if( execTimeOfContext->find(exec_currentCtx) != execTimeOfContext->end() ){
		//if ((*tmpTimeOfContext)[currentCtx] != 0.0)
			//printf ("%d\n", currentCtx);
 
		//assert((*tmpTimeOfContext)[currentCtx] == 0.0);
		//double now = corelab::Timer::GetTimeInSec();
		//double now = (double)timer_start();
		#ifdef _X86_
                double now = t.now();
                #endif
                #ifdef _ARM_
                unsigned int now = t.get_cyclecount();
                #endif

		(*tmpTimeOfContext)[exec_currentCtx] = now;
		// fprintf(stderr, "LOOP BEGIN %d, %lf \n", currentCtx, corelab::Timer::GetTimeInSec());

		//for calculating totalSometime
		if(nestingDepth > 0)
			totalSometime += (now - tmpSometime);
		tmpSometime = now;
		nestingDepth++;
	}

}

extern "C"
void campExecLoopNext () {
	if(exec_disableCxtChange != 0) return;
}

extern "C"
void campExecLoopEnd (CntxID cntxID) {
	if(exec_disableCxtChange!= 0) return;
	
	if( execTimeOfContext->find(exec_currentCtx) != execTimeOfContext->end() ){
		//if ((*tmpTimeOfContext)[currentCtx] != 0.0)
		//	printf ("%d\n",currentCtx);
		assert((*tmpTimeOfContext)[exec_currentCtx] != 0.0);
		//double now = corelab::Timer::GetTimeInSec();
		//double now = (double)(timer_start());
		#ifdef _X86_
                double now = t.now();
                #endif
                #ifdef _ARM_
                unsigned int now = t.get_cyclecount();
                #endif

		(*execTimeOfContext)[exec_currentCtx] += (now - (*tmpTimeOfContext)[exec_currentCtx]);
		(*tmpTimeOfContext)[exec_currentCtx] = 0.0;
		// fprintf(stderr, "LOOP END %d, %lf \n", currentCtx, corelab::Timer::GetTimeInSec());

		//for calculating totalSometime
		totalSometime += (now - tmpSometime);
		tmpSometime = now;
		nestingDepth--;
		totalLooptime += (*execTimeOfContext)[exec_currentCtx];
	}

	exec_currentCtx -= cntxID;
}

extern "C"
void campExecCallSiteBegin (CntxID cntxID) {
	if (cntxID < 1)
                return;

	if(exec_disableCxtChange == 0)
	exec_currentCtx += cntxID;
	else
		return;
		

	if( execTimeOfContext->find(exec_currentCtx) != execTimeOfContext->end() ){
		//assert((*tmpTimeOfContext)[currentCtx] == 0.0);
		if( (*tmpTimeOfContext)[exec_currentCtx] == 0.0 ) { // first call
			//double now = corelab::Timer::GetTimeInSec();
			//double now = (double)(timer_start());
			#ifdef _X86_
			double now = t.now();
			#endif
			#ifdef _ARM_
			unsigned int now = t.get_cyclecount();	
			#endif

			(*tmpTimeOfContext)[exec_currentCtx] = now;
		}
		else { // recursive call we actually don't consider recursive call when we make a context tree and camp exectime pass
			printf ("%d\n", exec_currentCtx);
			//assert (false);
			/*
			recurCount++;
			//double now = corelab::Timer::GetTimeInSec();
			//double now = (double)(timer_start());
			#ifdef _X86_
                        double now = t.now();
                        #endif
                        #ifdef _ARM_
                        unsigned int now = t.get_cyclecount();
                        #endif

			(*execTimeOfContext)[currentCtx] += (now - (*tmpTimeOfContext)[currentCtx]);
			(*tmpTimeOfContext)[currentCtx] = now;
			*/
			(*execTimeOfContext)[exec_currentCtx] +=1;	//this represents how many times nested.
		} 	
	}
}

extern "C"
void campExecCallSiteEnd  (CntxID cntxID) {

	if(exec_disableCxtChange !=0)
		return;
	if(cntxID < 1 )
		return;

	if( execTimeOfContext->find(exec_currentCtx) != execTimeOfContext->end() ){
		//assert((*tmpTimeOfContext)[currentCtx] != 0.0);
		if( (*tmpTimeOfContext)[exec_currentCtx] != 0.0){ 
			//double now = corelab::Timer::GetTimeInSec();
			//double now = (double)(timer_start());
			
				#ifdef _X86_
                                double now = t.now();
                                #endif
                                #ifdef _ARM_
                                unsigned int now = t.get_cyclecount();
                                #endif

                                (*execTimeOfContext)[exec_currentCtx] += (now - (*tmpTimeOfContext)[exec_currentCtx]);
                                /*
                                if( recurCount > 0){
                                        (*tmpTimeOfContext)[currentCtx] = now;
                                        recurCount--;   
                                }
                                else*/
                                (*tmpTimeOfContext)[exec_currentCtx] = 0.0;
		}
	}

	if(exec_disableCxtChange == 0)
        	exec_currentCtx -= cntxID;

}

// Memory Event
extern "C" void campExecDisableCtxtChange(){
	exec_disableCxtChange++;
}

extern "C" void campExecEnableCtxtChange(){
	exec_disableCxtChange--;
}
