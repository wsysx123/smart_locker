#ifndef _MQTT_
#define _MQTT_
#include "MqttKit.h"
#include "Common.h"
#define MQTTSIZE 1024
extern unsigned char dataMem[MQTTSIZE];
extern unsigned char mqttwaitingack;
extern MQTT_PACKET_STRUCTURE mqttbuf;
extern u8  ackmsg[MQTTSIZE];
extern unsigned char lastpublish[MQTTSIZE];
extern unsigned char publish[MQTTSIZE];
extern u8 lastpublishlen;
extern u8 publishlen;
extern unsigned char packetdata[MQTTSIZE];
extern u8 packetlen;
extern u32 acklen;
extern char IMEIstr[20];
extern char topics[1][50];
#define Productkey "a1vUeRR8Av7"
#define Devicename IMEIstr
#define mqttpassword "ff55df2102bc5341c1994f6dffb1cd6fec897a2e"

#define mqttserver Productkey ".iot-as-mqtt.cn-shanghai.aliyuncs.com"
#define mqttport 1883

#define mqttclient Devicename
#define topichead "/"Productkey
extern char mqttusername[30];

//#define topic "s2c/bike/"##Devicename



typedef enum{
    STATE_MQTT_TCP_NOCONNECT,
	STATE_MQTT_TCP_CONNECT,	
	STATE_MQTT_MQTT_CONNECT_WAITCONNECTACK,
	STATE_MQTT_MQTT_CONNECT_OK,	
	STATE_MQTT_MQTT_SUBSCRIBE_WAITCONNECTACK,
	STATE_MQTT_MQTT_SUBSCRIBE_OK,
	STATE_MQTT_MQTT_PUBLISH_PACKET,
	STATE_MQTT_MQTT_PUBLISH,
	STATE_MQTT_MQTT_PUBLISH_WAITACK,
	STATE_MQTT_MQTT_PUBLISH_OK,
	STATE_MQTT_MQTT_WAITING
}Enum_MQTTSTATE;
extern unsigned char MQTTSTATE;


static void Callback_MQTT_Timer(u32 timerId, void* param);
void buffer_init(MQTT_PACKET_STRUCTURE * mqttPacket);
void MQTT_Ping();
void MQTT_Subscribe(const char *topics[], unsigned char topic_cnt);
void MQTT_Publish(const char *topic, const char *msg,u32 len);
void MQTT_RevPro(unsigned char *cmd);
void GetBCDIMEI(void);
void packet(u32,char*);
void extinit(void);
void getusername();

void send2server(char * data,u32 len);
void gettopicid();

#endif