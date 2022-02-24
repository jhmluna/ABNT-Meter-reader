#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <Credentials.h>

/* Parâmetros do serviço MQTT
Observation: const char *ptr is a pointer to a constant character. You cannot change the value pointed by ptr,
but you can change the pointer itself. "const char *" is a (non-const) pointer to a const char.
*/
const char *mqtt_server = MQQT_SERVER_IP;
const char *clientId = BASE_TOPIC;
char commandTopic[TOPIC_LENGTH + 7];  // Topic used to receive commands from NodeRed
char dataTopic[TOPIC_LENGTH + 4];     // Topic used to send data to NodeRed

void setup() {

}

void loop() {
    
}