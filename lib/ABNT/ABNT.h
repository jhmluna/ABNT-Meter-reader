/*
	ABNT.h - Library to keep ABNT standard information.
*/
#ifndef ABNT_H
#define ABNT_H

#include <Arduino.h>
#include <SoftwareSerial.h>

#define _debug			// Set debug mode for development.
// #define _printRx		// Sets mode for printing received bytes. Used to increase the RX buffer.

// Set up serial
const int baudRate = 9600; // Serial Baude rate for meter communication
const int blockSize = 258; // Response Block Size

class Abnt
{
	// Códigos ABNT NBR 14522:2008 para sincronizar comunicação
	const byte _ENQ = 0x05;
	const byte _ACK = 0x06;
	const byte _NAK = 0x15;
	const byte _STARTMARKER = 0x23;

  byte receivedBytes[blockSize];  // Array to store the data received from the meter.
	const byte command_23[66];  	  // Array with Command 23 - Reading registers of visible channels.
	SoftwareSerial & _debugPort;    // Serial port for debug purposes.

  // Private functions
	void sendAck(void);
	unsigned int crc16Calc(byte *array, unsigned int tamanho_buffer);
	byte bcdToDec(byte val);
	void disablePullUp(void);

public:
	Abnt(SoftwareSerial & ss)
			: command_23{0x23, 0x12, 0x34, 0x56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
									 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
									 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
									 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
									 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
									 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x0b}
			, _debugPort(ss)
	{
	}
	bool sendCommand_23(void);
	bool receiveBytes(void);
  unsigned long getEnergy(bool energyType);
  unsigned long getDemand(void);
  unsigned long getSerialNumber(void);
};

#endif