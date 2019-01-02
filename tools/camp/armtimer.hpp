#include <sys/time.h>

class armtimer{
private:
unsigned int start_count;
unsigned int end_count;
unsigned int perf_count;
double perf_time;
struct timeval start_time, end_time;

public:

inline 
unsigned int get_cyclecount (void)
{
        unsigned int value;
        asm volatile ("MRC p15, 0, %0, c9, c13, 0\t\n": "=r"(value));
        return value;
}

inline 
void init_perfcounter (int do_reset, int enable_divider)                               
{   
    // in general enable all counters (including cycle counter)                                       
    int value = 1;                                                                                    
    
    // peform reset:                                                                                  
    if (do_reset)                                                                                     
    {   
        value |= 2;     // reset all counters to zero.
        value |= 4;     // reset cycle counter to zero.                                               
    }                                                                                                 
    
    if (enable_divider) 
        value |= 8;     // enable "by 64" divider for CCNT.                                           
    
    value |= 16;                                                                                                      
    
    // program the performance-counter control-register:
    asm volatile ("MCR p15, 0, %0, c9, c12, 0\t\n" :: "r"(value));                                    
    
    // enable all counters:  
    asm volatile ("MCR p15, 0, %0, c9, c12, 1\t\n" :: "r"(0x8000000f));                               
    
    // clear overflows:
    asm volatile ("MCR p15, 0, %0, c9, c12, 3\t\n" :: "r"(0x8000000f));

	gettimeofday(&start_time, NULL);
	start_count = get_cyclecount();
                                                              
}

void stop_perfcounter (void)
{
	gettimeofday(&end_time, NULL);
	end_count = get_cyclecount();
	
	perf_time = (((double)end_time.tv_sec+((double)end_time.tv_usec/1000000))-((double)start_time.tv_sec+((double)start_time.tv_usec/1000000)));
	perf_count = end_count - start_count;
}

double get_result_time (void)
{
	return perf_time;
}

unsigned int get_result_count (void)
{
	return perf_count;
}

};

