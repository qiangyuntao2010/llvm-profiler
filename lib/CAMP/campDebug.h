/*--------------------------------------*
 * campDebug.h 		  		                *
 *                                      *
 * Debug interfaces 								    *
 * Written by: gwangmu                  *
 *--------------------------------------*/

#ifndef CORELAB_CAMP_DEBUG_H
#define CORELAB_CAMP_DEBUG_H

#include <cstdlib>
#include <execinfo.h>
#include <unistd.h>

using namespace std;

namespace corelab
{
	namespace CAMP
	{
		namespace Debug
		{
			inline void
			DumpBacktrace ()
			{
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
		}
	}
}

#endif
