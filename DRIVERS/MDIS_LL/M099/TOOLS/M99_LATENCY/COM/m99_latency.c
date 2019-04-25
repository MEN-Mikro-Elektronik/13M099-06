/*********************  P r o g r a m  -  M o d u l e ***********************/
/*!  
 *        \file  m99_latency.c
 *
 *      \author  klaus.popp@men.de
 * 
 *  	 \brief  Measures interrupt and signal latency 
 *
 *     Switches: -
 */
/*
 *---------------------------------------------------------------------------
 * Copyright (c) 2003-2019, MEN Mikro Elektronik GmbH
 ****************************************************************************/
/*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <MEN/men_typs.h>
#include <MEN/mdis_api.h>
#include <MEN/usr_utl.h>
#include <MEN/usr_oss.h>
#include <MEN/m99_drv.h>

#define CHK(expr) \
 if(!(expr)){ \
	printf("*** Expression %s at line %d failed (%s)\n", \
    #expr, __LINE__, M_errstring(UOS_ErrnoGet() )); \
    goto ABORT;\
 }

#define TIME_PER_TICK	4		/* 68230 timer runs at 250kHz */
#define TICKS2US(tks) ((tks)*TIME_PER_TICK)

typedef struct {
	int32 min;
	int32 max; 
	int32 avgAcc;
	int32 count;
	int32 first;
	int32 totalMin;
	int32 totalMax;
} STATS;

static STATS G_irqStats, G_sigStats;
static MDIS_PATH G_path;
static const char IdentString[]=MENT_XSTR(MAK_REVISION);

/**********************************************************************/
/** print usage
 */
static void usage(void)
{
	printf("Usage: m99_latency [<opts>] <device> [<opts>]\n");
	printf("Function: Measures interrupt and signal "
		   "latency \n");
	printf("Options:\n");
	printf("    -t=<rate>      timer value [250]=1ms\n");
	printf("    -i=<interval>  interval      [1]=1s\n");
	printf("    device     devicename (M99)        [none]\n");
	printf("\n");
	printf("%s\n", IdentString );
	printf("(c) 2003..2010 by MEN mikro elektronik GmbH\n\n");
}

static void InitStats( STATS *st )
{
	st->min = 0x7fffffff;
	st->max = 0;
	st->avgAcc = 0;
	st->count = 0;
	st->first = 0;
	st->totalMin = 0;
	st->totalMax = 0;
}

static void UpdateStats( STATS *st, int32 tval )
{
	if( !st->first )
	{
		if( tval < st->min )
			st->min = tval;
		if( st->min < st->totalMin )
			st->totalMin = st->min;
	
		if( tval > st->max )
			st->max = tval;
		if( st->max > st->totalMax )
			st->totalMax = st->max;

		st->count++;
		st->avgAcc += tval;
	}
	else
	{
		st->first--;
	}
}

static void PrintStats( const STATS *st )
{
	printf("%6ld   %6ld   %6ld    (%6ld)",
		   TICKS2US(st->min), 
		   st->count ? TICKS2US(st->avgAcc)/st->count : -1, 
		   TICKS2US(st->max),
		   st->count
		  );
}

static void __MAPILIB SigHandler( u_int32 sigCode )
{
	if( sigCode == UOS_SIG_USR2 ){
		int32 tval=0, irqLat=0;

		M_getstat( G_path, M99_GET_TIME, &tval );
		M_getstat( G_path, M99_IRQ_LAT, &irqLat );
		
		UpdateStats( &G_irqStats, irqLat );
		UpdateStats( &G_sigStats, tval );		
	}
}


/**********************************************************************/
/** where all begins...
 */
int main( int argc, char **argv )
{
	int   interval;
	int32 n,timerval;
	char *device=NULL,*str,*errstr,buf[40];
	STATS irqStats, sigStats;

	InitStats(&irqStats);
	InitStats(&sigStats);

	if ((errstr = UTL_ILLIOPT("t=i=?", buf))) {	/* check args */
		printf("*** %s\n", errstr);
		return(1);
	}

	if (UTL_TSTOPT("?")) {						/* help requested ? */
		usage();
		return(1);
	}

	for (n=1; n<argc; n++)   		/* search for device */
		if (*argv[n] != '-') {
			device = argv[n];
			break;
		}

	if (!device) {
		usage();
		return(1);
	}
	
	timerval	= ((str=UTL_TSTOPT("t=")) ? atoi(str) : 250);

	interval	= ((str=UTL_TSTOPT("i=")) ? atoi(str) : 1);

	CHK((G_path = M_open(device)) >= 0);
	G_irqStats.first    = 3;
	G_irqStats.totalMin = 0x7fffffff;
	G_irqStats.totalMax = 0;
	G_sigStats.first    = 3;
	G_sigStats.totalMin = 0x7fffffff;
	G_sigStats.totalMax = 0;

	CHK( UOS_SigInit( SigHandler ) == 0 );
	CHK( UOS_SigInstall( UOS_SIG_USR2 ) == 0 );

	CHK( M_setstat(G_path,M99_SIG_set_cond1, UOS_SIG_USR2 ) == 0 );
	CHK( M_setstat(G_path,M99_SIG_set_cond2, UOS_SIG_USR2 ) == 0 );
	CHK( M_setstat(G_path,M99_SIG_set_cond3, UOS_SIG_USR2 ) == 0 );
	CHK( M_setstat(G_path,M99_SIG_set_cond4, UOS_SIG_USR2 ) == 0 );

	CHK( M_setstat(G_path,M_MK_IRQ_COUNT,0) == 0 );
	CHK( M_setstat(G_path,M99_TIMERVAL,timerval) == 0 );
	CHK( M_setstat(G_path,M_MK_IRQ_ENABLE,1) == 0 );

	printf("generating interrupts: timerval=%d\n", timerval );
	printf("(press any key for exit)\n");
	printf("    current Interrupt-Latency        |     current Signal-Latency          \n");
	printf("  min[us]  avg[us]  max[us]  (irq/s) |  min[us]  avg[us]  max[us]  (sigs/s)\n");
	printf(" =================================== | ====================================\n");
	
	/* Do not change the tool output because it is required for TestAutomation */
	
	while ( UOS_KeyPressed() == -1 ) {	
		
		UOS_Delay( interval * 1000 );
		UOS_SigMask();
		sigStats = G_sigStats;
		irqStats = G_irqStats;
		InitStats( &G_sigStats );
		InitStats( &G_irqStats );
		
		PrintStats( &irqStats );
		printf(" | ");
		PrintStats( &sigStats );
		printf("\n");
		UOS_SigUnMask();
	}
	
 ABORT:	
	M_setstat(G_path, M99_SIG_clr_cond1, UOS_SIG_USR2 );
	M_setstat(G_path, M99_SIG_clr_cond2, UOS_SIG_USR2 );
	M_setstat(G_path, M99_SIG_clr_cond3, UOS_SIG_USR2 );
	M_setstat(G_path, M99_SIG_clr_cond4, UOS_SIG_USR2 );

	UOS_SigRemove( UOS_SIG_USR2 );
	UOS_SigExit();
	if( G_path >= 0 ) 
		M_close( G_path );	
	printf("IRQ: total min/max   %d/%d [us]       | ", TICKS2US(irqStats.totalMin),  TICKS2US(irqStats.totalMax) );
	printf("SIG: total min/max   %d/%d [us]\n", TICKS2US(sigStats.totalMin),  TICKS2US(sigStats.totalMax) );
	return 0;
}


