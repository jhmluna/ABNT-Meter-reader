#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <Credentials.h>

void setup() {
  Serial.begin(9600);
  while (!Serial)
  {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  /* Parâmetros do serviço MQTT
  Observation: const char *ptr is a pointer to a constant character. You cannot change the value pointed by ptr,
  but you can change the pointer itself. "const char *" is a (non-const) pointer to a const char.
  */

  const char mqtt_server[] = MQQT_SERVER_IP;
  Serial.println(mqtt_server);

  const char clientId[] = BASE_TOPIC;
  Serial.println(clientId);

  const int TOPIC_LENGTH = strlen(BASE_TOPIC);

  char dataTopic[TOPIC_LENGTH + 4]; // Topic used to send data to NodeRed
  strcpy(dataTopic, BASE_TOPIC);
  strcat(dataTopic, "data");
  Serial.println(dataTopic);

  char commandTopic[TOPIC_LENGTH + 7]; // Topic used to receive commands from NodeRed
  strcpy(commandTopic, BASE_TOPIC);
  strcat(commandTopic, "command");
  Serial.println(commandTopic);

  Serial.println(BASE_TOPIC);
  Serial.println(dataTopic);
  Serial.println(commandTopic);
}

void loop() {

}
