#include <Arduino.h>
#include <Credentials.h>

char dataTopic[TOPIC_LENGTH + 4];     // Topic used to send data to NodeRed
char commandTopic[TOPIC_LENGTH + 7];     // Topic used to send data to NodeRed

void setup() {
	Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
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
