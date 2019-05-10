/*********************  P r o g r a m  -  M o d u l e ***********************
 *
 *         Name: m99_drv.c
 *      Project: M99 module driver (MDIS V4.x)
 *
 *       Author: uf
 *        $Date: 2010/08/20 14:10:23 $
 *    $Revision: 1.19 $
 *
 *  Description: Low level driver for M99 modules
 *
 *     Required: ---
 *     Switches: _ONE_NAMESPACE_PER_DRIVER_
 *               MASK_IRQ_ILLEGAL   Calles OSS_SigCreate() with mask IRQ.
 *                                  Only allowed for special test!
 *
 *---------------------------------------------------------------------------
 * Copyright (c) 1997-2019, MEN Mikro Elektronik GmbH
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

static const char RCSid[]="M99 - m99 low level driver $Id: m99_drv.c,v 1.19 2010/08/20 14:10:23 CKauntz Exp $";

/* pass debug definitions to dbg.h */
#define DBG_MYLEVEL		m99Hdl->dbgLevel

#include <MEN/men_typs.h>   /* system dependend definitions   */
#include <MEN/dbg.h>
#include <MEN/oss.h>        /* os services                    */
#include <MEN/mdis_err.h>   /* mdis error numbers             */
#include <MEN/mbuf.h>       /* buffer lib functions and types */
#include <MEN/maccess.h>    /* hw access macros and types     */
#include <MEN/desc.h>       /* descriptor functions           */
#include <MEN/mdis_api.h>   /* global set/getstat codes       */
#include <MEN/mdis_com.h>   /* info function      codes       */
#include <MEN/modcom.h>     /* id prom access                 */

#include <MEN/ll_defs.h>    /* low level driver definitions   */
#include <MEN/ll_entry.h>   /* low level driver entry struct  */
#include <MEN/m99_drv.h>    /* M99 driver header file */

/*-----------------------------------------+
|  DEFINES & CONST                         |
+------------------------------------------*/
#define MOD_ID_MAGIC_WORD   0x5346        /* eeprom identification (magic) */
#define M99_MOD_ID          99
#define M99_DONT_ID_CHECK 0

#define SRAM         0
#define MC68230      0

#define M99_MAX_CH     1
#define M99_CH_WIDTH   1    /* byte */

#define M99_DEFAULT_BUF_SIZE     128   /* byte */
#define M99_DEFAULT_BUF_TIMEOUT  1000  /* ms */

#define M99_SRAM_RW_BUF_SIZE 64  /* byte */
#define M99_CLOCK_FREQ  250000
#define M99_JITTER_OFF  0
#define M99_JITTER_ON   1

/* MC68230 registers */
#define PGC_REG    0x80    /* port general control */
#define PADD_REG   0x84    /* port A data dir */
#define PBDD_REG   0x86    /* port B data dir */
#define PAC_REG    0x8c    /* port A control */
#define PBC_REG    0x8e    /* port B control */
#define PAD_REG    0x90    /* port A data */
#define PBD_REG    0x92    /* port B data */
#define TC_REG     0xa0    /* timer control */
#define TIV_REG    0xa2    /* timer irq vector, not used - no vector from modul supported */
#define CPH_REG    0xa6    /* counter preload high */
#define CPM_REG    0xa8    /* counter preload middle */
#define CPL_REG    0xaa    /* counter preload low */
#define CNTH_REG   0xae	   /* current counter high */
#define CNTM_REG   0xb0	   /* current counter middle */
#define CNTL_REG   0xb2	   /* current counter low */
#define TS_REG     0xb4    /* timer status */


/* debug handle */
#define DBH		m99Hdl->dbgHdl

/*-----------------------------------------+
|  TYPEDEFS                                |
+------------------------------------------*/
typedef struct
{
    int32           OwnMemSize;
    u_int32         dbgLevel;			  /* debug level */
	DBG_HANDLE*     dbgHdl;        		  /* debug handle */
    OSS_HANDLE*     osHdl;                /* for complicated os */
    OSS_IRQ_HANDLE  *irqHdl;
    MACCESS         maM68230;             /* access pointer to MC68230 */
    MACCESS         maSRAM;               /* access pointer to SRAM    */
    MBUF_HANDLE     *inbuf;
    MBUF_HANDLE     *outbuf;
    u_int32         useModulId;
    int32           nbrOfChannels;
    OSS_SIG_HANDLE  *cond[M99_MAX_SIGNALS];
    u_int32         medPreLoad;           /* medium irq rate */
    u_int32         timerval;             /* current timervalue */
    u_int32         jittermode;           /* jitter mode flag */
    u_int32         irqCount;             /* number of interrupts occurred */
    u_int32         rd_offs;              /* address in SRAM for next read */
    u_int32         wr_offs;              /* address in SRAM for next write */
    u_int32         RWbufSize;            /* read and write buffer size */
    int32           laststep;             /* last step of irq rate */
	u_int32			irqLatency; 		  /* current interrupt latency  */
	u_int32			maxIrqLatency; 		  /* max. interrupt latency  */
	MDIS_IDENT_FUNCT_TBL idFuncTbl;		  /* id function table */
} M99_HANDLE;


/*-----------------------------------------+
|  GLOBALS                                 |
+------------------------------------------*/

/*-----------------------------------------+
|  PROTOTYPES                              |
+------------------------------------------*/
static char* M99_Ident( void );

static int32 setStatBlock(
    M99_HANDLE         *m99Hdl,
    int32              code,
    M_SETGETSTAT_BLOCK *blockStruct);

static int32 getStatBlock(
    M99_HANDLE         *m99Hdl,
    int32              code,
    M_SETGETSTAT_BLOCK *blockStruct);

static void  setTime( M99_HANDLE* m99Hdl, int32 timerval);
static u_int32 getTime( M99_HANDLE *m99Hdl );
static void  dostep( M99_HANDLE* m99Hdl );

static int32 M99_HwBlockRead(
                  M99_HANDLE  *m99Hdl,
                  void  *buf,
                  int32 size);

static int32 M99_HwBlockWrite(
                  M99_HANDLE  *m99Hdl,
                  void  *buf,
                  int32 size);
static int32 M99_Init( DESC_SPEC       *descSpec,
                           OSS_HANDLE      *osHdl,
                           MACCESS         *ma,
                           OSS_SEM_HANDLE  *devSemHdl,
                           OSS_IRQ_HANDLE  *irqHdl,
                           LL_HANDLE       **llHdlP );

static int32 M99_Exit(        LL_HANDLE **llHdlP );
static int32 M99_Read(        LL_HANDLE *llHdl,  int32 ch,  int32 *value );
static int32 M99_Write(       LL_HANDLE *llHdl,  int32 ch,  int32 value );
static int32 M99_SetStat(     LL_HANDLE *llHdl,  int32 ch,  int32 code, INT32_OR_64 value32_or_64 );
static int32 M99_GetStat(     LL_HANDLE *llHdl,  int32 ch,  int32 code, INT32_OR_64 *value32_or_64P );
static int32 M99_BlockRead(   LL_HANDLE *llHdl,  int32 ch,  void  *buf, int32 size, int32 *nbrRdBytesP );
static int32 M99_BlockWrite(  LL_HANDLE *llHdl,  int32 ch,  void  *buf, int32 size, int32 *nbrWrBytesP);
static int32 M99_Irq(         LL_HANDLE *llHdl );
static int32 M99_Info(        int32     infoType, ... );

