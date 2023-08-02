#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "logger.h"

#define MAX_TIME_STR_LEN 256

int newSession = 1;


void plog(char *fmt,...)
{
	va_list		list;
	char		*p, *r;
	int		e;
	FILE		*fp;
	time_t 		rawTime;
	struct tm 	*timeInfo;
	char 		timeBuffer[MAX_TIME_STR_LEN + 1];

	
	if(newSession) // If new session, start the log file over from scratch
	{
		fp = fopen ("log.txt","w");
		newSession = 0;
	}
	else
		fp = fopen ("log.txt","a");

	// Print Time
	time(&rawTime);
	timeInfo = localtime(&rawTime);
	strftime(timeBuffer, MAX_TIME_STR_LEN, "%c", timeInfo);
	fprintf(fp, "[%s] ", timeBuffer);
	fprintf(stdout, "[%s] ", timeBuffer);

	// Parse format string
	va_start(list, fmt);
	for ( p = fmt ; *p ; ++p )
	{
		if ( *p != '%' )//If simple string
		{
			fputc( *p,fp );
			fputc( *p, stdout);
		}
		else
		{
			switch ( *++p )
			{
				// String
				case 's':
				{
					r = va_arg( list, char * );

					fprintf(fp,"%s", r);
					fprintf(stdout, "%s", r);
					continue;
				}

				// Int
				case 'd':
				{
					e = va_arg( list, int );

					fprintf(fp,"%d", e);
					fprintf(stdout, "%d", e);
					continue;
				}

				default:
				{
					fputc( *p, fp );
					fputc( *p, stdout);
				}
			}
		}
	}
	va_end( list );

	// New line after entry
	fputc( '\n', fp );
	fputc( '\n', stdout);

	fclose(fp);
}






