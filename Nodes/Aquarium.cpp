#include <ZsutDhcp.h>
#include <ZsutEthernet.h>
#include <ZsutEthernetUdp.h>
#include <ZsutFeatures.h>
#include <time.h>

#define UDP_SERVER_PORT         13588
#define PACKET_BUFFER_LENGTH        200
#define AQUARIUM     ZSUT_PIN_D3

// Node nr 5 - Aquarium
// PINY: Z4 and D3

byte MAC[] = {0x00, 0xAE, 0xBB, 0xCA, 0xDE, 0x01};
unsigned char packetBuffer[PACKET_BUFFER_LENGTH];
unsigned char packetToSend[PACKET_BUFFER_LENGTH];
unsigned char registerString[PACKET_BUFFER_LENGTH];
unsigned int localPort = UDP_SERVER_PORT;
ZsutIPAddress server_ip = ZsutIPAddress(192, 168, 126, 3);
uint16_t D3value, Z4value = 0;
ZsutEthernetUDP Udp;
int globalID = 1;
char *noAction;

struct Device {
	char device_id[50];
	struct tm *timestamp;
    char location[50];
    char object[50];
    const char *quantity;
    const char *action[2];
    const char *state[2];
};
struct Device deviceInfo;

void setup() {
	strcpy(deviceInfo.device_id, "id_Aquarium_1");
	ZsutPinMode(AQUARIUM, OUTPUT);
	
	time_t rawtime;
    time( &rawtime );
    deviceInfo.timestamp = localtime( &rawtime );
	strcpy(deviceInfo.location, "lRoom");
	strcpy(deviceInfo.object, "oAquarium");
	deviceInfo.quantity = "qWaterTemperature";
	deviceInfo.action[0] = "aHeatUP";
	deviceInfo.action[1] = "aHeatSTOP";
	deviceInfo.state[0] = "sHeatInProgressON";
	deviceInfo.state[1] = "sHeatInProgressOFF";
	
	
	Serial.begin(115200);
	Serial.print(F("Zsut eth udp server init..."));
	ZsutEthernet.begin(MAC);

	Serial.print(F("My IP address: "));
	for (byte thisByte = 0; thisByte < 4; thisByte++) {
		Serial.print(ZsutEthernet.localIP()[thisByte], DEC); Serial.print(F("."));
	}
	Serial.println();
	Udp.begin(localPort);
	
	snprintf(registerString, 200, "ENROLLING_NEW_DEVICE;\
%s;\
%s;\
%s;\
%s;\
%s;\
%s,%s;\
%s,%s;\
END;", deviceInfo.device_id,asctime(deviceInfo.timestamp),deviceInfo.location,deviceInfo.object,deviceInfo.quantity,deviceInfo.action[0],deviceInfo.action[1],deviceInfo.state[0],deviceInfo.state[1]);

	Udp.beginPacket(server_ip, localPort);
	Udp.write(registerString, strlen(registerString));
	Udp.endPacket();

	Serial.print(F("Wiadomosc rejestrujaca zostala wyslana.\n"));
	Z4value = ZsutAnalog4Read();
}

void loop() {
	memset(packetBuffer, 0, PACKET_BUFFER_LENGTH);
	time_t rawtime;
    time( &rawtime );
	
	uint16_t new_value;
	char *state_to_send;
	
	new_value = ZsutAnalog4Read();
	if(Z4value != new_value){
		Z4value = new_value;
		char value_to_send[4];
		char id_to_send[4];
		sprintf(value_to_send, "%d", Z4value);
		sprintf(id_to_send, "%d", globalID);
		
		D3value = ZsutDigitalRead();
		D3value = (D3value >> 3) & 0x01;
		if(D3value == 1){
			state_to_send = deviceInfo.state[0];
			noAction = deviceInfo.action[0];
		}else{
			state_to_send = deviceInfo.state[1];
			noAction = deviceInfo.action[1];
		}
	
		snprintf(packetToSend, PACKET_BUFFER_LENGTH, "PROVIDING_DATA;\
%s;\
%s;\
%s,%s;\
%s;\
END;", id_to_send, asctime(localtime( &rawtime )), deviceInfo.quantity,value_to_send, state_to_send);

		Udp.beginPacket(server_ip, localPort);
		Udp.write(packetToSend, PACKET_BUFFER_LENGTH);
		Udp.endPacket();
		globalID++;
		Serial.print(F("Zaraportowano temperature wody w akwarium = "));
		Serial.print(value_to_send);
		Serial.println();
	}
	
	int packetSize = Udp.parsePacket();
	if (packetSize > 0) {
		Serial.print(F("Przyszlo polecenie z serwera!\n"));
		int len = Udp.read(packetBuffer, PACKET_BUFFER_LENGTH), j = 0;

		packetBuffer[len] = '\0';

		char *tokens[5];
		char * token = strtok(packetBuffer, ";");
		while (token != NULL){
			tokens[j++] = token;
			token = strtok(NULL, ";");
		}
		if(strncmp(tokens[3], noAction, strlen(tokens[3])) != 0){
			if(strncmp(tokens[3], deviceInfo.action[0], strlen(tokens[3])) == 0){
				ZsutDigitalWrite(AQUARIUM, HIGH);
				Serial.print(F("Uruchomiono grzanie wody w akwarium!\n"));
			}else{
				ZsutDigitalWrite(AQUARIUM, LOW);
				Serial.print(F("Wylaczono grzanie wody w akwarium!\n"));
			}
		}
	} else {
		Udp.flush();
	}
	
}