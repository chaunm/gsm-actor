/*
 * GsmModem.c
 *
 *  Created on: Jun 30, 2016
 *      Author: ChauNM
 */

#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef PI_RUNNING
#include <wiringPi.h>
#endif
#include "GsmModem.h"
#include "universal.h"
#include "ATCommand.h"
#include "serialcommunication.h"
#include "GsmActor.h"
#include "../log/log.h"

PGSMMODEM gsmModem = NULL;
WORD gsmCheckBillingCount;
BYTE gsmRestartCount;

static BOOL GsmModemRestart();

static VOID GsmSetRestart(BYTE nCount)
{
	gsmRestartCount = nCount;
}

BYTE GsmModemGetStatus()
{
	return gsmModem->status;
}

VOID GsmModemSetStatus(char status, char* command)
{
	gsmModem->status = status;
	char* newCommand = StrDup(command);
	if (gsmModem->waitingCommand != NULL)
	{
		free(gsmModem->waitingCommand);
		gsmModem->waitingCommand = NULL;
	}
	gsmModem->waitingCommand = newCommand;
}


VOID GsmModemSetCmdStatus(char* commandStatus)
{
	//free old status
	char* newCmdStatus = StrDup(commandStatus);
	if (gsmModem->commandStatus != NULL)
	{
		free(gsmModem->commandStatus);
		gsmModem->commandStatus = NULL;
	}
	gsmModem->commandStatus = newCmdStatus;
}

BYTE GsmModemExecuteCommand(char* command)
{
	WORD timeout;
	if (gsmModem->status == GSM_MODEM_OFF)
		return COMMAND_ERROR;
	while(gsmModem->status != GSM_MODEM_ACTIVE)
	{
		sleep(1);
	}
	GsmModemSetCmdStatus(NULL);
	atSendCommand(command, gsmModem->serialPort);
	GsmModemSetStatus(GSM_MODEM_WAITING, command);
	timeout = 0;
	char* eventMessage = malloc(50);
	memset(eventMessage, 0, 50);
	while (gsmModem->status == GSM_MODEM_WAITING)
	{
		usleep(100000);
		timeout++;
		if (timeout == GSM_COMMAND_TIMEOUT)
		{
			printf("command %s timeout\n", gsmModem->waitingCommand);
			sprintf(eventMessage, "command.%s.timeout", gsmModem->waitingCommand);
			LogWrite(eventMessage);
			GsmActorPublishGsmErrorEvent(eventMessage, NULL);
			GsmModemSetStatus(GSM_MODEM_ACTIVE, NULL);
			free(eventMessage);
			return COMMAND_TIMEOUT;
		}
	}
	if (gsmModem->status == GSM_MODEM_CMD_ERROR)
	{
		printf("command %s failed\n", gsmModem->waitingCommand);
		sprintf(eventMessage, "command.%s.error", gsmModem->waitingCommand);
		LogWrite(eventMessage);
		GsmActorPublishGsmErrorEvent(eventMessage, gsmModem->commandStatus);
		GsmModemSetStatus(GSM_MODEM_ACTIVE, NULL);
		free(eventMessage);
		return COMMAND_ERROR;
	}
	free(eventMessage);
	return COMMAND_SUCCESS;
}

// Day la ham duoc goi trong vong quet buffer nhan
char* GsmModemHandleCommandMessage(char* command, char* message)
{
	char* commandStatus = NULL;
	printf("command %s, status %s\n", command, message);
	// command = NULL mean event from device
	commandStatus = StrDup(message);
	if (command == NULL)
	{
		// get incoming call information
		if (strstr(message, "+CLIP: \""))
		{
			atHandleClipEvent(message);
			//end call
			atSendCommand("ATH", gsmModem->serialPort);
		}
		// get sms information
		if (strstr(message, "+CMT: \""))
		{
			atHandleCmtEvent(message);
		}
		// billing report
		if (strstr(message, "+CUSD: "))
		{
			atHandleCusdEvent(message);
		}
	}
	return commandStatus;
}