/*****************************  M99_Ident  **********************************
 *
 *  Description:  Gets the pointer to ident string.
 *
 *
 *---------------------------------------------------------------------------
 *  Input......:  -
 *
 *  Output.....:  return  pointer to ident string
 *
 *  Globals....:  -
 ****************************************************************************/
static char* M99_Ident( void )
{
    return( (char*) RCSid );
}/*M99_Ident*/



/**************************** M99_GetEntry *********************************
 *
 *  Description:  Gets the entry points of the low level driver functions.
 *
 *
 *---------------------------------------------------------------------------
 *  Input......:  ---
 *
 *  Output.....:  drvP  pointer to the initialized structure
 *
 *  Globals....:  ---
 *
 ****************************************************************************/
#ifdef _ONE_NAMESPACE_PER_DRIVER_
    void LL_GetEntry( LL_ENTRY* drvP )
#else
# ifdef MAC_BYTESWAP
    void M99_SW_GetEntry( LL_ENTRY* drvP )
# else
    void M99_GetEntry( LL_ENTRY* drvP )
# endif
#endif /* _ONE_NAMESPACE_PER_DRIVER_ */
{

    drvP->init        = M99_Init;
    drvP->exit        = M99_Exit;
    drvP->read        = M99_Read;
    drvP->write       = M99_Write;
    drvP->blockRead   = M99_BlockRead;
    drvP->blockWrite  = M99_BlockWrite;
    drvP->setStat     = M99_SetStat;
    drvP->getStat     = M99_GetStat;
    drvP->irq         = M99_Irq;
    drvP->info        = M99_Info;

}/*M99_GetEntry*/

/******************************** M99_Init ***********************************
 *
 *  Description:  Decodes descriptor , allocates and inits ll-drv data
 *                inits the hardware.
 *                Reads and checks the ID if in descriptor enabled.
 *                Clears and disables the module interrupts.
 *                The driver support 1 channel.
 *
 *                deskriptor key        default         range
 *
 *                DEBUG_LEVEL           DBG_OFF          see oss.h
 *                RD_BUF/SIZE           512              byte
 *                RD_BUF/MODE           MBUF_USR_CTRL
 *                RD_BUF/TIMEOUT        1000             milli sec.
 *                RD_BUF/HIGHWATER      512              byte
 *                WR_BUF/SIZE           512              byte
 *                WR_BUF/MODE           MBUF_USR_CTRL
 *                WR_BUF/TIMEOUT        1000             milli sec.
 *                WR_BUF/HIGHWATER      512              byte
 *                M99_SRAM_RW_BUF_SIZE  64               byte
 *                ID_CHECK              0                0..1
 *                M99_COUNTER_PRELOAD   250000           10..500000
 *                M99_IRQ_JITTER        0                0..1
 *
 *
 *---------------------------------------------------------------------------
 *  Input......:  descSpec   pointer to descriptor specifier
 *                osHdl      pointer to the os specific structure
 *                maHdl      pointer to access handle
 *                devSemHdl  device semaphore for unblocking in wait
 *                irqHdl     irq handle for mask and unmask interrupts
 *
 *  Output.....:  llHdlP  pointer to low level driver handle
 *                return 0 | error code
 *
 *  Globals....:  ---
 *
 ****************************************************************************/
