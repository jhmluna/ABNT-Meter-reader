#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

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

// Códigos ABNT NBR 14522:2008 para sincronizar comunicação
#define ENQ 0x05
#define ACK 0x06
#define NAK 0x15

#define SW_SERIAL_UNUSED_PIN -1

bool sendData = false;	// Flag para informar que foi enviado comando
bool newData = false;

// Set up serial
const int baudRate = 9600;
const int blockSize = 258;				// Response Block Size
byte receivedBytes[blockSize] = {};

// Software Serial for debug Conisdering ESP-01
SoftwareSerial swSer(SW_SERIAL_UNUSED_PIN, 0);

// Parâmetros do WiFi
const char *ssid = "SSID";         // your network SSID (name)
const char *password = "PASWD";    // your network key
WiFiClient espClient;

/* Parâmetros do serviço MQTT
Observation: const char *ptr is a pointer to a constant character. You cannot change the value pointed by ptr,
but you can change the pointer itself. "const char *" is a (non-const) pointer to a const char.
*/
const char *mqtt_server = "Broker-ip";
const char *clientId = "Your client identifier";
const char *commandTopic = "Your topic to transfer commands";	// Topic used to receive commands from NodeRed
const char *dataTopic = "Your topic to transfer data";			// Topic used to send data to NodeRed
// LWT message constants
byte willQos = 1;
const char *willTopic = "Your topic to transfer status";
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
const PROGMEM byte command_23[66] = {0x23, 0x12, 0x34, 0x56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
								  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
								  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
								  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
								  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
								  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x0b};

void setup() {
	Serial.setRxBufferSize(258); // Changes the RX buffer size as needed.
	Serial.begin(baudRate);		// Comm serial port. Must be changed to communication port after the end of development.
	
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
}

void loop() {
	
	ArduinoOTA.handle();
	
	// Check to see if it's time to read the meter data; that is, if the difference between the current
	// time and last time you read the meter is bigger than the interval at which you want to read it.
	unsigned long currentMillis = millis();
	
	if ((currentMillis - previousMillis >= interval) && (sendData == false)) {
		// Send command to the meter.
		sendCommand_23();
		
		// Save the last time you read the meter.
		previousMillis = currentMillis;
	}
	
    if (sendData == true) recvBytes();
    if (newData == true) showNewData();
	
	if (!client.connected()) reconnect();
	client.loop();
}

// Configura o Wifi 
void setup_wifi() {
	
	delay(10);
	
	#ifdef _debug
		swSer.print(F("Connecting to "));
		swSer.println(ssid);
	#endif
	
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		#ifdef _debug
			swSer.print(F("."));
		#endif
	}
	
	// Disconnect stations from the network established by the soft-AP.
	WiFi.softAPdisconnect(true);
	
	#ifdef _debug
		swSer.print(F(" WiFi connected.\nIP address: "));
		swSer.println(WiFi.localIP());
	#endif
}

// Desabilita pull up do pino referente ao GPIO3 que é o RX do ESP8266 para receber dados da porta ótica.
void disablePullUp() {
	// Desabilita pull up do pino referente ao RX GPIO3 - eagle_soc.h
	// C:\Users\<USER>\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.5.2\tools\sdk\include\eagle_soc.h
	PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0RXD_U);
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

// Tenta conexão/reconexão ao servidor MQTT
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
				swSer.println(F("Attempt number "));
				swSer.println(attempt);
			#endif
		}
	}
}

