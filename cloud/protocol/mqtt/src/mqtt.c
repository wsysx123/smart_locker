#include "MqttKit.h"
#include "Common.h"
#include "mqtt.h"
#include "ql_uart.h"
#include "ql_socket.h"

#include "ql_uart.h"
#include "ql_gpio.h"
#include "ql_time.h"
#include "ql_eint.h"


#define DEBUG_ENABLE 1
#if DEBUG_ENABLE > 0
#define DEBUG_PORT  UART_PORT1
#define DBG_BUF_LEN   1024
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

extern s32 m_remain_len;
extern u8 m_send_buf;
extern char *m_pCurrentPos;
#define send2server(data,len) {\
   	m_pCurrentPos = m_send_buf;\
	Ql_memcpy(m_pCurrentPos + m_remain_len,data,len);\
	m_remain_len +=len;\
}



unsigned char dataMem[MQTTSIZE];
MQTT_PACKET_STRUCTURE mqttbuf;
u8  ackmsg[MQTTSIZE];

u32 acklen = 0;
unsigned char mqttwaitingack = 0;

unsigned char lastpublish[MQTTSIZE];
u8 lastpublishlen;
unsigned char publish[MQTTSIZE];
u8 publishlen;
unsigned char packetdata[MQTTSIZE];
u8 packetlen;

unsigned char MQTTSTATE = STATE_MQTT_TCP_NOCONNECT;
char topics[1][50]={topichead};
//const char *topics[] = {"testtopic"};
u8 second=0;
u8 minute = 0;
u8 hour = 0;
u8 rand = 0;
char IMEIstr[20];
char IMEIBCD[10];

char mqttusername[30];



void MQTT_Subscribe(const char *topics[], unsigned char topic_cnt)
{
	
	unsigned char i = 0;
	
	APP_DEBUG(topics[0]);
	for(; i < topic_cnt; i++)
		APP_DEBUG("Subscribe Topic: %s\r\n", topics[i]);
	if(MQTT_PacketSubscribe(MQTT_SUBSCRIBE_ID, MQTT_QOS_LEVEL0, topics, topic_cnt, &mqttbuf) == 0)
	{
		send2server(mqttbuf._data,mqttbuf._len);
		APP_DEBUG("\r\n");
		Ql_UART_Write(UART_PORT1,mqttbuf._data,mqttbuf._len);
		APP_DEBUG("\r\n");
		MQTT_DeleteBuffer(&mqttbuf);											//É¾°ü
	}
	else
		APP_DEBUG( "WARN:	MQTT_PacketConnect Failed\r\n");

}


void MQTT_Publish(const char *topic, const char *msg,u32 len)
{

	APP_DEBUG( "Publish Topic: %s, Msg: %s\r\n", topic, msg);
	Ql_memcpy(lastpublish,msg,len);
	lastpublishlen = len;
	if(MQTT_PacketPublish(MQTT_PUBLISH_ID, topic, msg,len, MQTT_QOS_LEVEL0, 0, 1, &mqttbuf) == 0)
	{
		send2server(mqttbuf._data,mqttbuf._len);					//ÏòÆ½Ì¨·¢ËÍ¶©ÔÄÇëÇó
		APP_DEBUG("\r\n");
		Ql_UART_Write(UART_PORT1,mqttbuf._data,mqttbuf._len);
		APP_DEBUG("\r\n");
		APP_DEBUG("publish send ok\r\n");
		MQTT_DeleteBuffer(&mqttbuf);											//É¾°ü
	}

}