static int32 M99_Init
(
    DESC_SPEC       *descP,
    OSS_HANDLE      *osHdl,
    MACCESS         *ma,
    OSS_SEM_HANDLE  *DevSem,
    OSS_IRQ_HANDLE  *irqHdl,
    LL_HANDLE       **llHdlP
)
{
    u_int32     i;
    int8       zero = 0;
    M99_HANDLE  *m99Hdl;
    int32       retCode;
    u_int32     gotsize;
    u_int32     inBufferSize;
    u_int32     inBufferTimeout;
    u_int32     outBufferSize;
    u_int32     outBufferTimeout;
    DESC_HANDLE *descHdl;
    int         illModId = 1;
    int         modIdMagic;
    int         modId;
    u_int32     mode;
    u_int32     lowWater;
    u_int32     highWater;
    u_int32     dbgLevelDesc;
    u_int32     dbgLevelMbuf;

    retCode = DESC_Init( descP, osHdl, &descHdl );
    if( retCode )
    {
        return( retCode );
    }/*if*/

    /*-------------------------------------+
    |  LL-HANDLE allocate and initialize   |
    +-------------------------------------*/
    m99Hdl = (M99_HANDLE*) OSS_MemGet( osHdl, sizeof(M99_HANDLE), &gotsize );
    if( m99Hdl == NULL )
    {
       *llHdlP = 0;
       return( ERR_OSS_MEM_ALLOC );
    }/*if*/

    /* set return ll-handle */
    *llHdlP = (LL_HANDLE*) m99Hdl;

    /* fill turkey with 0 */
    OSS_MemFill( osHdl, gotsize, (char*) m99Hdl, zero );

    /* fill up the turkey */
    m99Hdl->OwnMemSize = gotsize;
    m99Hdl->osHdl      = osHdl;
    m99Hdl->maM68230   = ma[MC68230];
    m99Hdl->maSRAM     = ma[SRAM];
    m99Hdl->irqHdl     = irqHdl;
    m99Hdl->nbrOfChannels = M99_MAX_CH;

    /*------------------------------+
    |  init id function table       |
    +------------------------------*/
	/* drivers ident function */
	m99Hdl->idFuncTbl.idCall[0].identCall = M99_Ident;
	/* libraries ident functions */
	m99Hdl->idFuncTbl.idCall[1].identCall = MBUF_Ident;
	m99Hdl->idFuncTbl.idCall[2].identCall = DESC_Ident;
	m99Hdl->idFuncTbl.idCall[3].identCall = OSS_Ident;
	/* terminator */
	m99Hdl->idFuncTbl.idCall[4].identCall = NULL;

	/* prepare debugging */
	DBG_MYLEVEL = DBG_ALL;		/* say: all debug ON */
	DBGINIT((NULL,&DBH));

    /*-------------------------------------+
    |  get DEBUG params                    |
    +-------------------------------------*/
    /* DEBUG_LEVEL_DESC */
    retCode = DESC_GetUInt32( descHdl,
                              DBG_OFF,
                              &dbgLevelDesc,
                              "DEBUG_LEVEL_DESC",
                              NULL );
    if( retCode != 0 && retCode != ERR_DESC_KEY_NOTFOUND ) goto CLEANUP;
	DESC_DbgLevelSet(descHdl, dbgLevelDesc);
    retCode = 0;

    /* DEBUG_LEVEL_MBUF */
    retCode = DESC_GetUInt32( descHdl,
                              DBG_OFF,
                              &dbgLevelMbuf,
                              "DEBUG_LEVEL_MBUF",
                              NULL );
    if( retCode != 0 && retCode != ERR_DESC_KEY_NOTFOUND ) goto CLEANUP;
    retCode = 0;

    /* DEBUG_LEVEL */
    retCode = DESC_GetUInt32( descHdl,
                              DBG_OFF,
                              &m99Hdl->dbgLevel,
                              "DEBUG_LEVEL",
                              NULL );
    if( retCode != 0 && retCode != ERR_DESC_KEY_NOTFOUND ) goto CLEANUP;
    retCode = 0;

    DBGWRT_1((DBH, "LL - M99_Init\n" )  );

    /*-------------------------------------+
    |  get RD_BUF params                   |
    +-------------------------------------*/
    retCode = DESC_GetUInt32( descHdl,
                              M99_DEFAULT_BUF_SIZE,
                              &inBufferSize,
                              "RD_BUF/SIZE",
                              0 );
    if( retCode != 0 && retCode != ERR_DESC_KEY_NOTFOUND ) goto CLEANUP;
    retCode = 0;

    retCode = DESC_GetUInt32( descHdl,
                              M_BUF_USRCTRL,
                              &mode,
                              "RD_BUF/MODE",
                              0 );
    if( retCode != 0 && retCode != ERR_DESC_KEY_NOTFOUND ) goto CLEANUP;
    retCode = 0;

    retCode = DESC_GetUInt32( descHdl,
                              M99_DEFAULT_BUF_TIMEOUT,
                              &inBufferTimeout,
                              "RD_BUF/TIMEOUT",
                              0 );
    if( retCode != 0 && retCode != ERR_DESC_KEY_NOTFOUND ) goto CLEANUP;
    retCode = 0;

    retCode = DESC_GetUInt32( descHdl,
                              M99_DEFAULT_BUF_SIZE,
                              &highWater,
                              "RD_BUF/HIGHWATER",
                              0 );

    retCode = MBUF_Create( osHdl, DevSem, m99Hdl, inBufferSize,
                           M99_CH_WIDTH, mode,
                           MBUF_RD, highWater, inBufferTimeout,
                           m99Hdl->irqHdl,
                           &m99Hdl->inbuf );
    if( retCode ) goto CLEANUP;

	/* set MBUF debug level */
	MBUF_SetStat(m99Hdl->inbuf, NULL, M_BUF_RD_DEBUG_LEVEL, dbgLevelMbuf);

    /*-------------------------------------+
    |  get WR_BUF params                   |
    +-------------------------------------*/
    retCode = DESC_GetUInt32( descHdl,
                              M99_DEFAULT_BUF_SIZE,
                              &outBufferSize,
                              "WR_BUF/SIZE",
                              0 );
    if( retCode != 0 && retCode != ERR_DESC_KEY_NOTFOUND ) goto CLEANUP;
    retCode = 0;

    retCode = DESC_GetUInt32( descHdl,
                              M_BUF_USRCTRL,
                              &mode,
                              "WR_BUF/MODE",
                              0 );
    if( retCode != 0 && retCode != ERR_DESC_KEY_NOTFOUND ) goto CLEANUP;
    retCode = 0;

    retCode = DESC_GetUInt32( descHdl,
                              M99_DEFAULT_BUF_TIMEOUT,
                              &outBufferTimeout,
                              "WR_BUF/TIMEOUT",
                              0 );
    if( retCode != 0 && retCode != ERR_DESC_KEY_NOTFOUND ) goto CLEANUP;
    retCode = 0;

    retCode = DESC_GetUInt32( descHdl,
                              0,
                              &lowWater,
                              "WR_BUF/LOWWATER",
                              0 );

    retCode = MBUF_Create( osHdl, DevSem, m99Hdl, outBufferSize,
                           M99_CH_WIDTH, mode,
                           MBUF_WR , lowWater, outBufferTimeout,
                           m99Hdl->irqHdl,
                           &m99Hdl->outbuf );
    if( retCode ) goto CLEANUP;

	/* set MBUF debug level */
	MBUF_SetStat(NULL, m99Hdl->outbuf, M_BUF_WR_DEBUG_LEVEL, dbgLevelMbuf);

    /*-------------------------------------+
    |                                      |
    +-------------------------------------*/
    /* SRAM BUFFER ========== */
    retCode = DESC_GetUInt32( descHdl,
                              M99_SRAM_RW_BUF_SIZE,
                              &m99Hdl->RWbufSize,
                              "M99_SRAM_RW_BUF_SIZE",
                              NULL );
    if( retCode != 0  && retCode != ERR_DESC_KEY_NOTFOUND ) goto CLEANUP;
    retCode = 0;

    m99Hdl->rd_offs   = 0;
    m99Hdl->wr_offs   = m99Hdl->RWbufSize;



    /*-------------------------------------+
    |                                      |
    +-------------------------------------*/
    /* descriptor - use module id ? */
    retCode = DESC_GetUInt32( descHdl,
                            M99_DONT_ID_CHECK,
                            &m99Hdl->useModulId,
                            "ID_CHECK",
                            NULL );
    if( retCode != 0 && retCode != ERR_DESC_KEY_NOTFOUND ) goto CLEANUP;
    retCode = 0;


    illModId = 0;
    if( m99Hdl->useModulId != M99_DONT_ID_CHECK )
    {
        modIdMagic = (u_int16)m_read((U_INT32_OR_64)m99Hdl->maM68230, 0);
        modId      = (u_int16)m_read((U_INT32_OR_64)m99Hdl->maM68230, 1);
        if( modIdMagic != MOD_ID_MAGIC_WORD )
        {
             DBGWRT_1((DBH, "    id - illegal magic word\n" )  );
             retCode = ERR_LL_ILL_ID;
             illModId = 1;
             goto CLEANUP;
        }/*if*/

        if( modId != M99_MOD_ID )
        {
             DBGWRT_1((DBH, "    id - illegal module id\n" )  );
             retCode = ERR_LL_ILL_ID;
             illModId = 1;
             goto CLEANUP;
        }/*if*/
    }/*if*/


    /*-------------------------------------+
    |                                      |
    +-------------------------------------*/
    /* fill sram read buffer with dummy values */
    for( i=0; i < (m99Hdl->RWbufSize/2); i++ )
        MWRITE_D16( m99Hdl->maSRAM, (m99Hdl->rd_offs+(2*i)), i );  /* read buf: 0..RW_BUF_SIZE */


    /* fill sram write buffer with dummy values */
    MBLOCK_SET_D16( (m99Hdl->maSRAM),
                    (m99Hdl->wr_offs),
                    ((int)m99Hdl->RWbufSize), 0xa55a);



    /*-------------------------------------+
    |                                      |
    +-------------------------------------*/
    /* setup MC68230 ========== */
    MWRITE_D16( m99Hdl->maM68230, PGC_REG,  zero );
    MWRITE_D16( m99Hdl->maM68230, PAD_REG,  0xaa );
    MWRITE_D16( m99Hdl->maM68230, PADD_REG, 0xff );
    MWRITE_D16( m99Hdl->maM68230, PBDD_REG, 0x00 );
    MWRITE_D16( m99Hdl->maM68230, PAC_REG,  zero );
    MWRITE_D16( m99Hdl->maM68230, PBC_REG,  zero );
    MWRITE_D16( m99Hdl->maM68230, TIV_REG,  0x0f ); /* like default after reset */

    /* medium irq frequenz */
    retCode = DESC_GetUInt32( descHdl,
                              M99_CLOCK_FREQ,     /* 1 irq per sec default */
                              &m99Hdl->medPreLoad,
                              "M99_COUNTER_PRELOAD",
                              NULL );
    if( retCode != 0 && retCode != ERR_DESC_KEY_NOTFOUND ) goto CLEANUP;
    retCode = 0;

    /* irq time jitter mode */
    retCode = DESC_GetUInt32( descHdl,
                              M99_JITTER_OFF,
                              &m99Hdl->jittermode,
                              "M99_IRQ_JITTER",
                              NULL );
    if( retCode != 0 && retCode != ERR_DESC_KEY_NOTFOUND ) goto CLEANUP;
    retCode = 0;

	m99Hdl->irqLatency = 0xffffffff;
	m99Hdl->maxIrqLatency = 0xffffffff;

    /* preload timer register */
    m99Hdl->laststep   = -1;
    setTime( m99Hdl, m99Hdl->medPreLoad );

    MWRITE_D16( m99Hdl->maM68230, TC_REG, 0x81); /* timer irq (disabled) */

    DESC_Exit( &descHdl );
    return( retCode );

 CLEANUP:
    DESC_Exit( &descHdl );
    if( !illModId )
    {
    	LL_HANDLE **llHdlP;
    	llHdlP = (LL_HANDLE**)&m99Hdl;
        M99_Exit( llHdlP );
    }
    else
    {
		OSS_MemFree(osHdl, m99Hdl, m99Hdl->OwnMemSize);
    }/*if*/

    return( retCode );

}/*M99_Init*/

