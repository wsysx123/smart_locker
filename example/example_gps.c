#ifdef __EXAMPLE_GPS__
/***************************************************************************************************
*   Example:
*       
*           GPS Routine
*
*   Description:
*
*           This example gives an example for GPS operation.it use to get the navigation information.
*           Through Uart port, input the special command, there will be given the response about module operation.
*
*   Usage:
*
*           Precondition:
*
*                   Use "make clean/new" to compile, and download bin image to module.
*           
*           Through Uart port:
*
*               Input "GPSOpen"  to retrieve the current local date and time.
*               Input "GPSClose"  to set the current local date and time.
*               Input "GPSRead=<item>" to get navigation information which you want.
*               Input "GPSCMD=<cmdType>,<cmdString>" to send command to GPS module.
*				Input "GPSEPO=<status>" to enable/disable EPO download.
*				Input "GPSAid" to inject EPO data into GPS module.
*       
****************************************************************************************************/
#include "ril.h"
#include "ril_util.h"
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_system.h"
#include "ql_uart.h"
#include "ql_stdlib.h"
#include "ql_error.h"
#include "ril_gps.h"
#include "ql_gnss.h"

#define __GNSS_NEW_API__            // receive NMEA by callback funciton

#define DEBUG_ENABLE 1
#if DEBUG_ENABLE > 0
#define DEBUG_PORT  UART_PORT1
#define DBG_BUF_LEN   512
static char DBG_BUFFER[DBG_BUF_LEN];
#define APP_DEBUG(FORMAT,...) {\
    Ql_memset(DBG_BUFFER, 0, DBG_BUF_LEN);\
    Ql_sprintf(DBG_BUFFER,FORMAT,##__VA_ARGS__); \
    if (UART_PORT2 == (DEBUG_PORT)) \
    {\
        Ql_Debug_Trace(DBG_BUFFER);\
    } else {\
        Ql_UART_Write((Enum_SerialPort)(DEBUG_PORT), (u8*)(DBG_BUFFER), Ql_strlen((const char *)(DBG_BUFFER)));\
    }\
}
#else
#define APP_DEBUG(FORMAT,...) 
#endif

#define SERIAL_RX_BUFFER_LEN  2048
static u8 m_RxBuf_Uart[SERIAL_RX_BUFFER_LEN];
static void Callback_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* param);
static void Callback_GPS_CMD_Hdlr(char *str_URC);

//extern
extern s32 Analyse_Command(u8* src_str,s32 symbol_num,u8 symbol, u8* dest_buf);

/************************************************************************/
/* The entrance for this example application                            */
/************************************************************************/
void proc_main_task(s32 taskId)
{
    s32 ret;
    ST_MSG msg;

    // Register & open UART port
    ret = Ql_UART_Register(UART_PORT1, Callback_UART_Hdlr, NULL);
    if (ret < QL_RET_OK)
    {
        Ql_Debug_Trace("Fail to register serial port[%d], ret=%d\r\n", UART_PORT1, ret);
    }
    ret = Ql_UART_Open(UART_PORT1, 115200, FC_NONE);

    if (ret < QL_RET_OK)
    {
        Ql_Debug_Trace("Fail to open serial port[%d], ret=%d\r\n", UART_PORT1, ret);
    }
    
    APP_DEBUG("\r\n<-- OpenCPU: GPS Example -->\r\n");

    // Start message loop of this task
    while (TRUE)
    {
        Ql_OS_GetMessage(&msg);

        switch(msg.message)
        {
            case MSG_ID_RIL_READY:
                Ql_RIL_Initialize();
                APP_DEBUG("<-- RIL is ready -->\r\n");
            break;

            case MSG_ID_USER_START:
                break;

            default:
                break;
        }
    }
}


static s32 ReadSerialPort(Enum_SerialPort port, /*[out]*/u8* pBuffer, /*[in]*/u32 bufLen)
{
    s32 rdLen = 0;
    s32 rdTotalLen = 0;
    if (NULL == pBuffer || 0 == bufLen)
    {
        return -1;
    }
    Ql_memset(pBuffer, 0x0, bufLen);
    while (1)
    {
        rdLen = Ql_UART_Read(port, pBuffer + rdTotalLen, bufLen - rdTotalLen);
        if (rdLen <= 0)  // All data is read out, or Serial Port Error!
        {
            break;
        }
        rdTotalLen += rdLen;
        // Continue to read...
    }
    if (rdLen < 0) // Serial Port Error!
    {
        APP_DEBUG("<--Fail to read from port[%d]-->\r\n", port);
        return -99;
    }
    return rdTotalLen;
}