VOID GsmModemProcessIncoming(void* pParam, char* message)
{
	PGSMMODEM gsmModem = (PGSMMODEM)pParam;
	if ((strcmp(message, "OK") == 0))
	{
		printf("command %s execute ok\n", gsmModem->waitingCommand);
		GsmModemSetStatus(GSM_MODEM_ACTIVE, NULL);
	}
	else if (strcmp(message, "ERROR") == 0)
	{
		printf("command %s execute failed\n", gsmModem->waitingCommand);
		GsmModemSetStatus(GSM_MODEM_CMD_ERROR, gsmModem->waitingCommand);
		GsmModemSetCmdStatus(message);
	}
	else
	{
		char* commandStatus = GsmModemHandleCommandMessage(gsmModem->waitingCommand, message);
		if(commandStatus != NULL)
		{
			GsmModemSetCmdStatus(commandStatus);
			//CME ERROR Report
			if (strstr(message, "+CME ERROR:") || strstr(message, "+CMS ERROR:"))
			{
				GsmModemSetStatus(GSM_MODEM_CMD_ERROR, gsmModem->waitingCommand);
			}
			free(commandStatus);
		}
	}
}

BOOL GsmModemCheckCarrierRegister()
{
	char* currentCarrier = NULL;
	BYTE index;
	BYTE carrierStart = 0;
	if(GsmModemExecuteCommand("AT+COPS?") == COMMAND_SUCCESS)
	{
		currentCarrier = malloc(50);
		memset(currentCarrier, 0, 50);
		for (index = 0; index < strlen(gsmModem->commandStatus); index++)
		{
			if ((carrierStart > 0) && (gsmModem->commandStatus[index] != '"'))
			{
				currentCarrier[index - carrierStart] = gsmModem->commandStatus[index];
			}
			if (gsmModem->commandStatus[index] == '"')
			{
				if(carrierStart == 0)
					carrierStart = index + 1;
				else
					break;
			}
		}
		if (strlen(currentCarrier) == 0)
		{
			free(currentCarrier);
			return TRUE;
		}
		if (gsmModem->carrier != NULL)
		{
			if (strcmp(gsmModem->carrier, currentCarrier) != 0)
			{
				free(gsmModem->carrier);
				gsmModem->carrier = StrDup(currentCarrier);
				// publish
				//GsmActorPublishGsmCarrier(currentCarrier);
			}
		}
		else
		{
			gsmModem->carrier = StrDup(currentCarrier);
			// publish
			//GsmActorPublishGsmCarrier(currentCarrier);
		}
		free(currentCarrier);
		return TRUE;
	}
	else
	{
		if(gsmModem->carrier != NULL)
		{
			free(gsmModem->carrier);
			gsmModem->carrier = NULL;
		}
		return FALSE;
	}
}

BYTE GsmModemCheckBilling()
{
	BYTE result;
	char* command = malloc(50);
	memset(command, 0, 50);
	strcpy(command, "AT+CUSD=1,\"*101#\"");
	result = GsmModemExecuteCommand(command);
	free(command);
	return result;
}

BYTE GsmModemSendSms(const char* number, const char* message)
{
	if ((gsmModem == NULL) || (message == NULL) ||(number == NULL)) return COMMAND_SUCCESS;
	char* command = malloc(255);
	BYTE timeout;
	strcpy(command, "AT+CMGS=");
	strcat(command, "\"");
	strcat(command, number);
	strcat(command, "\"");
	printf("send command %s\n", command);
	atSendCommand(command, gsmModem->serialPort);
	GsmModemSetStatus(GSM_MODEM_WAITING, command);
	usleep(100000);
	memset(command, 0, 255);
	strcpy(command, message);
	strcat(command, "\x1a\r\n");
	SerialOutput(gsmModem->serialPort, (PBYTE)command, strlen(command));
	free(command);
	timeout = 0;
	char* eventMessage = malloc(50);
	memset(eventMessage, 0, 50);
	while (gsmModem->status == GSM_MODEM_WAITING)
	{
		usleep(100000);
		timeout++;
		if (timeout == GSM_COMMAND_TIMEOUT)
		{
			printf("command %s timeout\n", gsmModem->waitingCommand);
			sprintf(eventMessage, "command.%s.timeout", gsmModem->waitingCommand);
			GsmActorPublishGsmErrorEvent(eventMessage, NULL);
			GsmModemSetStatus(GSM_MODEM_ACTIVE, NULL);
			free(eventMessage);
			return COMMAND_TIMEOUT;
		}
	}
	if (gsmModem->status == GSM_MODEM_CMD_ERROR)
	{
		printf("send sms failed %s\n", gsmModem->waitingCommand);
		sprintf(eventMessage, "command.%s.error", gsmModem->waitingCommand);
		GsmActorPublishGsmErrorEvent(eventMessage, gsmModem->commandStatus);
		GsmModemSetStatus(GSM_MODEM_ACTIVE, NULL);
		free(eventMessage);
		return COMMAND_ERROR;
	}
	free(eventMessage);
	GsmModemCheckBilling();
	return COMMAND_SUCCESS;
}

