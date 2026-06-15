#include <stdio.h>
#include <syslog.h>

void loganevent(const char *processname, const char *eventstr)
{
	openlog(NULL, LOG_NOWAIT|LOG_PID, LOG_USER);
	syslog(LOG_WARNING, "%s: %s.",processname, eventstr);
	closelog();
}