/****************************** M99_Exit *************************************
 *
 *  Description:  Deinits HW, disables interrupts, frees allocated memory.
 *
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdlP  pointer to low level driver handle
 *
 *  Output.....:  llHdlP  NULL
 *                return 0 | error code
 *
 *  Globals....:  ---
 *
 ****************************************************************************/
static int32 M99_Exit
(
   LL_HANDLE    **llHdlP
)
{
    M99_HANDLE*       m99Hdl;
    int i;


    m99Hdl = (M99_HANDLE*) *llHdlP;

    DBGWRT_1((DBH, "LL - M99_Exit\n" )  );

    /* disable IRQ's */
    MWRITE_D16( m99Hdl->maM68230, TC_REG, 0x81 );    /* timer irq (disabled) */


    /* clear LED's */
    MWRITE_D16( m99Hdl->maM68230, PAD_REG, 0xff );

    if( m99Hdl->inbuf )
       MBUF_Remove( &m99Hdl->inbuf );

    if( m99Hdl->outbuf )
       MBUF_Remove( &m99Hdl->outbuf );

    /* deinit lldrv memory */
    for( i = 0; i < M99_MAX_SIGNALS; i++ )
    {
        if( m99Hdl->cond[i] != NULL )
           OSS_SigRemove( m99Hdl->osHdl, &m99Hdl->cond[i] );
    }/*for*/

	/* cleanup debug */
	DBGEXIT((&DBH));

    /* dealloc lldrv memory */
    if( OSS_MemFree( m99Hdl->osHdl, (int8*) m99Hdl, m99Hdl->OwnMemSize ) )
    {
       return( -1 );
    }

    *llHdlP = 0;

    return(0);
}/*M99_Exit*/

/****************************** M99_Read *************************************
 *
 *  Description:  Reads at least one word from SRAM and increment the
 *                read offset.
 *
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl  pointer to ll-drv data structure
 *                ch     current channel
 *
 *  Output.....:  valueP read value
 *                return 0 | error code
 *
 *  Globals....:  ---
 *
 ****************************************************************************/
static int32 M99_Read
(
    LL_HANDLE *llHdl,
    int32 ch,
    int32 *value
)
{
    M99_HANDLE*       m99Hdl = (M99_HANDLE*) llHdl;

    DBGWRT_1((DBH, "LL - M99_Read\n" )  );
    DBGWRT_2((DBH, "     M99_Read: from ch=%d\n",ch )  );

    *(u_int16*)value = MREAD_D16(m99Hdl->maSRAM, m99Hdl->rd_offs );

    m99Hdl->rd_offs +=2;

    if( m99Hdl->rd_offs >= m99Hdl->RWbufSize )
        m99Hdl->rd_offs = 0;

    return(0);
}/*M99_Read*/

/****************************** M99_Write ************************************
 *
 *  Description:  Writes at least one word to SRAM and increment the
 *                write offset.
 *
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl  pointer to ll-drv data structure
 *                ch     current channel
 *                value  switch on/off
 *
 *  Output.....:  return 0 | error code
 *
 *  Globals....:  ---
 *
 ****************************************************************************/
static int32 M99_Write
(
    LL_HANDLE *llHdl,
    int32 ch,
    int32 value
)
{
    M99_HANDLE*       m99Hdl = (M99_HANDLE*) llHdl;

    DBGWRT_1((DBH, "LL - M99_Write\n" )  );
    DBGWRT_2((DBH, "     M99_Write: value=0x%08x to ch=%d\n", value, ch )  );

    MWRITE_D16(m99Hdl->maSRAM, m99Hdl->wr_offs, value);

    m99Hdl->wr_offs +=2;

    if( m99Hdl->wr_offs >= (m99Hdl->RWbufSize*2) )
        m99Hdl->wr_offs = m99Hdl->RWbufSize;

    MWRITE_D16( m99Hdl->maM68230, PAD_REG, value );
    return(0);
}/*M99_Write*/

/****************************** M99_SetStat **********************************
 *
 *  Description:  Changes the device state.
 *
 *               code                      values
 *                M_MK_IRQ_ENABLE           0..1
 *                M_LL_DEBUG_LEVEL          see oss.h
 *                M_LL_IRQ_COUNT            irq count
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl         pointer to ll-drv data structure
 *                code          setstat code
 *                ch            current channel
 *                value32_or_64 setstat value or pointer to blocksetstat data
 *
 *  Output.....:  return 0 | error code
 *
 *  Globals....:  ---
 *
 ****************************************************************************/