void Callback_GPS_CMD_Hdlr(char *str_URC)
{
	if(str_URC != NULL)
	{
		APP_DEBUG("%s", str_URC);
	}
}

#ifdef __GNSS_NEW_API__
/*****************************************************************
* Function:     isXdigit 
* 
* Description:
*               This function is used to check whether the 
*               input character number is a uppercase hexadecimal digital.
*
* Parameters:
*               [in]ch:   
*                       A character num.
*
* Return:        
*               TRUE  - The input character is a uppercase hexadecimal digital.
*               FALSE - The input character is not a uppercase hexadecimal digital.
*
*****************************************************************/
bool isXdigit(char ch)
{
    if((ch>='0' && ch<='9'
) || (ch >= 'A' && ch<='F'))
    {
        return TRUE;
    }

    return FALSE;
}

/*****************************************************************
* Function:     Ql_check_nmea_valid 
* 
* Description:
*               This function is used to check whether the 
*               input nmea is a valid nmea which contains '$' and <CR>.
*
* Parameters:
*               [in]nmea_in:   
*                       string of a sigle complete NMEA sentence
*
* Return:        
*               TRUE  - nmea_in is a valid NMEA sentence.
*               FALSE - nmea_in is an invalid NMEA sentence.
*
*****************************************************************/
bool Ql_check_nmea_valid(char *nmea_in)
{
    char *p=NULL, *q=NULL;
    
    u8 len = Ql_strlen(nmea_in);
    
    if(len > MAX_NMEA_LEN)                              // validate NMEA length
    {
        return FALSE;
    }
    
    p = nmea_in;
    if(*p == '$')                                       // validate header
    {
        q=Ql_strstr(p, "\r");                           // validate tail
        if(q)
        {
            if(isXdigit(*(q-1)) && isXdigit(*(q-2)))    // validate checksum is a hex digit
            {
                u8 cs_cal = 0;
                char cs_str[3]={0};
                while(*++p!='*')
                {
                    cs_cal ^= *p;
                }
                Ql_sprintf(cs_str, "%X", cs_cal);        // checksum is always uppercase.
                if(!Ql_strncmp(cs_str, q-2, 2))          // validate checksum(2 bits)
                {
                    return TRUE;
                }
                else
                {
                    return FALSE;
                }
            }
            else
            {
                return FALSE;
            }
        }
        else
        {
            return FALSE;
        }
    }
    else
    {
        return FALSE;
    }
    
    return FALSE;
}

/*****************************************************************
* Function:     Ql_get_nmea_segment_by_index 
* 
* Description:
*               This function is used to get the specified segment from a string
*               of a sigle complete NMEA sentence which contains '$' and <CR>.
*
* Parameters:
*               [in]nmea_in:   
*                       String of a sigle complete NMEA sentence
*               [in]index:     
*                       The index of the segment to get.
*               [out]segment: 
*                       The buffer to save the specified segment.
*               [in]max_len: 
*                       The length of the segment buffer.
*
* Return:        
*               >0 - Get segment successfully and it's the length of the segment.
*               =0 - Segment is empty.
*               <0 - Error number.
*
*****************************************************************/
char Ql_get_nmea_segment_by_index(char *nmea_in, u8 index, char *segment, u8 max_len)
{
    char *p=NULL, *q=NULL;
    u8 i=0;
    
    p = nmea_in;
    q = NULL;
    
    for(i=0; i<index; i++)
    {
        q=Ql_strstr(p, ",");
        if(q)
        {
            p = q+1;
        }
        else
        {
            break;
        }
    }
    
    q = NULL;
    if(i<index)
    {
        return -1;                  // Segment is not exist;
    }
    else
    {
        if((q=Ql_strstr(p, ",")) || (q=Ql_strstr(p, "*")))
        {
            u8 dest_len = q-p;
            if((*p==',') && (dest_len==1))
            {
                return 0;           // Segment is empty which is between ',' and ',' or ',' and '*'.
            }            
            else if(dest_len<max_len)
            {
                Ql_memset(segment, 0, max_len);
                Ql_memcpy(segment, p, dest_len);
                return dest_len;
            }
            else
            {
                return -2;          // Segment is longer than max_len!
            }
        }
        else
        {
            return -1;             // Segment is not exist;
        }
    }
    return -100;                   // Unknow error!
}


