#ifndef SHADOW_MEM_H
#define SHADOW_MEM_H

#include <sys/mman.h>
#include <cstring>
#include <cassert>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <cstdlib>
#include <execinfo.h>

#include <errno.h>
#include <string.h>


inline static void DumpBacktrace ();

#define ADDR_MASK 		0x00007fffffffffffL 			// 47-bit Address Mask
#define SHADOW_XMASK1 	0x0000400000000000L 			// XOR Mask to get Shadow Address (or 0x0000200000000000L)
#define SHADOW_XMASK2 	0x0000200000000000L 			// XOR Mask to get Shadow Address (or 0x0000200000000000L)
#define PAGE_SIZE 		4096 							// Page Size (4Kbyte) : TODO:should be depend on System. not fixed
#define SIZE_ELEM		8								// size of one element in byte
#define N_SHIFT_FOR_ONE_ELEM 5 							// number of bits to shift to get memory space for that element in shadow memory
#define N_SHIFT_FOR_ONE_SIZE_T 3 						//log (sizeof(size_t)) = log 8 = 3

#define GET_SHADOW_ADDR_HISTORY_TB(addr) ( ( ( ( (uint64_t)(addr) ) << (N_SHIFT_FOR_ONE_ELEM) ) & ADDR_MASK ) ^ SHADOW_XMASK1)
#define GET_SHADOW_ADDR_MALLOC_MAP(addr) ( (  ( (uint64_t)(addr) & 0xfffffffffffffff8 )   & ADDR_MASK ) ^ SHADOW_XMASK1)


class ShadowMemoryManager{
	public:


		static inline void AllocPage (void* addr, size_t size){
			uint64_t a = reinterpret_cast<uint64_t>(addr);
			uint64_t pagemask = ~(PAGE_SIZE-1);
			uint64_t pagebegin = a & pagemask;
			uint64_t pageend = (a+size-1) & pagemask;

		    //printf("%x %x\n", pagebegin, pageend+pagesize);
		    assert((pagebegin == pageend || pagebegin + PAGE_SIZE == pageend) && "ERROR: tries to alloc more than one page");

		    void* res;
		    if(pagebegin == pageend) // the place we want is within one page
				res = mmap ((void *)pagebegin, PAGE_SIZE, PROT_WRITE | PROT_READ,
									MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_NORESERVE, -1, (off_t)0);
			else{
				res = mmap ((void *)pagebegin, PAGE_SIZE, PROT_WRITE | PROT_READ,
									MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_NORESERVE, -1, (off_t)0);
				void* res2 = mmap ((void *)(pagebegin+PAGE_SIZE), PAGE_SIZE, PROT_WRITE | PROT_READ,
									MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_NORESERVE, -1, (off_t)0);
				if ((void*)(pagebegin+PAGE_SIZE) != res2){
					fprintf(stderr, "MAP_FAILED2: addr %p, pagebegin %lx, res %p, %s\n", addr, pagebegin+PAGE_SIZE, res2, strerror(errno));
					assert (res2 != MAP_FAILED && "ERROR :: MAP_FAILED 2");
					assert ((void *)(pagebegin+PAGE_SIZE) == res2);
				}

			}


			if ((void*)pagebegin != res){
				fprintf(stderr, "MAP_FAILED: addr %p, pagebegin %lx, res %p, %s\n", addr, pagebegin, res, strerror(errno));
				assert (res != MAP_FAILED && "ERROR :: MAP_FAILED");
				assert ((void *)pagebegin == res);
			}
			
			memset ((void *)pagebegin, 0, PAGE_SIZE);

		}

		static void initialize(){
			// Install SIGSEGV handler
			struct sigaction segvAction;
			segvAction.sa_flags = SA_SIGINFO | SA_NODEFER;
			sigemptyset (&segvAction.sa_mask);
			segvAction.sa_sigaction = SegFaultHandler;
			int hr = sigaction (SIGSEGV, &segvAction, NULL);
			assert (hr != -1);
		}

		/* SIGSEGV Handler */
		static void SegFaultHandler (int sig, siginfo_t* si, void* unused){
			// fprintf(stderr, "%p is segfault\n", si->si_addr);
			if (si->si_addr == NULL)
			{
				DumpBacktrace ();
				assert (si->si_addr != NULL);
			}
			AllocPage(si->si_addr, SIZE_ELEM);
			
		}
		
		//static struct sigaction	segvAction;
};


inline static void DumpBacktrace (){
	int j, nptrs;
	void *buffer[100];
	char **strings;

	nptrs = backtrace(buffer, 100);
	fprintf (stderr, "backtrace() returned %d addresses\n", nptrs);

	/* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
		 would produce similar output to the following: */

	strings = backtrace_symbols (buffer, nptrs);
	if (strings == NULL)
	{
		perror ("backtrace_symbols");
		exit (EXIT_FAILURE);
	}

	for (j = 0; j < nptrs; j++)
		fprintf (stderr, "%s\n", strings[j]);

	free(strings);
}

#endif // SHADOW_MEM_H
