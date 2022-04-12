#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <string>
#include "Credentials.h"
#include "ABNT.h"

// Function prototypes
void setup_wifi(void);
void disablePullUp(void);
std::string generateJson(void);
void reconnect(void);
void callback(char* topic, byte* payload, unsigned int length);

#ifdef _debug
	// Set RX as unused pin in software serial as it is only used for print debug messages
	#define SW_SERIAL_UNUSED_PIN -1
	// Software Serial for debug Conisdering ESP-01
	SoftwareSerial swSer(SW_SERIAL_UNUSED_PIN, 0);
#endif

/* Will store last time meter was readen
	Generally, you should use "unsigned long" for variables that hold time
	The value will quickly become too large for an int to store
*/
unsigned long previousMillis = 0;
const long interval = 15000;   // Interval at which to pooling the meter (milliseconds)

// Sets the server details and message callback function.
WiFiClient espClient;
PubSubClient client(mqtt_server, 1883, callback, espClient);

// MQTT config and LWT message constants
char dataTopic[TOPIC_LENGTH + 4];     // Topic used to send data to NodeRed
char commandTopic[TOPIC_LENGTH + 7];  // Topic used to send command to NodeRed
char willTopic[TOPIC_LENGTH + 6];			// Topic used to send status to NodeRed
byte willQos = 1;
const char *willMessage = "Offline";
bool willRetain = true;

// Abnt class for meter communication
Abnt abnt(swSer);

void setup() {
	Serial.setRxBufferSize(blockSize);
	Serial.begin(baudRate);		// Comm Serial port.

	#ifdef _debug				// Debug serial port. Must be disabled after the end of development.
		swSer.begin(baudRate);
		swSer.println(F("Setup start."));
	#endif

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
		swSer.println(F("\nEnd"));
	});

	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		swSer.printf("Progress: %u%%\r", (progress / (total / 100)));
	});

	ArduinoOTA.onError([](ota_error_t error) {
		swSer.printf("Error[%u]: ", error);
		if (error == OTA_AUTH_ERROR) {
			swSer.println(F("Auth Failed"));
		} else if (error == OTA_BEGIN_ERROR) {
			swSer.println(F("Begin Failed"));
		} else if (error == OTA_CONNECT_ERROR) {
			swSer.println(F("Connect Failed"));
		} else if (error == OTA_RECEIVE_ERROR) {
			swSer.println(F("Receive Failed"));
		} else if (error == OTA_END_ERROR) {
			swSer.println(F("End Failed"));
		}
	});

	ArduinoOTA.begin();

	#ifdef _debug
		swSer.print(F("End of Setup."));
	#endif
	
	// 
	strcpy(dataTopic, BASE_TOPIC);
  strcat(dataTopic, "/data");

  strcpy(commandTopic, BASE_TOPIC);
  strcat(commandTopic, "/command");

	strcpy(willTopic, BASE_TOPIC);
  strcat(willTopic, "/status");

	#ifdef _debug
		swSer.println("");
		swSer.print(F("Tamanho do tópico: "));
		swSer.println(TOPIC_LENGTH);
		swSer.println(BASE_TOPIC);
		swSer.println(dataTopic);
		swSer.println(commandTopic);
		swSer.println(willTopic);
		swSer.println(clientId);
	#endif
}

void loop() {
	static bool commandSent = false;
	ArduinoOTA.handle();
	
	// Check to see if it's time to read the meter data; that is, if the difference between the current
	// time and last time you read the meter is bigger than the interval at which you want to read it.
	unsigned long currentMillis = millis();

	if ((currentMillis - previousMillis >= interval) && !commandSent) {
		commandSent = abnt.sendCommand_23();
		// Save the last time the meter was readen.
		previousMillis = currentMillis;
	}
	commandSent = abnt.receiveBytes();	// It's only true when receive all data from the meter.
	if (commandSent) {
		std::string meterMessage = generateJson();

		// Publish meter data in Node Red
		client.publish(dataTopic, meterMessage.c_str());
		#ifdef _debug
			swSer.print(F("Publish message: "));
			swSer.println(meterMessage.c_str());
		#endif
	
		commandSent = !commandSent;		// 
	}
	if (!client.connected()) reconnect();
	client.loop();
}

// Configure Wifi 
void setup_wifi() {
	
	delay(10);

	// Act as wifi client only.
	WiFi.mode(WIFI_STA);
	
	#ifdef _debug
		swSer.print(F("Connecting to "));
		swSer.println(WIFI_SSID);
	#endif

	WiFi.begin(WIFI_SSID, WIFI_PWD);	// Saved in Credentials.h
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		#ifdef _debug
			swSer.print(F("."));
		#endif
	}
	
	// Set hostname
	WiFi.hostname(clientId);	// Saved in Credentials.h
	
	// Disconnect stations from the network established by the soft-AP.
	WiFi.softAPdisconnect(true);
	
	#ifdef _debug
		swSer.print(F("MAC: "));
		swSer.println(WiFi.macAddress());

		swSer.print(F("WiFi connected.\nIP address: "));
		swSer.println(WiFi.localIP());
	#endif
}

// Generate JSON formatted String
std::string generateJson() {
	const int capacity = JSON_OBJECT_SIZE(4);		// Set JsonDocument capacity to hold 4 members.
	StaticJsonDocument<capacity> doc;
	// Add members to JsonDocument
	doc["kwh"] = abnt.getEnergy(true);
	doc["kvarh"] = abnt.getEnergy(false);
	doc["kw"] = abnt.getDemand();
	doc["sn"] = abnt.getSerialNumber();
	// Return JsonDocument serialized.
	std::string JsonString = "";
	serializeJson(doc, JsonString);
	return JsonString;
}

// MQTT Server connection/reconnection
void reconnect() {
	// Loop until we're reconnected
	
	int attempt = 0;
	while (!client.connected()) {
		
		#ifdef _debug
			swSer.print(F("Attempting MQTT connection..."));
		#endif

		// Attempt to connect
		if (client.connect(clientId, willTopic, willQos, willRetain, willMessage)) {
			delay(1);

			// Subscribe to message Topic defined.
			client.subscribe(commandTopic);
			
			#ifdef _debug
				swSer.println("Connected as " + String(clientId));
				swSer.println("Subscribe to topic " + String(commandTopic));
				swSer.println("Data are send to topic " + String(dataTopic));
			#endif
			
			// Publish status message indicating the client Node as online.
			client.publish(willTopic, "Online");
		}
		else {
			
			#ifdef _debug
				swSer.print(F("Failed, rc = "));
				swSer.println(client.state());
				swSer.println(F("Try again in 5 seconds."));
			#endif
			
			attempt += 1;
			if (attempt >= 20) ESP.restart();
			
			// Wait 5 seconds before retrying
			delay(5000);
			
			#ifdef _debug
				swSer.print(F("Attempt number: "));
				swSer.println(attempt);
			#endif
		}
	}
}

// Message callback function called when a message arrives for a subscription created by this client.
void callback(char* topic, byte* payload, unsigned int length) {
	swSer.print("Message arrived [");
	swSer.print(topic);
	swSer.print("] ");
	for (int i = 0; i < length; i++) {
		swSer.print((char)payload[i]);
	}
	swSer.println();
}
