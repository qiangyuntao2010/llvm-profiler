#ifndef CORELAB_CAMP_TIMER_H
#define CORELAB_CAMP_TIMER_H

#include <sys/time.h>
#include <unistd.h>
#include <inttypes.h>

namespace corelab
{
	namespace CAMP
	{
		namespace Timer
		{
			static double base;
			static double last;

			static inline double
			GetTimeInSec ()
			{
				struct timeval tval;
				gettimeofday (&tval, NULL);
				return tval.tv_sec + tval.tv_usec / 1000000.0;
			}

			inline void
			Set ()
			{
				last = base = GetTimeInSec();
			}

			inline double
			ElapsedFromBase ()
			{
				double time = GetTimeInSec();
				last = time;
				return time - base;
			}
			
			inline double
			ElapsedFromLast ()
			{
				double time = GetTimeInSec();
				double elapsed = time - last;
				last = time;
				return elapsed;
			}
		}
	}
}

#endif
