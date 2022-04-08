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

void Abnt::printArray() {
	for (int k = 0; k < blockSize; k++) {
		_debugPort.print(receivedBytes[k], HEX);
		_debugPort.print(F(","));
	}
}