void MQTT_Ping()
{

	
	APP_DEBUG( "mqtt ping\r\n");
	
	if(MQTT_PacketPing(&mqttbuf)== 0)
	{
		send2server(mqttbuf._data,mqttbuf._len);					//ÏòÆ½Ì¨·¢ËÍ¶©ÔÄÇëÇó
		MQTT_DeleteBuffer(&mqttbuf);											//É¾°ü
	}

}
void GetBCDIMEI(){

	RIL_GetIMEI(IMEIstr);
	IMEIBCD[0] = IMEIstr[2]-'0';
//	APP_DEBUG("0:%x ",IMEIBCD[0]);//不加这一句IMEI码错误

	for(int i=1;i<8;i++){
		IMEIBCD[i] = IMEIstr[(i+1)*2]-'0'+(IMEIstr[(i+1)*2-1]-'0')*16;
		APP_DEBUG("%d:%x ",i,IMEIBCD[i]);//不加这一句IMEI码错误
	}
	Ql_memcpy(IMEIstr,IMEIstr+2,15);
	APP_DEBUG("\r\n");
	for(int i=0;i<15;i++){
		APP_DEBUG("%d:%x ",i,Devicename[i]);
	}
	APP_DEBUG("\r\n");
	getusername();
	gettopicid(topics[0]);
}

void getusername(){
	char strtmp[20];
	//Ql_memcpy(strtmp,IMEIstr+2,16);
	Ql_memset(mqttusername,0,30);
	Ql_memcpy(mqttusername,Devicename,15);
	//Ql_strcpy(mqttusername,strtmp);
	Ql_strcat(mqttusername,"&");
	Ql_strcat(mqttusername,Productkey);

	APP_DEBUG("server:%s\r\n",mqttserver);
	APP_DEBUG("username:%s\r\n",mqttusername);
	APP_DEBUG("password:%s\r\n",mqttpassword);
}
void packet(u32 len,char* tmp){
	char cmd[50];

	ST_Time now;
	Ql_GetLocalTime(&now);
	cmd[0] = 0xA5;
	Ql_memcpy(cmd+1,IMEIBCD,8);
	cmd[9] = now.hour;
	cmd[10] = now.minute;
	cmd[11] = now.second;
	cmd[12] = rand++;
	if(rand==127) rand = 0;
	cmd[13] = 0x03;
	cmd[14] = len/0xffffff%0xff;
	cmd[15] = len/0xffff%0xff;
	cmd[16] = len/0xff%0xff;
	cmd[17] = len%0xff;
	Ql_memcpy(cmd+18,tmp,len);
	cmd[18+len] = 0;
	for(int i=13;i<=17+len;i++){
		cmd[18+len]+=cmd[i];
	}
	cmd[18+len+1] = 0x5A;
	for(int i=0;i<=len+19;i++){
		APP_DEBUG("%x ",cmd[i]);
		}
		APP_DEBUG("\r\npublish:");
	Ql_memcpy(publish,cmd,len+20);
	for(int i=0;i<=len+19;i++){
		APP_DEBUG("%x ",publish[i]);
		}
		APP_DEBUG("\r\n");
		publishlen = len+20;
		MQTTSTATE = STATE_MQTT_MQTT_PUBLISH;
/*	while(1){
		cmd[9] = now.hour;
		cmd[10] = now.minute;
		cmd[11] = now.second;
		for(int i=0;i<12;i++){
		APP_DEBUG("%x ",cmd[i]);
		}
		APP_DEBUG("\r\n");
		Ql_Sleep(1000);
		Ql_GetLocalTime(&now);
		}*/
}
void open(){
	char msg[3];
	msg[0]=0x28;
	msg[1]=0x01;

	if(Ql_GPIO_GetLevel(PINNAME_PCM_SYNC)==1){
			
			while(Ql_GPIO_GetLevel(PINNAME_PCM_SYNC)==1){
					APP_DEBUG("opening\r\n");
				Ql_GPIO_SetLevel(PINNAME_SIM2_CLK,PINLEVEL_HIGH);
				Ql_Sleep(10);
				Ql_GPIO_SetLevel(PINNAME_SIM2_CLK,PINLEVEL_LOW);
				Ql_Sleep(10);
			}
			Ql_GPIO_SetLevel(PINNAME_SIM2_CLK,PINLEVEL_LOW);
		}
	else{
			msg[2]=0x00;
		MQTTSTATE = STATE_MQTT_MQTT_PUBLISH_PACKET;
		packetlen = 3;
		Ql_memcpy(packetdata,msg,3);
	}
	APP_DEBUG("opened\r\n");
}