static int32 M99_SetStat
(
    LL_HANDLE *llHdl,
    int32  code,
    int32  ch,
    INT32_OR_64  value32_or_64
)
{
    register    u_int8 tc_reg;
    int32       cond;
    M99_HANDLE*	m99Hdl = (M99_HANDLE*) llHdl;
    int32       retCode;
    int32	value = (int32)value32_or_64;	/* 32bit value		       */
    INT32_OR_64	valueP	= value32_or_64;	/* stores 32/64bit pointer     */

    DBGWRT_1((DBH, "LL - M99_SetStat\n" )  );
    DBGWRT_2((DBH, "     M99_SetStat: code=$%04x\n",code )  );

    switch(code)
    {
	    case M99_MAX_IRQ_LAT:
	    m99Hdl->maxIrqLatency = value;
	    break;
        /*--------------------------+
        |  debug level              |
        +--------------------------*/
        case M_LL_DEBUG_LEVEL:
            m99Hdl->dbgLevel = value;
            break;
        /*--------------------------+
        |  enable jitter mode       |
        +--------------------------*/
        case M99_JITTER:
            m99Hdl->jittermode = value;
            setTime(m99Hdl, m99Hdl->timerval);
            break;
        /*--------------------------+
        |  define irq rate          |
        +--------------------------*/
        case M99_TIMERVAL:
            if( value<1 || 0xffffff<value )           /* illgal timer value ? */
                return(ERR_LL_ILL_PARAM);

            m99Hdl->medPreLoad = value;
            m99Hdl->laststep = -1;

            tc_reg = (u_int8)MREAD_D16( m99Hdl->maM68230, TC_REG );      /* get timer control */
            MWRITE_D16( m99Hdl->maM68230, TC_REG, tc_reg & 0xfe);/* timer halt */
            setTime( m99Hdl, value );                              /* load timer */
            MWRITE_D16( m99Hdl->maM68230, TS_REG, 0x01 );        /* timer reset */
            MWRITE_D16( m99Hdl->maM68230, TC_REG, tc_reg );       /* timer control restore */
            break;
        /*--------------------------+
        |  enable interrupts        |
        +--------------------------*/
        case M_MK_IRQ_ENABLE:
            if( value ) {
                MWRITE_D16( m99Hdl->maM68230, TC_REG, 0xa1 );    /* timer irq (enabled) */
            }
            else {
                MWRITE_D16( m99Hdl->maM68230, TC_REG, 0x81 );    /* timer irq (disabled) */
				MWRITE_D16(m99Hdl->maM68230, TS_REG, 0xff);  /* clear interrupt */
			}
            break;
        /*--------------------------+
        |  set/clr signal cond 1..4 |
        +--------------------------*/
        case M99_SIG_set_cond1:
        case M99_SIG_set_cond2:
        case M99_SIG_set_cond3:
        case M99_SIG_set_cond4:
		{
#ifdef MASK_IRQ_ILLEGAL
		   OSS_IRQ_STATE irqState;
           irqState = OSS_IrqMaskR( m99Hdl->osHdl, m99Hdl->irqHdl );  /* DISABLE irqs */
#endif
		   cond = code - (M99_SIG_set_cond1);    /* condition nr */
           if( m99Hdl->cond[cond] != NULL )	     /* already defined ? */
           {
               retCode = ERR_OSS_SIG_SET;        /* can't set ! */
           }
           else
           {
               retCode = OSS_SigCreate( m99Hdl->osHdl, value, &m99Hdl->cond[cond]);
           }/*if*/
#ifdef MASK_IRQ_ILLEGAL
           OSS_IrqRestore( m99Hdl->osHdl, m99Hdl->irqHdl, irqState );  /* ENABLE irqs */
#endif
           return( retCode );
           break;
		}
        case M99_SIG_clr_cond1:
        case M99_SIG_clr_cond2:
        case M99_SIG_clr_cond3:
        case M99_SIG_clr_cond4:
           cond = code - (M99_SIG_clr_cond1);       /* condition nr */

           if( m99Hdl->cond[cond] == NULL )   /* not defined ? */
           {
               retCode = ERR_OSS_SIG_SET;        /* can't clr ! */
           }
           else
           {
			   retCode = (OSS_SigRemove( m99Hdl->osHdl, &m99Hdl->cond[cond] ) );
           }/*if*/
           return( retCode );
           break;

        /*--------------------------+
        |  (unknown)                |
        +--------------------------*/
        default:
            if(    ( M_RDBUF_OF <= code && code <= (M_WRBUF_OF+0x0f) )
                || ( M_RDBUF_BLK_OF <= code && code <= (M_RDBUF_BLK_OF+0x0f) )
              )
                return( MBUF_SetStat( m99Hdl->inbuf,
                                      m99Hdl->outbuf,
                                      code,
                                      value ) );

            if( M_LL_BLK_OF <= code && code <= (M_DEV_BLK_OF + 0xff)  )
                return( setStatBlock( m99Hdl, code, (M_SETGETSTAT_BLOCK*) valueP ) );


            return(ERR_LL_UNK_CODE);
    }/*switch*/

    return(0);
}/*M99_SetStat*/

/****************************** M99_GetStat **********************************
 *
 *  Description:  Changes the device state.
 *
 *               code                  *valueP
 *                M_LL_CH_NUMBER        32
 *                M_LL_CH_DIR           direction of curr ch
 *                M_LL_CH_LEN           length in bit
 *                M_LL_CH_TYP           M_CH_BINARY
 *                M_LL_IRQ_COUNT        irq count
 *                M_LL_ID_CHECK         eeprom id is checked in M99_Init
 *                M_LL_DEBUG_LEVEL      see oss.h
 *                M_MK_BLK_REV_ID       pointer to the ident function table
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl  pointer to ll-drv data structure
 *                code   setstat code
 *                ch     current channel
 *                value32_or_64P  pointer to getstat value or pointer to blocksetstat data
 *
 *  Output.....:  value32_or_64P  pointer to getstat value or pointer to blocksetstat data
 *                return  0 | error code
 *
 *  Globals....:  ---
 *
 ****************************************************************************/