// Envia comando 23 para medidor.
void sendCommand_23() {
	/*	Comando 23 - Leitura de registradores dos canais visíveis
	Octeto 001: 23 Registradores após a última reposição de demanda (atuais)
	Octeto 002: Número de série do Leitor MSB
	Octeto 003: Número de série do Leitor
	Octeto 004: Número de série do Leitor LSB
	Octeto 005 até
	Octeto 064: NULL
	Octeto 065: CRC LSB
	Octeto 066: CRC MSB
	*/

	byte rb;				// Temp byte for Serial data receiving.
	Serial.end();			// Desabilita serial para garantir que a porta esteja em LOW
	delay(1000);			// Aguarda tempo para medidor reconhecer.
	Serial.begin(baudRate);		// Habilita serial
	disablePullUp();
	delay(1500);			// Aguarda tempo para medidor reconhecer início de transmissão.
	
	#ifdef _debug
		swSer.println(F("Start sendCommand_23"));
	#endif
	
	while (Serial.available() > 0) {
		rb = Serial.read();
		#ifdef _debug
			swSer.print(F("sendCommand_23: rb = ")); swSer.println(rb, HEX);
		#endif
	}
	
	if (rb == ENQ) {
		// Envia comando
		for (int k = 0; k < 66; k++) {
			byte myChar = pgm_read_byte(command_23 + k);
			Serial.write(myChar);
			
			#ifdef _printRx
				swSer.print(myChar, HEX);
				swSer.print(F(","));
			#endif
		}
		#ifdef _debug
			swSer.println(F("Command sent"));
		#endif
		
		sendData = true;		// Habilita 
	}
	else {
		sendData = false;
		
		// Publish status message indicating the client Node as Meter Not Connected.
		client.publish(willTopic, "MeterNotConnected");
		#ifdef _debug
			swSer.println(F("Não recebeu ENQ"));
		#endif
	}
}

// Envia ACK para medidor.
void sendAck() {
	//	Envia ACK significando que a última resposta transmitida foi recebida corretamente pelo leitor.

	Serial.end();			// Desabilita serial para garantir que a porta esteja em LOW
	delay(1000);			// Aguarda tempo para medidor reconhecer.
	Serial.begin(baudRate);		// Habilita serial
	disablePullUp();
	delay(1500);			// Aguarda tempo para medidor reconhecer início de transmissão.
	
	Serial.write(ACK);		// Envia ACK para medidor
	Serial.end();			// Desabilita serial para garantir que a porta esteja em LOW
			
		#ifdef _debug
			swSer.print(F("ACK command ")); swSer.print(ACK, HEX); swSer.println(F(" sent"));
		#endif
}

// Verifica se recebeu dado do medidor e salva na variável global receivedBytes.
void recvBytes() {
	static bool recvInProgress = false;
	static int ndx = 0;		// Counter for number of bytes received
	byte startMarker = 0x23;		// Response and Command Code
	byte rb;					// Temp byte for Serial data receiving.
	
	while (Serial.available() > 0) {
		
		rb = Serial.read();

		#ifdef _printRx
			swSer.print(F("recvBytes: rb = ")); swSer.println(rb,HEX);
		#endif
		
		if ((rb == ENQ || rb == NAK) && ndx == 0) {

			#ifdef _debug
				swSer.println(F("NAK, ENQ or other received data other than startMarker as first byte after sending data."));
			#endif
			
			sendData = false;
		}

		if (rb == startMarker && recvInProgress == false) {
			recvInProgress = true;
			#ifdef _debug
				swSer.println(F("Start Reception"));
			#endif
		}

		if (recvInProgress == true) {
      if (ndx < blockSize) {
        receivedBytes[ndx] = rb;
				#ifdef _printRx
					//swSer.print(F("recvBytes: rb = "));
					//swSer.println(rb,HEX);
					swSer.print(F(" ndx = "));
					swSer.println(ndx);
				#endif
				ndx++;
			}
      else {
        recvInProgress = false;
        ndx = 0;
				newData = true;
				sendData = false;
				#ifdef _debug
					swSer.println(F("End of reception"));
				#endif
				sendAck();
			}
		}
		
		yield();		// Efetua o feed do SW WDT.
	}
}

// Calcular CRC do dado recebido do medidor. Caso esteja ok, chama a função de publicação da informação no broker.
void showNewData() {
    if (newData == true) {

		#ifdef _debug
			swSer.println(F("Start showNewData"));
		#endif
		
		// Disable newData flag
		newData = false;
		
		// Calcula CRC dos bytes recebidos. Se diferente de 0, sai da função.
		unsigned int crc = calcula_crc16(receivedBytes, blockSize);
		if (crc != 0)
		{
			#ifdef _debug
				swSer.println(F("Erro de CRC."));
			#endif
			return;
		}
		
		unsigned long meterId = converteId(receivedBytes);
		unsigned long *meter_data = converte_energia(receivedBytes);
		publish_data(meterId, meter_data);

		#ifdef _printRx
			// Exibe dados caso CRC esteja ok.
			swSer.println(F("CRC = ")); swSer.print(crc);
			swSer.println(F("Received HEX values"));
			for (int n = 0; n < blockSize; n++) {
				swSer.print(receivedBytes[n], HEX);
				swSer.print(F(" "));
			}
			swSer.println();
		#endif
    }
}

