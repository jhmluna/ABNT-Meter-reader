#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include "Credentials.h"
#include "ABNT.h"
#include "Comm.h"

#define _debug			// Set debug mode for development.
//#define _printRx		// Sets mode for printing received bytes. Used to increase the RX buffer.

// Function prototypes
void setup_wifi(void);
void disablePullUp(void);
void callback(char* topic, byte* payload, unsigned int length);
void reconnect(void);
void sendAck(void);
void recvBytes(void);
void showNewData(void);
void publish_data(unsigned long meterId, unsigned long meter_data[3]);
unsigned long * converte_energia(byte *message_energia);
unsigned long converteId(byte *message_id);
byte bcdToDec(byte val);
unsigned int calcula_crc16(byte *array, int tamanho_buffer);

// Software Serial for debug Conisdering ESP-01
SoftwareSerial swSer(SW_SERIAL_UNUSED_PIN, 0);

char dataTopic[TOPIC_LENGTH + 4];     // Topic used to send data to NodeRed
char commandTopic[TOPIC_LENGTH + 7];     // Topic used to send data to NodeRed

const long interval = 90000;   // Interval at which to pooling the meter (milliseconds)

/*
	Generally, you should use "unsigned long" for variables that hold time
	The value will quickly become too large for an int to store
*/
unsigned long previousMillis = 0;	// Will store last time LED was updated


void setup() {
	Serial.setRxBufferSize(blockSize);
	Serial.begin(baudRate);		// Comm Serial port. Must be changed to communication port after the end of development.

	#ifdef _debug				// Debug serial port. Must be disabled after the end of development.
		swSer.begin(baudRate);
		swSer.print(F("Início do Setup."));
	#endif

	// Desabilita pull up do pino referente ao RX GPIO3 - eagle_soc.h
	disablePullUp();

	// Configura GPIO2 como saída para ser usado como chave que conecta/desconecta o pino GPIO1/U0TXD à saída.
	// GPIO1 tem efeito na inicialização, se mantido em LOW; ESP não será inicializado neste caso.
	pinMode(2, OUTPUT);
	digitalWrite(2, LOW);  // Sets GPIO2 LOW by connecting the transistor acting as a switch and connecting GPIO1 / U0TXD to the output.

	// We start by connecting to a WiFi network
	setup_wifi();

	// --- Functions related to Over The Air update ---
	ArduinoOTA.onStart([]() {
		String type;
		if (ArduinoOTA.getCommand() == U_FLASH) {
			type = "sketch";
		} else { // U_FS
			type = "filesystem";
		}

		// NOTE: if updating FS this would be the place to unmount FS using FS.end()
		swSer.println("Start updating " + type);
	});

	ArduinoOTA.onEnd([]() {
		swSer.println("\nEnd");
	});

	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		swSer.printf("Progress: %u%%\r", (progress / (total / 100)));
	});

	ArduinoOTA.onError([](ota_error_t error) {
		swSer.printf("Error[%u]: ", error);
		if (error == OTA_AUTH_ERROR) {
			swSer.println("Auth Failed");
		} else if (error == OTA_BEGIN_ERROR) {
			swSer.println("Begin Failed");
		} else if (error == OTA_CONNECT_ERROR) {
			swSer.println("Connect Failed");
		} else if (error == OTA_RECEIVE_ERROR) {
			swSer.println("Receive Failed");
		} else if (error == OTA_END_ERROR) {
			swSer.println("End Failed");
		}
	});

	ArduinoOTA.begin();

	#ifdef _debug
		swSer.print(F("Fim do Setup."));
	#endif

  strcpy(dataTopic, BASE_TOPIC);
  strcat(dataTopic, "/data");

  strcpy(commandTopic, BASE_TOPIC);
  strcat(commandTopic, "/command");

  swSer.println("");
  swSer.print("Tamanho do tópico: ");
  swSer.println(TOPIC_LENGTH);
  swSer.println(BASE_TOPIC);
  swSer.println(dataTopic);
  swSer.println(commandTopic);
	swSer.println(clientId);
}

void loop() {
	ArduinoOTA.handle();
	
	// Check to see if it's time to read the meter data; that is, if the difference between the current
	// time and last time you read the meter is bigger than the interval at which you want to read it.
	unsigned long currentMillis = millis();

	if (currentMillis - previousMillis >= interval) {
		swSer.println(Abnt::ACK);
		swSer.println(Abnt::ENQ);
		swSer.println(Abnt::NAK);

		// Save the last time you read the meter.
		previousMillis = currentMillis;
	}
}

// Desabilita pull up do pino referente ao GPIO3 que é o RX do ESP8266 para receber dados da porta ótica.
void disablePullUp() {
	// Desabilita pull up do pino referente ao RX GPIO3 - eagle_soc.h
	// C:\Users\<USER>\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.5.2\tools\sdk\include\eagle_soc.h
	PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0RXD_U);
}

// Configura o Wifi 
void setup_wifi() {
	
	delay(10);

	// Act as wifi client only.
	WiFi.mode(WIFI_STA);
	
	#ifdef _debug
		swSer.print(F("Connecting to "));
		swSer.println(WIFI_SSID);
	#endif

	WiFi.begin(WIFI_SSID, WIFI_PWD);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		#ifdef _debug
			swSer.print(F("."));
		#endif
	}
	
	// Set hostname
	WiFi.hostname(clientId);
	
	// Disconnect stations from the network established by the soft-AP.
	WiFi.softAPdisconnect(true);
	
	#ifdef _debug
		swSer.print(F("MAC: "));
		swSer.println(WiFi.macAddress());

		swSer.print(F("WiFi connected.\nIP address: "));
		swSer.println(WiFi.localIP());
	#endif
}