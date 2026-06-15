#include <stdlib.h>
#include <unistd.h>  /* UNIX standard function definitions */
#include <errno.h>   /* Error number definitions */
#include <sys/time.h> 
#include <time.h> 

int DeadDelayMS(int ms)
{
	struct timespec	req, rem;
	
	req.tv_sec = 0;
	req.tv_nsec = ms * 1000000;//8000000;//8ms in nanoseconds
	rem.tv_sec = 0;
	rem.tv_nsec = 0;
	while(1)// fail check errno and wait again.
	{
		if(nanosleep(&req, &rem) != -1)
			break;
		else
		{
			if(errno == EINTR)
			{
				if(rem.tv_nsec != 0)
				{
					req.tv_sec = 0;
					req.tv_nsec = rem.tv_nsec;
					rem.tv_sec = 0;
					rem.tv_nsec = 0;
				}
				else
					break;
			}
			else
				break;
		}
	}
	return 0;
}

int DeadDelayUS(int us)
{
	struct timespec	req, rem;
	
	req.tv_sec = 0;
	req.tv_nsec = us * 1000;//8000;//8us in nanoseconds
	rem.tv_sec = 0;
	rem.tv_nsec = 0;
	while(1)// fail check errno and wait again.
	{
		if(nanosleep(&req, &rem) != -1)
			break;
		else
		{
			if(errno == EINTR)
			{
				if(rem.tv_nsec != 0)
				{
					req.tv_sec = 0;
					req.tv_nsec = rem.tv_nsec;
					rem.tv_sec = 0;
					rem.tv_nsec = 0;
				}
				else
					break;
			}
			else
				break;
		}
	}
	return 0;
}