void Customer_NMEA_Hdlr(u8 *multi_nmea, u16 len, void *customizePara)
{
    u8 *p=NULL, *q=NULL;
    char one_nmea[MAX_NMEA_LEN+1];
    char dest_segment[32];

    //RMC
    p=NULL; q=NULL;
    p = Ql_strstr(multi_nmea, "$GNRMC");
    if(p)
    {
        q = Ql_strstr(p, "\r");
        if(q)
        {
            Ql_memset(one_nmea, 0, sizeof(one_nmea));
            Ql_memcpy(one_nmea, p, q-p+1);
            if(Ql_check_nmea_valid(one_nmea))
            {
                //fix status - index 2
                char ret=-1;
                ret = Ql_get_nmea_segment_by_index(one_nmea, 2, dest_segment, sizeof(dest_segment));
                if(ret > 0)
                {
                    if(!Ql_strcmp(dest_segment, "A"))
                    {
                        APP_DEBUG("Fixed, Fix status: %s\r\n", dest_segment);
                    }
                    else
                    {
                        APP_DEBUG("Unfixed, Fix status: %s\r\n", dest_segment);
                    }
                }
                
                //Latitude - index 3
                ret = -1;
                ret = Ql_get_nmea_segment_by_index(one_nmea, 3, dest_segment, sizeof(dest_segment));
                if(ret > 0)
                {
                    double dLat=0.0;
                    dLat = Ql_atof(dest_segment);
                    APP_DEBUG("Latitude: %0.4f\r\n", dLat);
                }

                //Longitude -index 5
                ret=-1;
                ret = Ql_get_nmea_segment_by_index(one_nmea, 5, dest_segment, sizeof(dest_segment));
                if(ret > 0)
                {
                    double dLon=0.0;
                    dLon = Ql_atof(dest_segment);
                    APP_DEBUG("Longitude: %0.4f\r\n", dLon);
                }
            }
        }
    }

    //GGA
    p=NULL; q=NULL;
    p = Ql_strstr(multi_nmea, "$GNGGA");
    if(p)
    {
        q = Ql_strstr(p, "\r");
        if(q)
        {
            Ql_memset(one_nmea, 0, sizeof(one_nmea));
            Ql_memcpy(one_nmea, p, q-p+1);
            if(Ql_check_nmea_valid(one_nmea))
            {
                //Altitude - index 9
                char ret = -1;
                ret = Ql_get_nmea_segment_by_index(one_nmea, 9, dest_segment, sizeof(dest_segment));
                if(ret > 0)
                {
                    double dAut=0.0;
                    dAut = Ql_atof(dest_segment);
                    APP_DEBUG("Altitude: %0.1f\r\n", dAut);
                }
            }
        }
    }
}
#endif