BYTE GsmModemMakeCall(const char* number)
{
	BYTE result;
	if (number == NULL)
		return COMMAND_SUCCESS;
	char* command = malloc(50);
	memset(command, 0, 50);
	strcpy(command, "ATD");
	strcat(command, number);
	strcat(command, ";");
	result = GsmModemExecuteCommand(command);
	free(command);
	//GsmModemCheckBilling();
	gsmCheckBillingCount = 120;
	return result;
}

BYTE GsmModemCheckRssi()
{
	BYTE signalStrength;
	BYTE result;
	char* signal = malloc(3);
	memset(signal, 0, 3);
	result = GsmModemExecuteCommand("AT+CSQ");
	if (result == COMMAND_SUCCESS)
	{
		if (strstr(gsmModem->commandStatus, "+CSQ: "))
		{
			memcpy(signal, (void*)(gsmModem->commandStatus + 6), 2);
			signalStrength = atoi(signal);
			if (signalStrength < 2)
				gsmModem->signalStrength = NO_SIGNAL;
			else if ((signalStrength > 1) && (signalStrength < 10))
				gsmModem->signalStrength = SIGNAL_POOR;
			else if ((signalStrength > 9) && (signalStrength < 15))
				gsmModem->signalStrength = SIGNAL_FAIR;
			else if ((signalStrength > 14) && (signalStrength < 20))
				gsmModem->signalStrength = SIGNAL_GOOD;
			else
				gsmModem->signalStrength = SIGNAL_EXCELLENT;
		}
	}
	free(signal);
	return result;
}

BYTE GsmCheckSimCard()
{
	static WORD nCount = CHECK_CARRIER_PERIOD - 1;
	nCount++;
	if (nCount == 10 * CHECK_SIM_CARD_PERIOD)
		nCount = 0;
	if ((nCount % CHECK_SIM_CARD_PERIOD) != 0)
		return COMMAND_SUCCESS;
	if (GsmModemExecuteCommand("AT+CPIN?") == COMMAND_SUCCESS)
	{
		gsmModem->simStatus = TRUE;
		return COMMAND_SUCCESS;
	}
	else if (gsmModem->simStatus == TRUE)
	{
		GsmActorPublishGsmErrorEvent("simcard", NULL);
		gsmModem->simStatus = FALSE;
	}
	LogWrite("Sim card error");
	return COMMAND_ERROR;
}

VOID GsmCheckStatus()
{
	if (digitalRead(GSM_STATUS_PIN) == LOW)
	{
		if (gsmModem->status != GSM_MODEM_OFF)
		{
			gsmModem->status = GSM_MODEM_OFF;
			GsmActorPublishGsmErrorEvent("sim800.off", NULL);
			LogWrite("Gsm Modem turned off");
			GsmSetRestart(5);
		}
	}
	else
		GsmSetRestart(0);
	if (gsmRestartCount > 0)
	{
		gsmRestartCount--;
		printf("%d\n", gsmRestartCount);
		if (gsmRestartCount == 0)
			GsmModemRestart();
	}
}
BOOL GsmGetPhoneNumber()
{
	if (GsmModemExecuteCommand("AT+CNUM") == COMMAND_SUCCESS)
	{
		if (gsmModem->phoneNumber != NULL)
			free(gsmModem->phoneNumber);
		gsmModem->phoneNumber = atHandleCnumEvent(gsmModem->commandStatus);
		GsmActorPublishPhoneNumber(gsmModem->phoneNumber);
	}
	return FALSE;
}

