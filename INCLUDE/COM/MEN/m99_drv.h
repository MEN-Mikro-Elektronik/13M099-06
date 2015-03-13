/***********************  I n c l u d e  -  F i l e  ************************
 *
 *         Name: m99_driver.h
 *
 *       Author: uf
 *        $Date: 2009/06/30 11:51:38 $
 *    $Revision: 1.4 $
 *
 *  Description: Header file for M99 driver modules
 *               - M99 specific status codes
 *               - M99 function prototypes
 *
 *     Switches: _ONE_NAMESPACE_PER_DRIVER_
 *               _LL_DRV_
 *
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: m99_drv.h,v $
 * Revision 1.4  2009/06/30 11:51:38  CRuff
 * R: Porting to MDIS5
 * M: changed according to MDIS Porting Guide 0.5
 *
 * Revision 1.3  2003/06/06 13:52:44  kp
 * added support to measure interrupt/signal latency
 *
 * Revision 1.2  2001/08/27 11:56:09  kp
 * made all functions static except GetEntry
 *
 * Revision 1.1  1998/02/23 10:07:37  franke
 * initial
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 1998 by MEN mikro elektronik GmbH, Nuernberg, Germany
 ****************************************************************************/
#ifndef _M99_LLDRV_H
#  define _M99_LLDRV_H

#  ifdef __cplusplus
      extern "C" {
#  endif

/*-----------------------------------------+
|  TYPEDEFS                                |
+------------------------------------------*/

/*-----------------------------------------+
|  DEFINES & CONST                         |
+------------------------------------------*/

/*--------- M99 specific status codes (MCOD_OFFS...MCOD_OFFS+0xff) --------*/
                         /* S,G: S=setstat, G=getstat code */
#define M99_TIMERVAL      M_DEV_OF+0x01    /* G,S: timer value */
#define M99_JITTER        M_DEV_OF+0x02    /* G,S: jitter mode enable */
#define M99_IRQCOUNT      M_DEV_OF+0x03    /* G,S: irq counter */
#define M99_SIG_set_cond1 M_DEV_OF+0x04    /* G,S: signal */
#define M99_SIG_set_cond2 M_DEV_OF+0x05    /* G,S: signal */
#define M99_SIG_set_cond3 M_DEV_OF+0x06    /* G,S: signal */
#define M99_SIG_set_cond4 M_DEV_OF+0x07    /* G,S: signal */
#define M99_SIG_clr_cond1 M_DEV_OF+0x08    /*   S: signal */
#define M99_SIG_clr_cond2 M_DEV_OF+0x09    /*   S: signal */
#define M99_SIG_clr_cond3 M_DEV_OF+0x0a    /*   S: signal */
#define M99_SIG_clr_cond4 M_DEV_OF+0x0b    /*   S: signal */
#define M99_GET_TIME	  M_DEV_OF+0x0c	   /* G  : get elapsed counter value */
#define M99_MAX_IRQ_LAT	  M_DEV_OF+0x0d	   /* G,S: max irq latency ticks */
#define M99_IRQ_LAT	  	  M_DEV_OF+0x0e	   /* G  : last irq latency ticks */

/* set/get block codes */
#define M99_SETGET_BLOCK_SRAM  M_DEV_BLK_OF+0x01  /* G,S: write/read 128 byte to from sram */

#define M99_MAX_SIGNALS   4




/*-----------------------------------------+
|  GLOBALS                                 |
+------------------------------------------*/

/*-----------------------------------------+
|  PROTOTYPES                              |
+------------------------------------------*/
#ifdef _LL_DRV_

#   ifndef _ONE_NAMESPACE_PER_DRIVER_
        extern void M99_GetEntry( LL_ENTRY* drvP );
#   endif /* !_ONE_NAMESPACE_PER_DRIVER_ */

#endif /* _LL_DRV_ */

/*-----------------------------------------+
|  BACKWARD COMPATIBILITY TO MDIS4         |
+-----------------------------------------*/
#ifndef U_INT32_OR_64
 /* we have an MDIS4 men_types.h and mdis_api.h included */
 /* only 32bit compatibility needed!                     */
 #define INT32_OR_64  int32
 #define U_INT32_OR_64 u_int32
 typedef INT32_OR_64  MDIS_PATH;
#endif /* U_INT32_OR_64 */

#  ifdef __cplusplus
      }
#  endif

#endif/*_M99_LLDRV_H*/