static void Callback_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* param)
{
    s32 iRet = 0;
    switch (msg)
    {
        case EVENT_UART_READY_TO_READ:
        {
            char* p = NULL;
            s32 totalBytes = ReadSerialPort(port, m_RxBuf_Uart, sizeof(m_RxBuf_Uart));
            if (totalBytes <= 0)
            {
                break;
            }

            //APP_DEBUG("command is : %s\r\n",m_RxBuf_Uart);

            /* Power On GPS */
			p = Ql_strstr(m_RxBuf_Uart, "GPSOpen");
            if(p)
            {
                #ifdef __GNSS_NEW_API__
                iRet = Ql_GNSS_PowerOn(RMC_EN | GGA_EN, Customer_NMEA_Hdlr, NULL);  // Also can use ALL_NMEA_EN to enable receiving all NMEA sentences.
                #else
                iRet = RIL_GPS_Open(1);
                #endif
                if(RIL_AT_SUCCESS != iRet) 
                {
                    APP_DEBUG("Power on GPS fail, iRet = %d.\r\n", iRet);
                    break;
                }

                APP_DEBUG("Power on GPS Successful.\r\n");
                break;
            }

            /* Power Off GPS */
            p = Ql_strstr(m_RxBuf_Uart, "GPSClose");
            if(p)
            {
                #ifdef __GNSS_NEW_API__
                iRet = Ql_GNSS_PowerDown();
                #else
                iRet = RIL_GPS_Open(0);
                #endif
                if(RIL_AT_SUCCESS != iRet) 
                {
                    APP_DEBUG("Power off GPS fail, iRet = %d.\r\n", iRet);
                    break;
                }

                APP_DEBUG("Power off GPS Successful.\r\n");
                break;
            }

            /* GPSRead=<item> */
            p = Ql_strstr(m_RxBuf_Uart, "GPSRead=<");
            if(p)
            {
                u8 rdBuff[1000];
                u8 item[10] = {0};

                Ql_memset(rdBuff,0,sizeof(rdBuff));
                Ql_strncpy(item,m_RxBuf_Uart+9,3);

                iRet = RIL_GPS_Read(item,rdBuff);
                if(RIL_AT_SUCCESS != iRet)
                {
                    APP_DEBUG("Read %s information failed.\r\n",item);
                    break;
                }
                APP_DEBUG("%s\r\n",rdBuff);
                break;
            }

			/* GPSEPO=<status>*/
            p = Ql_strstr(m_RxBuf_Uart, "GPSEPO");
            if(p)
            {
				s8 status = 0;
				u8 status_buf[8] = {0};
				if (Analyse_Command(m_RxBuf_Uart, 1, '>', status_buf))
				{
					APP_DEBUG("<--Parameter I Error.-->\r\n");
					break;
				}
				status = Ql_atoi(status_buf);
				
				iRet = RIL_GPS_EPO_Enable(status);
				
                if(RIL_AT_SUCCESS != iRet) 
                {
                    APP_DEBUG("Set EPO status to %d fail, iRet = %d.\r\n", status, iRet);
                    break;
                }

                APP_DEBUG("Set EPO status to %d successful, iRet = %d.\r\n", status, iRet);
                break;
            }

			/* GPSAid */
            p = Ql_strstr(m_RxBuf_Uart, "GPSAid");
            if(p)
            {	
				iRet = RIL_GPS_EPO_Aid();
				
                if(RIL_AT_SUCCESS != iRet) 
                {
                    APP_DEBUG("EPO aiding fail, iRet = %d.\r\n", iRet);
                    break;
                }

                APP_DEBUG("EPO aiding successful, iRet = %d.\r\n", iRet);
                break;
            }
			
			/* GPSCMD=<cmdType>,<cmdString> */
            p = Ql_strstr(m_RxBuf_Uart, "GPSCMD=<");
            if(p)
            {
                s8 type;
				u8 cmd_buf[128] = {0};
				u8 type_buf[8] = {0};
				if (Analyse_Command(m_RxBuf_Uart, 1, '>', type_buf))
				{
					APP_DEBUG("<--Parameter I Error.-->\r\n");
					break;
				}
				type = Ql_atoi(type_buf);
                Ql_memset(cmd_buf,0,sizeof(cmd_buf));
				if (Analyse_Command(m_RxBuf_Uart, 2, '>', cmd_buf))
                {
                    APP_DEBUG("<--Parameter II Error.-->\r\n");
                    break;
                }

                iRet = RIL_GPS_CMD_Send(type, cmd_buf, Callback_GPS_CMD_Hdlr);
				
                if(RIL_AT_SUCCESS != iRet)
                {
                    APP_DEBUG("Send command \"%s\" failed! iRet=%d.\r\n",cmd_buf, iRet);
                    break;
                }
                break;
            }

            APP_DEBUG("Invalid command...\r\n");
        }break;

        case EVENT_UART_READY_TO_WRITE:
        {
            //...
        }break;

        default:
        break;
    }
}

#endif  //__EXAMPLE_ALARM__