static int32 M99_GetStat
(
    LL_HANDLE *llHdl,
    int32  code,
    int32  ch,
    INT32_OR_64  *value32_or_64P
)
{
    int32       cond;
    int32       processId;
    int32	*valueP = (int32*)value32_or_64P;	/* pointer to 32bit value      */
    INT32_OR_64	*value64P = value32_or_64P;		/* stores 32/64bit pointer     */
    //M_SG_BLOCK	*blk      = (M_SG_BLOCK*)value32_or_64P;/* stores block struct pointer */

    M99_HANDLE*   m99Hdl = (M99_HANDLE*)llHdl;

    DBGWRT_1((DBH, "LL - M99_GetStat\n" )  );
    DBGWRT_2((DBH, "     M99_GetStat: code=%x\n", code )  );

    switch(code)
    {
	    case M99_IRQ_LAT:
			*valueP = m99Hdl->irqLatency;
			break;
		/* get timer ticks elapsed since last irq */
	    case M99_GET_TIME:
		{
			OSS_IRQ_STATE irqState;
			irqState = OSS_IrqMaskR( m99Hdl->osHdl, m99Hdl->irqHdl );  /* DISABLE irqs */
			*valueP = m99Hdl->timerval - getTime( m99Hdl );
			OSS_IrqRestore( m99Hdl->osHdl, m99Hdl->irqHdl, irqState );
			break;
		}
	    case M99_MAX_IRQ_LAT:
			*valueP = m99Hdl->maxIrqLatency;
			break;
        /*------------------+
        |  get ch count     |
        +------------------*/
        case M_LL_CH_NUMBER:
            *valueP = m99Hdl->nbrOfChannels;
            break;

        /*------------------+
        |  irq count        |
        +------------------*/
        case M_LL_IRQ_COUNT:
            *valueP = m99Hdl->irqCount;
            break;

        /*------------------+
        |  id check enabled |
        +------------------*/
        case M_LL_ID_CHECK:
            *valueP = m99Hdl->useModulId;
            break;

        /*------------------+
        |  debug level       |
        +------------------*/
        case M_LL_DEBUG_LEVEL:
            *valueP = m99Hdl->dbgLevel;
            break;

        /*--------------------+
        |  ident table        |
        +--------------------*/
        case M_MK_BLK_REV_ID:
           *value64P = (INT32_OR_64)&m99Hdl->idFuncTbl;
           break;

        /* ------ special getstat codes --------- */
        /*--------------------------+
        |  get jitter mode          |
        +--------------------------*/
        case M99_JITTER:
            *valueP = m99Hdl->jittermode;
            break;
        /*--------------------------+
        |  get current irq rate     |
        +--------------------------*/
        case M99_TIMERVAL:
            *valueP = m99Hdl->timerval;
            break;
        /*--------------------------+
        |  get irq counter          |
        +--------------------------*/
        case M99_IRQCOUNT:
            *valueP = m99Hdl->irqCount;
            break;
        /*--------------------------+
        |  set     signal cond 1..4 |
        +--------------------------*/
        case M99_SIG_set_cond1:
        case M99_SIG_set_cond2:
        case M99_SIG_set_cond3:
        case M99_SIG_set_cond4:
           cond = code - (M99_SIG_set_cond1);      /* condition nr */
           if( m99Hdl->cond[cond] == NULL )   /* not defined ? */
           {
			   *valueP = 0;
           }
           else
           {
			   OSS_SigInfo( m99Hdl->osHdl, m99Hdl->cond[cond], valueP, &processId);           }/*if*/

           break;

        /*--------------------------+
        |  (unknown)                |
        +--------------------------*/
        default:
           if(    ( M_RDBUF_OF <= code && code <= (M_WRBUF_OF+0x0f) )
               || ( M_RDBUF_BLK_OF <= code && code <= (M_RDBUF_BLK_OF+0x0f) )
             )
               return( MBUF_GetStat( m99Hdl->inbuf,
                                     m99Hdl->outbuf,
                                     code,
                                     valueP ) );

           if( M_LL_BLK_OF <= code )
               return( getStatBlock( m99Hdl, code, (M_SETGETSTAT_BLOCK*) value64P ) );

           return(ERR_LL_UNK_CODE);
    }/*switch*/

    return(0);
}/*M99_GetStat*/

/******************************* M99_BlockRead *******************************
 *
 *  Description:  Reads all channel to buffer. ( see also M99_HwBlockRead() )
 *                Supported modes:  M_BUF_USRCTRL         reads from hw
 *                                  M_BUF_RINGBUF         reads from buffer
 *                                  M_BUF_RINGBUF_OVERWR   "     "     "
 *                                  M_BUF_CURRBUF          "     "     "
 *
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl     pointer to ll-drv data structure
 *                ch        current channel
 *                buf       buffer to store read values
 *                size      should be multiple of width
 *
 *  Output.....:  nbrRdBytesP  number of read bytes
 *                return       0 | error code
 *
 *  Globals....:  ---
 *
 ****************************************************************************/
static int32 M99_BlockRead
(
     LL_HANDLE *llHdl,
     int32     ch,
     void      *buf,
     int32     size,
     int32     *nbrRdBytesP
)
{
    M99_HANDLE*  m99Hdl = (M99_HANDLE*)llHdl;
    int32 fktRetCode;
    int32 bufMode;

    DBGWRT_1((DBH, "LL - M99_BlockRead\n" )  );

    fktRetCode   = 0;
    *nbrRdBytesP = 0;

    fktRetCode = MBUF_GetBufferMode( m99Hdl->inbuf , &bufMode );
    if( fktRetCode != 0 && fktRetCode != ERR_MBUF_NO_BUF )
    {
       return( fktRetCode );
    }/*if*/

    switch( bufMode )
    {

        case M_BUF_USRCTRL:
           size = M99_HwBlockRead( m99Hdl, buf, size );
           *nbrRdBytesP = size;
           fktRetCode   = 0; /* ovrwr ERR_MBUF_NO_BUFFER */
           break;

        case M_BUF_RINGBUF:
        case M_BUF_RINGBUF_OVERWR:
        case M_BUF_CURRBUF:
        default:
           fktRetCode = MBUF_Read( m99Hdl->inbuf, (u_int8*) buf, size,
                                   nbrRdBytesP );

    }/*switch*/

    return( fktRetCode );

}/*M99_BlockRead*/

/***************************** M99_HwBlockRead ******************************
 *
 *  Description:
 *
 *
 *---------------------------------------------------------------------------
 *  Input......:
 *
 *  Output.....:
 *
 *  Globals....:  ---
 *
 ****************************************************************************/
static int32 M99_HwBlockRead
(
     M99_HANDLE  *m99Hdl,
     void  *buf,
     int32 size
)
{
    int32  i;
    u_int16  *hlpP;

    hlpP = (u_int16*) buf;

    for(i=0; i< (size/2); i++)
    {
        *hlpP = MREAD_D16(m99Hdl->maSRAM, m99Hdl->rd_offs);
        m99Hdl->rd_offs +=2;
        hlpP++;

        if( m99Hdl->rd_offs >= m99Hdl->RWbufSize )
            m99Hdl->rd_offs = 0;
    }/*for*/

    return(i);
}/*M99_HwBlockRead*/

/****************************** M99_BlockWrite *******************************
 *
 *  Description:  Writes maximal 32 channels.
 *                Supported modes:  M_BUF_USRCTRL         writes to hw
 *                                  M_BUF_RINGBUF         writes to buffer
 *                                  M_BUF_RINGBUF_OVERWR   "     "     "
 *                                  M_BUF_CURRBUF          "     "     "
 *
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl  pointer to ll-drv data structure
 *                ch     current channel
 *                buf    buffer to store read values
 *                size   should be multiple of width
 *
 *  Output.....:  nbrWrBytesP  number of written bytes
 *                return       0 | error code
 *
 *  Globals....:  ---
 *
 ****************************************************************************/