void close(){
	char msg[3];
	msg[0]=0x28;
	msg[1]=0x01;

	if(Ql_GPIO_GetLevel(PINNAME_PCM_SYNC)==0){
			Ql_GPIO_SetLevel(PINNAME_SIM2_CLK,PINLEVEL_HIGH);
			while(Ql_GPIO_GetLevel(PINNAME_PCM_SYNC)==0){
				Ql_GPIO_SetLevel(PINNAME_SIM2_CLK,PINLEVEL_HIGH);
				Ql_Sleep(10);
				Ql_GPIO_SetLevel(PINNAME_SIM2_CLK,PINLEVEL_LOW);
				Ql_Sleep(10);
			}
			Ql_GPIO_SetLevel(PINNAME_SIM2_CLK,PINLEVEL_LOW);
		}
	else{
			msg[2]=0x01;
		MQTTSTATE = STATE_MQTT_MQTT_PUBLISH_PACKET;
		packetlen = 3;
		Ql_memcpy(packetdata,msg,3);
	}
}

void CALLBACK_Execute(char *cmd){
	long int cmdlen = 0;
	u8 check = 0;
	if(cmd[0]==0xA5)
		if(Ql_memcmp(cmd+1,IMEIBCD,8)==0){
			APP_DEBUG("receive mycmd!!!\r\n");
				for(int i=0;i<=7;i++){
					APP_DEBUG("%x ",IMEIBCD[i]);
					}
			if(cmd[13]==0x01){
				cmdlen =0;
				cmdlen += cmd[14]*0xffffff;
				cmdlen += cmd[15]*0xffff;
				cmdlen += cmd[16]*0xff;
				cmdlen += cmd[17];
				for(int i=0;i<cmdlen+5;i++){
					check+=cmd[i+13];
				}
				if(check==cmd[18+cmdlen]){
					APP_DEBUG("check ok!\r\n");
					switch (cmd[18])
						{
						case 0x26:
							if(cmd[20]==0x02){
								if(cmd[21]==0x02)
									open();
								
							}
							break;
						default : 
							APP_DEBUG("no command!\r\n");
						break;
						}
				}
				}
		}
	APP_DEBUG("receive %s!!!\r\n",cmd);
}

