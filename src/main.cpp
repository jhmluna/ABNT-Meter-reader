#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <Credentials.h>

#define _debug			// Set debug mode for print messages in development.
//#define _printRx		// Sets mode for printing received bytes. Used to increase the RX buffer.

// Códigos ABNT NBR 14522:2008 para sincronizar comunicação
#define ENQ 0x05
#define ACK 0x06
#define NAK 0x15

#define SW_SERIAL_UNUSED_PIN -1

// Function prototypes
void setup_wifi(void);
void disablePullUp(void);
void callback(char* topic, byte* payload, unsigned int length);
void reconnect(void);
void sendCommand_23(void);
void sendAck(void);
void recvBytes(void);
void showNewData(void);
void publish_data(unsigned long meterId, unsigned long meter_data[3]);
unsigned long * converte_energia(byte *message_energia);
unsigned long converteId(byte *message_id);
byte bcdToDec(byte val);
unsigned int calcula_crc16(byte *array, int tamanho_buffer);

bool sendData = false;	// Flag para informar que foi enviado comando
bool newData = false;

// Set up serial
const int baudRate = 9600;
const int blockSize = 258;				// Response Block Size
byte receivedBytes[blockSize] = {};

// Software Serial for debug Conisdering ESP-01
SoftwareSerial swSer(SW_SERIAL_UNUSED_PIN, 0);

// Parâmetros do WiFi
const char *ssid = WIFI_SSID;         // your network SSID (name)
const char *password = WIFI_PWD;    // your network key
WiFiClient espClient;

/* Parâmetros do serviço MQTT
Observation: const char *ptr is a pointer to a constant character. You cannot change the value pointed by ptr,
but you can change the pointer itself. "const char *" is a (non-const) pointer to a const char.
*/
const char *mqtt_server = MQQT_SERVER_IP;
const char *clientId = BASE_TOPIC;
char commandTopic[TOPIC_LENGTH + 7];  // Topic used to receive commands from NodeRed
strcpy(commandTopic, BASE_TOPIC);
strcat(commandTopic, "command");
char dataTopic[TOPIC_LENGTH + 4];     // Topic used to send data to NodeRed
strcpy(dataTopic, BASE_TOPIC);
strcat(dataTopic, "data");

// LWT message constants
byte willQos = 1;
const char *willTopic = strcpy(BASE_TOPIC, "/status");
const char *willMessage = "Offline";
bool willRetain = true;

// Callback function header
void callback(char* topic, byte* payload, unsigned int length);

// Sets the server details and message callback function.
PubSubClient client(mqtt_server, 1883, callback, espClient);

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 90000;           // interval at which to read (milliseconds)

// Array with Command 23 - Reading registers of visible channels.
const PROGMEM byte command_23[66] = {
  0x23, 0x12, 0x34, 0x56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x0b
};