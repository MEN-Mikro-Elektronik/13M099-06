#***************************  M a k e f i l e  *******************************
#
#        $Author: cs $
#          $Date: 2004/05/05 14:11:43 $
#      $Revision: 1.3 $
#        $Header: /dd2/CVSR/COM/DRIVERS/MDIS_LL/M099/DRIVER/COM/driver.mak,v 1.3 2004/05/05 14:11:43 cs Exp $
#
#    Description: makefile descriptor file for common
#                 modules MDIS 4.0   e.g. low level driver
#
#---------------------------------[ History ]---------------------------------
#
#   $Log: driver.mak,v $
#   Revision 1.3  2004/05/05 14:11:43  cs
#   added switch MAK_SWITCH=$(SW_PREFIX)MAC_MEM_MAPPED
#   removed needless PLD lib
#   removed needless switch MAK_OPTIM=$(OPT_1)
#
#   Revision 1.2  1998/05/28 10:02:23  see
#   missing dbg.h added
#   missing DBG lib added
#   missing ID lib added
#   mbuf lib must linked before oss lib
#
#   Revision 1.1  1998/03/27 16:37:38  Franke
#   initial
#
#   Revision 1.1  1998/02/04 17:40:09  uf
#   initial
#
#-----------------------------------------------------------------------------
#   (c) Copyright 1997 by MEN mikro elektronik GmbH, Nuernberg, Germany
#*****************************************************************************

MAK_NAME=m99

MAK_SWITCH=$(SW_PREFIX)MAC_MEM_MAPPED \

MAK_LIBS=$(LIB_PREFIX)$(MEN_LIB_DIR)/desc$(LIB_SUFFIX)     \
         $(LIB_PREFIX)$(MEN_LIB_DIR)/mbuf$(LIB_SUFFIX)     \
         $(LIB_PREFIX)$(MEN_LIB_DIR)/oss$(LIB_SUFFIX)      \
         $(LIB_PREFIX)$(MEN_LIB_DIR)/id$(LIB_SUFFIX)      \
         $(LIB_PREFIX)$(MEN_LIB_DIR)/dbg$(LIB_SUFFIX)     \


MAK_INCL=$(MEN_INC_DIR)/m99_drv.h     \
         $(MEN_INC_DIR)/men_typs.h    \
         $(MEN_INC_DIR)/oss.h         \
         $(MEN_INC_DIR)/mdis_err.h    \
         $(MEN_INC_DIR)/mbuf.h        \
         $(MEN_INC_DIR)/maccess.h     \
         $(MEN_INC_DIR)/desc.h        \
         $(MEN_INC_DIR)/mdis_api.h    \
         $(MEN_INC_DIR)/mdis_com.h    \
         $(MEN_INC_DIR)/modcom.h      \
         $(MEN_INC_DIR)/ll_defs.h     \
         $(MEN_INC_DIR)/ll_entry.h    \
         $(MEN_INC_DIR)/dbg.h    \


MAK_INP1=m99_drv$(INP_SUFFIX)
MAK_INP2=

MAK_INP=$(MAK_INP1) \
        $(MAK_INP2)

