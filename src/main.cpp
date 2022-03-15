#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <Credentials.h>
#include <ABNT.h>

#define _debug			// Set debug mode for development.
//#define _printRx		// Sets mode for printing received bytes. Used to increase the RX buffer.

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

char dataTopic[TOPIC_LENGTH + 4];     // Topic used to send data to NodeRed
char commandTopic[TOPIC_LENGTH + 7];     // Topic used to send data to NodeRed

void setup() {
	Serial.begin(baudRate);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  	Serial.begin(baudRate);		// Debug serial port. Must be changed to communication port after the end of development.
	Serial.setRxBufferSize(258);
	swSer.begin(baudRate);		// Comm serial port. Must be disabled after the end of development.

	swSer.print(F("Início do Setup."));

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

	swSer.print(F("Fim do Setup."));

  strcpy(dataTopic, BASE_TOPIC);
  strcat(dataTopic, "data");

  strcpy(commandTopic, BASE_TOPIC);
  strcat(commandTopic, "command");

  Serial.println("");
  Serial.print("Tamanho do tópico: ");
  Serial.println(TOPIC_LENGTH);
  Serial.print(BASE_TOPIC);
  Serial.print(dataTopic);
  Serial.print(commandTopic);
}

void loop() {

}
