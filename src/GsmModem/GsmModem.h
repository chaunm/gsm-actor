/*
 * GsmModem.h
 *
 *  Created on: Jun 30, 2016
 *      Author: ChauNM
 */

#ifndef GSMMODEM_H_
#define GSMMODEM_H_

#include "serialcommunication.h"

#ifdef PI_RUNNING
#define GSM_POWER_PIN		22
#define GSM_STATUS_PIN		21
#endif

#define GSM_MODEM_ACTIVE		0x00
#define	GSM_MODEM_WAITING		0x01
#define GSM_MODEM_CMD_ERROR		0x02

#define COMMAND_SUCCESS		0x00
#define COMMAND_ERROR		0x01
#define COMMAND_TIMEOUT		0x02

#define NO_SIGNAL			0
#define SIGNAL_POOR			1
#define SIGNAL_FAIR			2
#define SIGNAL_GOOD			3
#define SIGNAL_EXCELLENT	4

#define CHECK_RSSI_PERIOD		30
#define CHECK_CARRIER_PERIOD	30
#define CHECK_BILLING_PERIOD 	86400

#define GSM_COMMAND_TIMEOUT	300
typedef struct tagGSMDEVICE {
	PSERIAL serialPort;
	BYTE 	status;
	BYTE 	signalStrength;
	BYTE 	powerMode;
	BOOL 	simStatus;
	char* 	waitingCommand;
	char* 	commandStatus;
	char* 	carrier;
	char* 	phoneNumber;
} GSMMODEM, *PGSMMODEM;

BYTE GsmModemSendSms(const char* number, const char* message);
BYTE GsmModemMakeCall(const char* number);
BOOL GsmModemInit(char* SerialPort, int ttl);
VOID GsmModemDeInit();

#endif /* GSMMODEM_H_ */