VOID GsmReportCarrier()
{
	char* oldCarrier = StrDup(gsmModem->carrier);
	BYTE oldsignalStrength = gsmModem->signalStrength;
	static WORD nCount = CHECK_CARRIER_PERIOD - 1;
	/* check billing report if needed */
	if (gsmCheckBillingCount > 0)
	{
		gsmCheckBillingCount--;
		if (gsmCheckBillingCount == 0)
			GsmModemCheckBilling();
	}
	/* check carrier status */
	nCount++;
	if (nCount == 10 * CHECK_CARRIER_PERIOD)
		nCount = 0;
	if ((nCount % CHECK_CARRIER_PERIOD) != 0)
		return;
	if (GsmModemCheckCarrierRegister() == FALSE)
	{
		GsmActorPublishGsmCarrier("no carrier", NO_SIGNAL, gsmModem->phoneNumber);
		if (oldCarrier != NULL)
			free(oldCarrier);
		return;
	}
	if (GsmModemCheckRssi() != COMMAND_SUCCESS)
	{
		GsmActorPublishGsmCarrier("no carrier", NO_SIGNAL, gsmModem->phoneNumber);
		if (oldCarrier != NULL)
			free(oldCarrier);
		return;
	}
	if (oldCarrier != NULL)
	{
		if ((strcmp(gsmModem->carrier, oldCarrier) != 0) || (oldsignalStrength != gsmModem->signalStrength))
			GsmActorPublishGsmCarrier(gsmModem->carrier, gsmModem->signalStrength, gsmModem->phoneNumber);
	}
	else
		GsmActorPublishGsmCarrier(gsmModem->carrier, gsmModem->signalStrength, gsmModem->phoneNumber);
	if (oldCarrier != NULL)
		free(oldCarrier);
}

VOID GsmModemProcessLoop()
{
	while(1)
	{
		GsmCheckSimCard();
		GsmReportCarrier();
		GsmCheckStatus();
		sleep(1);
	}
}

static VOID GsmModemInitIo()
{
	char command[255];
	printf("Init IO for control gsm\n");
	sprintf(command, "gpio export %d in", GSM_STATUS_PIN);
	system(command);
	sprintf(command, "gpio export %d out", GSM_POWER_PIN);
	system(command);
	wiringPiSetupSys();
	pinMode(GSM_POWER_PIN, OUTPUT);
	pinMode(GSM_STATUS_PIN, INPUT);
	digitalWrite(GSM_POWER_PIN, LOW);
}

BOOL GsmModemPowerOn()
{
	static BYTE nCount;
	if (digitalRead(GSM_STATUS_PIN) == HIGH)
		return TRUE;
	digitalWrite(GSM_POWER_PIN, HIGH);
	sleep(2);
	digitalWrite(GSM_POWER_PIN, LOW);
	sleep(2);
	while (digitalRead(GSM_STATUS_PIN) == LOW)
	{
		sleep(1);
		nCount++;
		if (nCount == SIM_800_BOOT_TIME)
			return FALSE;
	}
	return TRUE;
}

PGSMMODEM GsmGetInfo()
{
	return gsmModem;
}

