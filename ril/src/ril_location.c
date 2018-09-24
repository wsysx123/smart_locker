/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Quectel Co., Ltd. 2013
*
*****************************************************************************/
/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ril_location.c 
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   The module defines the information, and APIs related to RIL.
 *
 * Author:
 * -------
 * -------
 *
 *============================================================================
 *             HISTORY
 *----------------------------------------------------------------------------
 * 
 ****************************************************************************/
#include "ril_location.h"
#include "ril.h"
#include "ril_util.h"
#include "ql_common.h"
#include "ql_uart.h"
#include "ql_stdlib.h"
#include "ql_error.h"


#define RIL_LOC_DEBUG_ENABLE 0
#if RIL_LOC_DEBUG_ENABLE > 0
#define RIL_LOC_DEBUG_PORT  UART_PORT2
static char DBG_Buffer[100];
#define RIL_LOC_DEBUG(BUF,...)  QL_TRACE_LOG(RIL_LOC_DEBUG_PORT,BUF,100,__VA_ARGS__)
#else
#define RIL_LOC_DEBUG(BUF,...) 
#endif

static CB_LocInfo callback_loc = NULL;

s32 RIL_GetLocation(CB_LocInfo cb_loc)
{
	s32 ret = RIL_AT_FAILED;
	char strAT[200]; 
    callback_loc = cb_loc;

	Ql_memset(strAT, 0, sizeof(strAT));
	Ql_sprintf(strAT, "AT+QLOCCFG=\"ASYNCH\",1\r\n");
	ret = Ql_RIL_SendATCmd(strAT,Ql_strlen(strAT),NULL,NULL,0);
	if (RIL_AT_SUCCESS == ret)
	{
        Ql_memset(strAT, 0, sizeof(strAT));
        Ql_sprintf(strAT, "AT+QCELLLOC=1\n");                
        RIL_LOC_DEBUG(DBG_Buffer,"<-- Send AT:%s, ret = %d -->\r\n",strAT, ret);        
        ret = Ql_RIL_SendATCmd(strAT,Ql_strlen(strAT),NULL,NULL,0);
	}
    return ret;
}

s32 RIL_GetLocationByCell(ST_CellInfo* cell, CB_LocInfo cb_loc)
{
	s32 ret = RIL_AT_SUCCESS;
	char strAT[200];
    callback_loc = cb_loc;        

	Ql_memset(strAT, 0, sizeof(strAT));
	Ql_sprintf(strAT, "AT+QLOCCFG=\"ASYNCH\",1\r\n");
	Ql_RIL_SendATCmd(strAT,Ql_strlen(strAT),NULL,NULL,0);
	if (RIL_AT_SUCCESS == ret)
	{
        Ql_memset(strAT, 0, sizeof(strAT));
        Ql_sprintf(strAT, "AT+QCELLLOC=3,%d,%d,%d,%d,%d,%d\n",cell->cellId,cell->lac,cell->mnc,cell->mcc,cell->rssi,cell->timeAd);
        ret = Ql_RIL_SendATCmd(strAT,Ql_strlen(strAT),NULL,NULL,0);
    }
    return ret;
}

void OnURCHandler_QCELLLocation(const char* strURC,void* reserved)
{
	char buff[30];
	ST_LocInfo loclnfo;
 	if (NULL != callback_loc)
 	{
		Ql_strcpy(buff,"\r\n+QCELLLOC:\0");
		if(Ql_StrPrefixMatch(strURC,buff))
		{
			Ql_sscanf(strURC,"%*[^:]: %[^,]",buff);
			loclnfo.longitude = Ql_atof(buff);
			
			Ql_sscanf(strURC,"%*[^,],%[^\r\n]",buff);
			loclnfo.latitude = Ql_atof(buff);

			callback_loc(0,&loclnfo);
		}
	}
}

