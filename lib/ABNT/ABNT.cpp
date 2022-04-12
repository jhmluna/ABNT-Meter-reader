/*
  ABNT.c - Functions to work with ABNT standard
*/

#include <Arduino.h>
#include "ABNT.h"

// Envia sequência de bytes referentes ao comando 23 da norma
bool Abnt::sendCommand_23() {
  byte rb = 0;			// Temp byte for Serial data receiving.
	Serial.end();			// Desabilita serial para garantir que a porta esteja em LOW
	delay(1000);			// Aguarda tempo para medidor reconhecer.
	Serial.begin(baudRate);		// Habilita serial

  /* Desabilita pull up do pino referente ao GPIO3 que é o RX do ESP8266 para receber dados da porta ótica.
	  C:\Users\<USER>\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.5.2\tools\sdk\include\eagle_soc.h
  */
	PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0RXD_U);
	delay(1500);			// Aguarda tempo para medidor reconhecer início de transmissão.
	
	#ifdef _debug
		_debugPort.println(F("Start sendCommand_23"));
	#endif

  while (Serial.available() > 0) {
		rb = Serial.read();
		#ifdef _printRx
			_debugPort.print(F("sendCommand_23: rb = ")); _debugPort.println(rb, HEX);
		#endif
	}
	
	if (rb == _ENQ) {
		// Envia comando
		for (int k = 0; k < 66; k++) {
			byte myChar = pgm_read_byte(command_23 + k);
			Serial.write(myChar);
			
			#ifdef _printRx
				_debugPort.print(myChar, HEX);
				_debugPort.print(F(","));
			#endif
		}
		#ifdef _debug
			_debugPort.println(F("Command sent"));
		#endif
		
		return true;
	}
	else {
		#ifdef _debug
			_debugPort.println(F("Não recebeu ENQ"));
		#endif

    return false;
	}
}

// Verifica se recebeu dado do medidor e salva na variável global receivedBytes.
bool Abnt::receiveBytes() {
	bool completeDataReceived = false;
	static bool receivingInProgress = false;
	static int ndx = 0;		// Counter for number of bytes received
	byte rb;					// Temp byte for Serial data receiving.

	while (Serial.available() > 0) {
		
		rb = Serial.read();

		if ((rb == _ENQ || rb == _NAK) && ndx == 0) {

			#ifdef _printRx
				_debugPort.println(F("NAK, ENQ or other received data other than startMarker as first byte after sending command."));
			#endif
		}

		if (rb == _STARTMARKER && receivingInProgress == false) {
			receivingInProgress = true;
			#ifdef _debug
				_debugPort.println(F("Start Reception."));
			#endif
		}

		if (receivingInProgress == true) {
      if (ndx < blockSize) {
        receivedBytes[ndx] = rb;
				#ifdef _printRx
					//_debugPort.print(F("receiveBytes: rb = "));
					//_debugPort.println(rb,HEX);
					_debugPort.print(F(" ndx = "));
					_debugPort.println(ndx);
				#endif
				ndx++;
			}
      else {
        receivingInProgress = false;
        ndx = 0;
				completeDataReceived = true;
				#ifdef _debug
					_debugPort.print(F("End of reception. "));
					_debugPort.println(F("Send Ack!"));
				#endif
				sendAck();
			}
		}
		yield();		// Efetua o feed do SW WDT.
	}
	return completeDataReceived;
}

// Envia ACK para medidor.
void Abnt::sendAck() {
	//	Envia ACK significando que a última resposta transmitida foi recebida corretamente pelo leitor.

	Serial.end();			// Desabilita serial para garantir que a porta esteja em LOW
	delay(1000);			// Aguarda tempo para medidor reconhecer.
	Serial.begin(baudRate);		// Habilita serial

	/* Desabilita pull up do pino referente ao GPIO3 que é o RX do ESP8266 para receber dados da porta ótica.
	  C:\Users\<USER>\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.5.2\tools\sdk\include\eagle_soc.h
  */
	PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0RXD_U);
	delay(1500);			// Aguarda tempo para medidor reconhecer início de transmissão.
	
	Serial.write(_ACK);		// Envia ACK para medidor
	Serial.end();			// Desabilita serial para garantir que a porta esteja em LOW
			
		#ifdef _debug
			_debugPort.print(F("ACK command ")); _debugPort.print(_ACK, HEX); _debugPort.println(F(" sent"));
		#endif
}

// Calcula CRC da mensagem recebida pela serial.
unsigned int Abnt::crc16Calc(byte *array, unsigned int tamanho_buffer) {

/* Source: ETC 3.11 – Especificação Técnica Para Saída Serial Assíncrona Unidirecional 
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

// Converte formato BCD para decimal.
byte Abnt::bcdToDec(byte val) {
	return( (val/16*10) + (val%16) );
}

// Collect active and reactive energy saved in octets 006 to 010 and 080 to 084.
// energyType: true -> kwh, false -> kvarh
unsigned long Abnt::getEnergy(bool energyType) {
	unsigned long energy = 0;
	unsigned long multiplicador[5] = {100000000UL, 1000000UL, 10000UL, 100UL, 1UL};
	int energyIndex = energyType ? 5 : 79;
	for (int j = 0; j <= 4; j++) {
		energy = energy + (bcdToDec(receivedBytes[j + energyIndex]) * multiplicador[j]);
	}
	return energy * 2 / 1000;
}

// Collect demand saved in octets 041 to 043.
unsigned long Abnt::getDemand(void) {
	unsigned long demand = 0;
	unsigned long multiplicador[3] = {10000UL, 100UL, 1UL};
	for (int j = 0; j <= 2; j++) {
		demand = demand + (bcdToDec(receivedBytes[j + 40]) * multiplicador[j]);
	}
	return demand * 8;
}

// Converte para valor numérico o número de série salvo nos octetos 002 a 005.
unsigned long Abnt::getSerialNumber(void) {
	unsigned long serialNumber = 0;
	unsigned long multiplicador[4] = {1000000UL, 10000UL, 100UL, 1UL};
	for (int j = 0; j <= 3; j++) {
		serialNumber = serialNumber + (unsigned long)(bcdToDec(receivedBytes[j + 1]) * multiplicador[j]);
	}
	return serialNumber;
}

// Desabilita pull up do pino referente ao GPIO3 que é o RX do ESP8266 para receber dados da porta ótica.
void Abnt::disablePullUp() {
		// Desabilita pull up do pino referente ao RX GPIO3 - eagle_soc.h
	// C:\Users\<USER>\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.5.2\tools\sdk\include\eagle_soc.h
	PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0RXD_U);
}