// Imprime na tela a energia e o tipo com base no campo escopo.
void publish_data(unsigned long meterId, unsigned long meter_data[3]) {
	char msg[13];
	
	if (!client.connected()) reconnect();
	client.loop();
	
	#ifdef _debug
		swSer.print(F("Publish message: "));
	#endif

	// Publish serial number
	snprintf (msg, 10, "A0%lu", meterId);
	client.publish(dataTopic, msg);
	#ifdef _debug
		swSer.print(msg);
	#endif
	
	// Publish active energy
	snprintf (msg, 13, "A2%lu", meter_data[0]);
	client.publish(dataTopic, msg);
	#ifdef _debug
		swSer.print(msg);
	#endif
	
	// Publish reactive energy
	snprintf (msg, 13, "A7%lu", meter_data[1]);
	client.publish(dataTopic, msg);
	#ifdef _debug
		swSer.print(F(", ")); swSer.print(msg);
	#endif
	
	// Publish demand
	snprintf (msg, 10, "A13%lu", meter_data[2]);
	client.publish(dataTopic, msg);
	#ifdef _debug
		swSer.print(F(", ")); swSer.println(msg);
	#endif
}

// Converte energias salva considerando 5 bytes DADOS
unsigned long * converte_energia(byte *message_energia) {
	
	static unsigned long energias[3];
	unsigned long ativa = 0, reativa = 0, demanda = 0;
	unsigned long multiplicador[5] = {10000000UL, 1000000UL, 10000UL, 100UL, 1UL};
	
	// Collect active and reactive energy
	for (int j = 0; j <= 4; j++) {
		ativa = ativa + (bcdToDec(message_energia[j + 5]) * multiplicador[j]);
		reativa = reativa + (bcdToDec(message_energia[j + 79]) * multiplicador[j]);
	}
	// Collect demand
	for (int i = 0; i <= 2; i++) {
		demanda = demanda + (bcdToDec(message_energia[i + 40]) * multiplicador[i + 2]);
	}
	energias[0] = ativa*2/1000;
	energias[1] = reativa*2/1000;
	energias[2] = demanda*8;
	return energias;
}

// Converte para valor numérico o número de série salvo nos octetos 002 a 005.
unsigned long converteId(byte *message_id) {
	
	unsigned long multiplicador[4] = {100000000UL, 10000UL, 100UL, 1UL};
	unsigned long valor = 0;
	for (int j = 0; j <= 3; j++) {
		valor = valor + (unsigned long)(bcdToDec(message_id[j + 1]) * multiplicador[j]);
	}
	return valor;
}

// Converte formato BCD para decimal.
byte bcdToDec(byte val) {
	return( (val/16*10) + (val%16) );
}

// Calcula CRC da mensagem recebida pela serial.
unsigned int calcula_crc16(byte *array, int tamanho_buffer) {

/* Fonte: ETC 3.11 – Especificação Técnica Para Saída Serial Assíncrona Unidirecional 
https://www.copel.com/hpcopel/root/pagcopel2.nsf/0/4310F832B8AD31D00325776F005DCDDB/$FILE/ETC311%20Saida%20Serial%20Medidores%20eletronicos.pdf
*/
	unsigned int retorno = 0;
	unsigned int crcpolinv = 0x4003;
	unsigned int i;
	unsigned int j;
	for (i = 0; i < tamanho_buffer; i++) {
		retorno ^= (array[i] & 0xFF);
		for (j = 0; j < 8; j++) {
			if ((retorno & 0x0001) != 0) {
				retorno ^= crcpolinv;
				retorno >>= 1;
				retorno |= 0x8000;
			}
			else {
				retorno >>= 1;
			}
		}
	}
	return retorno;
}