void MQTT_RevPro(unsigned char *cmd)
{
	
	
	char *req_payload = NULL;
	char *cmdid_topic = NULL;
	
	unsigned short topic_len = 0;
	unsigned short req_len = 0;
	
	unsigned char qos = 0;
	static unsigned short pkt_id = 0;
	
	switch(MQTT_UnPacketRecv(cmd))
	{
		case MQTT_PKT_CONNACK:
			switch(MQTT_UnPacketConnectAck(cmd))
					{
						case 0:APP_DEBUG( "Tips:  connect ok!\r\n");
							MQTTSTATE = STATE_MQTT_MQTT_CONNECT_OK;
							break;						
						case 1:APP_DEBUG( "WARN:	Protocol Error \r\n");break;
						case 2:APP_DEBUG( "WARN:	Clientid Error\r\n");break;
						case 3:APP_DEBUG( "WARN:	Server Error\r\n");break;
						case 4:APP_DEBUG( "WARN:	Username or Password Error\r\n");break;
						case 5:APP_DEBUG( "WARN:	Illegal connection\r\n");break;
						
						default:APP_DEBUG( "ERR:	UnKnown error\r\n");break;
					}break;
		
		case MQTT_PKT_PINGRESP:
		
			APP_DEBUG( "Tips:	HeartBeat OK\r\n");
			//heart_beat = 1;
		
			break;
		
		case MQTT_PKT_CMD:															//命令下发
			
			if(MQTT_UnPacketCmd(cmd, &cmdid_topic, &req_payload, &req_len) == 0)	//解包
			{
				APP_DEBUG( "cmdid: %s, req: %s, req_len: %d\r\n", cmdid_topic, req_payload, req_len);
				
				//执行命令回调函数
				CALLBACK_Execute(req_payload);
				
				if(MQTT_PacketCmdResp(cmdid_topic, req_payload, &mqttbuf) == 0)	//命令回复
				{
					APP_DEBUG( "Tips:	Send CmdResp\r\n");
					
					Ql_UART_Write(UART_PORT1,mqttbuf._data,mqttbuf._len);
					send2server(mqttbuf._data,mqttbuf._len);
					MQTT_DeleteBuffer(&mqttbuf);									//É¾°ü
				}
				
				MQTT_FreeBuffer(cmdid_topic);
				MQTT_FreeBuffer(req_payload);
				//onenet_info.send_data = SEND_TYPE_DATA;
			}
		
			break;
			
		case MQTT_PKT_PUBLISH:														//接收publish
		
		if(MQTT_UnPacketPublish(cmd, &cmdid_topic, &topic_len, &req_payload, &req_len, &qos, &pkt_id) == 0)
			{
				APP_DEBUG( "topic: %s, topic_len: %d, payload: %s, payload_len: %d\r\n",
																	cmdid_topic, topic_len, req_payload, req_len);
				
				//命令回调
				CALLBACK_Execute(req_payload);
				
				switch(qos)
				{
					case 1:															//QOS=1 设备回复ACK
					
						if(MQTT_PacketPublishAck(pkt_id, &mqttbuf) == 0)
						{
							APP_DEBUG( "Tips:	Send PublishAck\r\n");
							//NET_DEVICE_SendData(mqttbuf._data, mqttbuf._len);
							Ql_UART_Write(UART_PORT1,mqttbuf._data,mqttbuf._len);
							send2server(mqttbuf._data,mqttbuf._len);
							MQTT_DeleteBuffer(&mqttbuf);
						}
					
					break;
					
					case 2:															//QOS=2 设备回复Rec
																					//平台回复Rel，设备再回复Comp
						if(MQTT_PacketPublishRec(pkt_id, &mqttbuf) == 0)
						{
							APP_DEBUG( "Tips:	Send PublishRec\r\n");
							//NET_DEVICE_SendData(mqttbuf._data, mqttbuf._len);
							Ql_UART_Write(UART_PORT1,mqttbuf._data,mqttbuf._len);
							send2server(mqttbuf._data,mqttbuf._len);
							MQTT_DeleteBuffer(&mqttbuf);
						}
					
					break;
					
					default:
						break;
				}
				
				MQTT_FreeBuffer(cmdid_topic);
				MQTT_FreeBuffer(req_payload);
			}
		
			break;
			
		case MQTT_PKT_PUBACK:														//发送publish平台回复ack
		
			if(MQTT_UnPacketPublishAck(cmd) == 0)
				{
				APP_DEBUG( "Tips:	MQTT Publish Send OK\r\n");
				MQTTSTATE = STATE_MQTT_MQTT_PUBLISH_OK;
			}
			
			break;
			
		case MQTT_PKT_PUBREC:														//收到Rec，设备回复Rel
		
			if(MQTT_UnPacketPublishRec(cmd) == 0)
			{
				APP_DEBUG( "Tips:	Rev PublishRec\r\n");
				if(MQTT_PacketPublishRel(MQTT_PUBLISH_ID, &mqttbuf) == 0)
				{
					APP_DEBUG( "Tips:	Send PublishRel\r\n");
					//NET_DEVICE_SendData(mqttbuf._data, mqttbuf._len);
					Ql_UART_Write(UART_PORT1,mqttbuf._data,mqttbuf._len);
					send2server(mqttbuf._data,mqttbuf._len);
					MQTT_DeleteBuffer(&mqttbuf);
				}
			}
		
			break;
			
		case MQTT_PKT_PUBREL:														//平台回复Rel，设备回复Comp
			
			if(MQTT_UnPacketPublishRel(cmd, pkt_id) == 0)
			{
				APP_DEBUG( "Tips:	Rev PublishRel\r\n");
				if(MQTT_PacketPublishComp(pkt_id, &mqttbuf) == 0)
				{
					APP_DEBUG( "Tips:	Send PublishComp\r\n");
					//NET_DEVICE_SendData(mqttbuf._data, mqttbuf._len);
					Ql_UART_Write(UART_PORT1,mqttbuf._data,mqttbuf._len);
					send2server(mqttbuf._data,mqttbuf._len);
					MQTT_DeleteBuffer(&mqttbuf);
				}
			}
		
			break;
		
		case MQTT_PKT_PUBCOMP:														//接收平台的comp
		
			if(MQTT_UnPacketPublishComp(cmd) == 0)
			{
				APP_DEBUG( "Tips:	Rev PublishComp\r\n");
			}
		
			break;
			
		case MQTT_PKT_SUBACK:														//平台订阅成功
		
			if(MQTT_UnPacketSubscribe(cmd) == 0)
				{
				APP_DEBUG( "Tips:	MQTT Subscribe OK\r\n");
				MQTTSTATE = STATE_MQTT_MQTT_SUBSCRIBE_OK;
				}
			else{
				APP_DEBUG( "Tips:	MQTT Subscribe Err\r\n");
				}
		
			break;
			
		case MQTT_PKT_UNSUBACK:														//取消订阅成功
		
			if(MQTT_UnPacketUnSubscribe(cmd) == 0)
				{
				APP_DEBUG( "Tips:	MQTT UnSubscribe OK\r\n");
				}
			else{
				APP_DEBUG( "Tips:	MQTT UnSubscribe Err\r\n");
				}
		
			break;
		
		default:
			
			break;
	}


}

