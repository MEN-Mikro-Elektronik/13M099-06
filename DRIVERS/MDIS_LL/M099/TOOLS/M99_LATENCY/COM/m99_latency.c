/*********************  P r o g r a m  -  M o d u l e ***********************/
/*!  
 *        \file  m99_latency.c
 *
 *      \author  klaus.popp@men.de
 *        $Date: 2010/08/20 14:10:25 $
 *    $Revision: 1.12 $
 * 
 *  	 \brief  Measures interrupt and signal latency 
 *
 *     Switches: -
 */
/*-------------------------------[ History ]---------------------------------
 *
 * $Log: m99_latency.c,v $
 * Revision 1.12  2010/08/20 14:10:25  CKauntz
 * R: *** TestAutomatisation is based on current view ***
 * M: Do not change the tool output because it is required for TestAutomation
 *
 * Revision 1.11  2010/02/09 15:18:31  ufranke
 * cosmetics
 *
 * Revision 1.10  2010/02/09 15:16:50  ufranke
 * R: measure interval of 1s is fix
 * M: added user parameter interval - default 1s
 * R: min,max values valid for the current measurement interval only
 * M: added totalMin and totalMax
 *
 * Revision 1.9  2009/08/04 14:39:07  CRuff
 * R: statistics sometimes show unrealistic values after porting to 64 bit
 *    (very high, very negative)
 * M: initialize variables tval, irqLat in method SigHandler() before reading
 *    the values with M_getstat
 *
 * Revision 1.8  2009/06/24 11:03:17  CRuff
 * R: 1.Porting to MDIS5
 * M: 1. changed according to MDIS Porting Guide 0.5
 *
 * Revision 1.7  2009/05/08 09:42:54  dpfeuffer
 * R: sighdl() uses wrong calling convention under Windows
 * M: __MAPILIB keyword added to sighdl()
 *
 * Revision 1.6  2008/09/17 12:02:09  CKauntz
 * R: Mdis-Api Interface changed to support 64 bit
 * M: Path changed to INT32_OR_64
 *
 * Revision 1.5  2006/09/14 09:56:36  DPfeuffer
 * cosmetics
 *
 * Revision 1.4  2006/07/21 09:19:06  ufranke
 * cosmetics
 *
 * Revision 1.3  2005/03/30 09:08:28  kp
 * clear down signal conditions at end of test (in case LL driver still inized)
 *
 * Revision 1.2  2004/05/05 14:12:00  cs
 * changed symbols G_path, G_irqStats, G_sigStats to static
 *
 * Revision 1.1  2003/06/06 13:52:43  kp
 * Initial Revision
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 2003..2010 by MEN mikro elektronik GmbH, Nuernberg, Germany 
 ****************************************************************************/

static char *RCSid="$Id: m99_latency.c,v 1.12 2010/08/20 14:10:25 CKauntz Exp $";

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
	printf("%s\n", RCSid );
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