BOOL GsmModemInit(char* SerialPort, int ttl)
{
	static pthread_t SerialProcessThread;
	static pthread_t SerialOutputThread;
	static pthread_t SerialHandleThread;
	BYTE bCommandState = COMMAND_SUCCESS;
	// turn on gsm module
	GsmModemInitIo();
#ifdef PI_RUNNING
	if (!GsmModemPowerOn())
	{
		LogWrite("Gsm Modem start fail");
		printf("can not power on gsm module\n");
		return FALSE;
	}
#endif
	// open serial port
	char* PortName = malloc(strlen("/dev/") + strlen(SerialPort) + 1);
	memset(PortName, 0, strlen("/dev/") + strlen(SerialPort) + 1);
	sprintf(PortName, "%s%s", "/dev/", SerialPort);
	printf("open port %s\n", PortName);
	// open serial port for communication with gsmModem
	PSERIAL pSerialPort = SerialOpen(PortName, B57600);
	if (pSerialPort == NULL)
	{
		printf("Can not open serial port %s, try another port\n", PortName);
		GsmActorPublishGsmStartedEvent("failure");
		GsmSetRestart(5);
		return FALSE;
	}
	free(PortName);
	printf("port opened\n");
	pthread_create(&SerialProcessThread, NULL, (void*)&SerialProcessIncomingData, (void*)pSerialPort);
	pthread_create(&SerialOutputThread, NULL, (void*)&SerialOutputDataProcess, (void*)pSerialPort);
	pthread_create(&SerialHandleThread, NULL, (void*)&SerialInputDataProcess, (void*)pSerialPort);
	sleep(1);
	// Create gsm device
	gsmModem = malloc(sizeof(GSMMODEM));
	memset(gsmModem, 0, sizeof(GSMMODEM));
	gsmModem->simStatus = TRUE;
	gsmModem->serialPort = pSerialPort;
	atRegisterIncommingProc(GsmModemProcessIncoming, (void*)gsmModem);
	// start up commands
	atSendCommand("AT", gsmModem->serialPort); //send this command to flush all data from buffer
	sleep(1);
	atSendCommand("ATE0", gsmModem->serialPort);
	sleep(1);
	bCommandState += GsmModemExecuteCommand("ATV1");
	bCommandState += GsmModemExecuteCommand("AT+CSCLK=0"); // no sleep
	// Check simcard error
	bCommandState = GsmCheckSimCard();
	bCommandState += GsmModemExecuteCommand("AT+CLIP=1"); // get caller information
	bCommandState += GsmModemExecuteCommand("AT+CMGF=1"); // sms text mode
	bCommandState += GsmModemExecuteCommand("AT+CNMI=3,2,0,1,0");

	sleep(1);
	GsmModemExecuteCommand("AT+CMEE=1");
	GsmModemExecuteCommand("AT+CMGDA=\"DEL ALL\"");

	if (bCommandState != COMMAND_SUCCESS)
	{
		GsmActorPublishGsmStartedEvent("failure");
		LogWrite("Gsm Modem start fail");
		printf("start gsm Modem failed\n");
		GsmSetRestart(5);
		return FALSE;
	}

	GsmGetPhoneNumber();
	//check network status
	GsmActorPublishGsmStartedEvent("success");
	LogWrite("Gsm Modem start succeed");
	sleep(5);
	GsmModemProcessLoop();
	return TRUE;
}

static BOOL GsmModemRestart()
{
	LogWrite("Restart Gsm Modem");
	BYTE bCommandState = COMMAND_SUCCESS;
	// turn on gsm module
#ifdef PI_RUNNING
	if (!GsmModemPowerOn())
	{
		printf("can not power on gsm module\n");
		LogWrite("Gsm Modem restart fail 1");
		GsmSetRestart(5);
		return FALSE;
	}
#endif
	gsmModem->simStatus = TRUE;
	gsmModem->status = GSM_MODEM_ACTIVE;
	if (gsmModem->waitingCommand != NULL)
		free(gsmModem->waitingCommand);
	gsmModem->waitingCommand = NULL;
	// start up commands
	atSendCommand("AT", gsmModem->serialPort); //send this command to flush all data from buffer
	sleep(1);
	atSendCommand("ATE0", gsmModem->serialPort);
	sleep(2);
	bCommandState += GsmModemExecuteCommand("ATV1");
	bCommandState += GsmModemExecuteCommand("AT+CSCLK=0"); // no sleep
	// Check simcard error
	bCommandState = GsmCheckSimCard();
	bCommandState += GsmModemExecuteCommand("AT+CLIP=1"); // get caller information
	bCommandState += GsmModemExecuteCommand("AT+CMGF=1"); // sms text mode
	bCommandState += GsmModemExecuteCommand("AT+CNMI=3,2,0,1,0");
	bCommandState += GsmModemExecuteCommand("AT+CMEE=1");
	sleep(1);
	GsmModemExecuteCommand("AT+CMGDA=\"DEL ALL\"");

	if (bCommandState != COMMAND_SUCCESS)
	{
		GsmActorPublishGsmStartedEvent("failure");
		printf("start gsm Modem failed\n");
		LogWrite("Gsm Modem restart fail 2");
		LogWrite("reset count to restart again");
		GsmSetRestart(5);
		return FALSE;
	}

	GsmGetPhoneNumber();
	//check network status
	GsmActorPublishGsmStartedEvent("success");
	LogWrite("Gsm Modem restart succeed");
//	sleep(5);
//	GsmModemProcessLoop();
	return TRUE;
}

VOID GsmModemDeInit()
{
	SerialClose(gsmModem->serialPort);
	if (gsmModem->commandStatus != NULL)
		free(gsmModem->commandStatus);
	if (gsmModem->waitingCommand != NULL)
		free(gsmModem->waitingCommand);
	if (gsmModem->carrier != NULL)
		free(gsmModem->carrier);
	free(gsmModem);
}