static int32 M99_BlockWrite
(
     LL_HANDLE *llHdl,
     int32     ch,
     void      *buf,
     int32     size,
     int32     *nbrWrBytesP
)
{
    M99_HANDLE*  m99Hdl = (M99_HANDLE*)llHdl;
    int32 fktRetCode;
    int32 bufMode;

    *nbrWrBytesP = 0;

    DBGWRT_1((DBH, "LL - M99_BlockWrite: buf %08p  size %d\n", buf, size )  );

    fktRetCode = MBUF_GetBufferMode( m99Hdl->outbuf , &bufMode );
    if( fktRetCode != 0 && fktRetCode != ERR_MBUF_NO_BUF )
    {
       return( fktRetCode );
    }/*if*/

    switch( bufMode )
    {

        case M_BUF_USRCTRL:
           size = M99_HwBlockWrite( m99Hdl, buf, size );
           *nbrWrBytesP = size;
           fktRetCode   = 0; /* ovrwr ERR_MBUF_NO_BUFFER */
           break;

        case M_BUF_RINGBUF:
        case M_BUF_RINGBUF_OVERWR:
        case M_BUF_CURRBUF:
        default:
           fktRetCode = MBUF_Write( m99Hdl->outbuf, (u_int8*)buf, size,
                                    nbrWrBytesP );
    }/*switch*/

    DBGWRT_2((DBH, "    return %08x  nbrWrBytes %d\n", fktRetCode, *nbrWrBytesP)  );
    return( fktRetCode );
}/*M99_BlockWrite*/

/********************************** ? ***************************************
 *
 *  Description:
 *
 *
 *---------------------------------------------------------------------------
 *  Input......:
 *
 *  Output.....:
 *
 *  Globals....:  ---
 *
 ****************************************************************************/
static int32 M99_HwBlockWrite
(
    M99_HANDLE  *m99Hdl,
    void  *buf,
    int32 size
)
{
    u_int16  *bufptr = (u_int16*) buf;                /* ptr to buffer */

    MWRITE_D16(m99Hdl->maSRAM, m99Hdl->wr_offs, *bufptr);

    m99Hdl->wr_offs += 2;

    if( m99Hdl->wr_offs == (m99Hdl->RWbufSize*2) )
        m99Hdl->wr_offs = m99Hdl->RWbufSize;

    MWRITE_D16( m99Hdl->maM68230, PAD_REG, *bufptr );

    return(2);
}/*M99_HwBlockWrite*/


/****************************** M99_Irq *************************************
 *
 *  Description:  The irq routine reads all channels.
 *                If signal is installed
 *                this is sended.
 *                It clears the module irq.
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl  pointer to ll-drv data structure
 *
 *  Output.....:  return LL_IRQ_DEV_NOT | LL_IRQ_DEVICE
 *
 *  Globals....:  ---
 *
 ****************************************************************************/
static int32 M99_Irq
(
   LL_HANDLE *llHdl
)
{
    register void  *buf=NULL;
    register int32 count;
    M99_HANDLE     *m99Hdl = (M99_HANDLE*) llHdl;
    int32          gotsize;
    u_int8         isrFired;
	u_int32 	   tval;
    OSS_IRQ_STATE  irqState1, irqState2;

    IDBGWRT_1((DBH, ">> m99_irq_c:\n" )  );

    isrFired = (u_int8)MREAD_D16(m99Hdl->maM68230, TS_REG);  /* interrupt from M68230 */

    if( !isrFired )
    {
        /* interrupt not from module */
        return( LL_IRQ_DEV_NOT);
    }/*if*/

    /* check the IRQ Mask and Spinlock implementation */
    /* call the methods twice to check if double calls cause problems */
    irqState1 = OSS_IrqMaskR( m99Hdl->osHdl, m99Hdl->irqHdl );
    irqState2 = OSS_IrqMaskR( m99Hdl->osHdl, m99Hdl->irqHdl );
    OSS_IrqRestore( m99Hdl->osHdl, m99Hdl->irqHdl, irqState2 );
    OSS_IrqRestore( m99Hdl->osHdl, m99Hdl->irqHdl, irqState1 );

	/* get elapsed time since timer expired */
	tval = m99Hdl->timerval - getTime( m99Hdl );
	IDBGWRT_3((DBH, " tval=0x%06x\n", tval )  );

	if( tval > m99Hdl->maxIrqLatency )
		m99Hdl->maxIrqLatency = tval;
	m99Hdl->irqLatency = tval;

    MWRITE_D16(m99Hdl->maM68230, TS_REG, 0xff);  /* clear interrupt */

    /*------------------+
    | send signal       |
    +------------------*/
    count  = m99Hdl->irqCount & 0x03;        /* 0..3 counter */

    if( m99Hdl->cond[count] != NULL )   	/* signal installed ? */
		OSS_SigSend( m99Hdl->osHdl, m99Hdl->cond[count] );

    /*------------------+
    | read from SRAM    |
    +------------------*/
    if( (buf = MBUF_GetNextBuf( m99Hdl->inbuf, 1, &gotsize)) != 0 )
    {
        M99_HwBlockRead( m99Hdl, buf, 1 );         /* read block into buf */
        MBUF_ReadyBuf( m99Hdl->inbuf );                 /* blockread ready */
    }

    /*------------------+
    | write to SRAM     |
    +------------------*/
    if( (buf = MBUF_GetNextBuf( m99Hdl->outbuf, 1, &gotsize)) != 0 )
    {
        M99_HwBlockWrite( m99Hdl, buf, 1 );        /* write block from buf */
        MBUF_ReadyBuf( m99Hdl->outbuf );
    }

    /*------------------+
    | toggle LED 0      |
    | (if no block-i/o) |
    +------------------*/
    if (!buf)
        MWRITE_D16( m99Hdl->maM68230, PAD_REG, ~(m99Hdl->irqCount ));

    /*------------------+
    | calc new irq rate |
    +------------------*/
    if( m99Hdl->jittermode )
    {
        dostep(m99Hdl);
    }

    m99Hdl->irqCount++;

    return( LL_IRQ_DEVICE );
}/*M99_Irq*/

/****************************** M99_Info ************************************
 *
 *  Description:  Gets low level driver infos.
 *
 *                NOTE: is callable before MXX_Init().
 *
 *---------------------------------------------------------------------------
 *  Input......:  infoType
 *                ...
 *
 *  Output.....:  0 | error code
 *
 *  Globals....:  ---
 *
 ****************************************************************************/