void ext_callback(Enum_PinName Pinname,Enum_PinLevel pinlevel,void * customparam){
	char msg[3];
	msg[0]=0x28;
	msg[1]=0x01;
	if(PINNAME_CTS==Pinname){
		Ql_Sleep(10);
		if(Ql_EINT_GetLevel(PINNAME_CTS)){
			APP_DEBUG("open!\r\n");
			msg[2]=0x00;
		}
		else{
			APP_DEBUG("locked!\r\n");
			msg[2]=0x01;		
		}
		MQTTSTATE = STATE_MQTT_MQTT_PUBLISH_PACKET;
		packetlen = 3;
		Ql_memcpy(packetdata,msg,3);
		//packet(3,msg);
		//MQTT_Publish(topics[0],msg,3);
	}
}

void extinit(){
	Ql_EINT_Register(PINNAME_CTS,ext_callback,NULL);
	Ql_EINT_Init(PINNAME_CTS,EINT_LEVEL_TRIGGERED,0,5,0);
	APP_DEBUG("extinit start!\r\n");
	//初始状态关锁
/*	if(Ql_GPIO_GetLevel(PINNAME_PCM_SYNC)==0){
			Ql_GPIO_SetLevel(PINNAME_SIM2_CLK,PINLEVEL_HIGH);
			while(Ql_GPIO_GetLevel(PINNAME_PCM_SYNC)==0){
				Ql_GPIO_SetLevel(PINNAME_SIM2_CLK,PINLEVEL_HIGH);
				Ql_Sleep(10);
				Ql_GPIO_SetLevel(PINNAME_SIM2_CLK,PINLEVEL_LOW);
				Ql_Sleep(10);
			}
			Ql_GPIO_SetLevel(PINNAME_SIM2_CLK,PINLEVEL_LOW);
		}*/
	APP_DEBUG("extinit ok!\r\n");
}

void gettopicid(char * topic){
	Ql_strcat(topic,"/");
	Ql_memcpy(topic+Ql_strlen(topic),Devicename,15);
	Ql_strcat(topic,"/s2c/bike/");
}

