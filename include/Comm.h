/*
  Comm.h - Header for communication configuration.
*/

#ifndef COMM_H
#define COMM_H

// Set up serial
const int baudRate = 9600;
const int blockSize = 258;				// Response Block Size

// To set RX as unused pin in software serial as it is only used for print debug messages
#define SW_SERIAL_UNUSED_PIN -1

#endif