static int32 M99_Info
(
   int32  infoType,
   ...
)
{
    int32   error;
    va_list argptr;
    u_int32 *nbrOfAddrSpaceP;
    u_int32 *addrModeP;
    u_int32 *dataModeP;
    u_int32 *addrSizeP;
    u_int32 *useIrqP;
    u_int32 addrSpaceIndex;


    error = 0;
    va_start( argptr, infoType );

    switch( infoType )
    {
        case LL_INFO_HW_CHARACTER:
          addrModeP  = va_arg( argptr, u_int32* );
          dataModeP  = va_arg( argptr, u_int32* );
          *addrModeP = MDIS_MA08;
          *dataModeP = MDIS_MD08;
          break;

        case LL_INFO_ADDRSPACE_COUNT:
          nbrOfAddrSpaceP = va_arg( argptr, u_int32* );
          *nbrOfAddrSpaceP = 1;
          break;

        case LL_INFO_ADDRSPACE:
          addrSpaceIndex = va_arg( argptr, u_int32 );
          addrModeP  = va_arg( argptr, u_int32* );
          dataModeP  = va_arg( argptr, u_int32* );
          addrSizeP  = va_arg( argptr, u_int32* );

          switch( addrSpaceIndex )
          {
              case 0:
                *addrModeP = MDIS_MA08;
                *dataModeP = MDIS_MD16;
                *addrSizeP = 0x100;
                break;

              default:
                 error = ERR_LL_ILL_PARAM;
          }/*switch*/
          break;

        case LL_INFO_IRQ:
          useIrqP  = va_arg( argptr, u_int32* );
          *useIrqP = 1;
          break;

		case LL_INFO_LOCKMODE:
		{
			u_int32 *lockModeP = va_arg(argptr, u_int32*);

			*lockModeP = LL_LOCK_CALL;
		}
		break;

        default:
          error = ERR_LL_ILL_PARAM;
    }/*switch*/

    va_end( argptr );
    return( error );
}/*M99_Info*/

/******************************* setTime *************************************
 *
 *  Description:  Load timer value
 *
 *---------------------------------------------------------------------------
 *  Input......:  m99Hdl ll drv handle
 *                timerval  timer value
 *  Output.....:  -
 *  Globals....:  -
 ****************************************************************************/
static void setTime
(
    M99_HANDLE *m99Hdl,
    int32      timerval
)
{
    m99Hdl->timerval = timerval;

    MWRITE_D16( m99Hdl->maM68230, CPL_REG, timerval       & 0xff);
    MWRITE_D16( m99Hdl->maM68230, CPM_REG, timerval >> 8  & 0xff);
    MWRITE_D16( m99Hdl->maM68230, CPH_REG, timerval >> 16 & 0xff);
}/*setTime*/

static u_int32 getTime( M99_HANDLE *m99Hdl )
{
	u_int32 low1, low2, mid, high;
	MACCESS ma = m99Hdl->maM68230;

	do {
		low1 = MREAD_D16( ma, CNTL_REG ) & 0xff;
		mid  = MREAD_D16( ma, CNTM_REG ) & 0xff;
		high = MREAD_D16( ma, CNTH_REG ) & 0xff;
		low2 = MREAD_D16( ma, CNTL_REG ) & 0xff;
		/*DBGWRT_3((DBH,"h=%02x m=%02x l1=%02x l2=%02x\n",
		  high, mid, low1, low2 ));*/
	} while( low2 > low1 );

	return ((u_int32)(high<<16)) + ((u_int32)(mid<<8)) + low1;
}

/******************************* dostep *************************************
 *
 *  Description:  Jitter (step) + load new timer value
 *
 *---------------------------------------------------------------------------
 *  Input......:  m99Hdl ll drv handle
 *  Output.....:  -
 *  Globals....:  -
 ****************************************************************************/
static void dostep
(
    M99_HANDLE*       m99Hdl
)
{
    int32 end;

    if( m99Hdl->timerval >= (m99Hdl->medPreLoad<<1)-1 )
        end = m99Hdl->medPreLoad>>1;
    else if( m99Hdl->timerval <= (m99Hdl->medPreLoad>>1)+1 )
        end = m99Hdl->medPreLoad<<1;
    else
        end = m99Hdl->laststep > 0 ? m99Hdl->medPreLoad<<1 : m99Hdl->medPreLoad>>1;

    m99Hdl->laststep = (end - (int32)m99Hdl->timerval)/2;

    IDBGWRT_2((DBH,"step=%08x time=%08x\n", m99Hdl->laststep, m99Hdl->timerval+m99Hdl->laststep) );

    setTime( m99Hdl, m99Hdl->timerval+m99Hdl->laststep );
}/*dostep*/

/**************************** setStatBlock ***********************************
 *
 *  Description:  decodes the M_SETGETSTAT_BLOCK struct code and executes them.
 *                The known codes are:
 *
 *                   M99_SETGETSRAM   size bytes of the data buffer will copied
 *                                    to the sram. It starts from begin of
 *                                    sram. ( size max 128 )
 *
 *---------------------------------------------------------------------------
 *  Input......:  blockStruct    the struct with code size and data buffer
 *
 *  Output.....:  0 | error code
 *
 *  Globals....:  ---
 *
 ****************************************************************************/
static int32 setStatBlock
(
    M99_HANDLE         *m99Hdl,
    int32              code,
    M_SETGETSTAT_BLOCK *blockStruct
)
{
   int32 error;

   switch( code )
   {
       case M99_SETGET_BLOCK_SRAM:
          if( blockStruct->size > 128 )
              error = ERR_LL_ILL_PARAM;
          else
          {
              /* copy data buffer to sram */
              MBLOCK_WRITE_D16( m99Hdl->maSRAM,
                                0,                /* start at begin of sram */
                                blockStruct->size,
                                (u_int16 *)blockStruct->data );

              error = 0;
          }/*if*/
          break;

       default:
          error = ERR_LL_UNK_CODE;
   }/*switch*/

   return( error );
}/*setStatBlock*/

/************************** getStatBlock *************************************
 *
 *  Description:  decodes the M_SETGETSTAT_BLOCK struct code and executes them.
 *                The known codes are:
 *
 *                   M99_SETGETSRAM   size bytes of the sram will copied to the
 *                                    data buffer. It starts from begin of
 *                                    sram.
 *
 *---------------------------------------------------------------------------
 *  Input......:  blockStruct    the struct with code size and data buffer
 *
 *  Output.....:  blockStruct->size  gotten size of data
 *                blockStruct->data  filled data buffer
 *                return - 0 | error code
 *
 *  Globals....:  ---
 *
 ****************************************************************************/
static int32 getStatBlock
(
    M99_HANDLE         *m99Hdl,
    int32              code,
    M_SETGETSTAT_BLOCK *blockStruct
)
{
   int32 error;


   switch( code )
   {
       case M99_SETGET_BLOCK_SRAM:
          if( blockStruct->size > 128 )
              error = ERR_LL_ILL_PARAM;
          else
          {
              /* copy data buffer to sram */
              MBLOCK_READ_D16( m99Hdl->maSRAM,
                               0,                /* start at begin of sram */
                               blockStruct->size,
                               (u_int16 *)blockStruct->data );

              error = 0;
          }/*if*/
          break;

       default:
          error = ERR_LL_UNK_CODE;
   }/*switch*/

   return( error );
}/*getStatBlock*/


