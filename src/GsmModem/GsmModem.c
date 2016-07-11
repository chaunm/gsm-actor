/*
 * GsmModem.c
 *
 *  Created on: Jun 30, 2016
 *      Author: ChauNM
 */

#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#ifdef PI_RUNNING
#include <wiringPi.h>
#endif
#include "GsmModem.h"
#include "universal.h"
#include "ATCommand.h"
#include "serialcommunication.h"
#include "GsmActor.h"

PGSMMODEM gsmModem = NULL;

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
			GsmActorPublishGsmErrorEvent(eventMessage);
			GsmModemSetStatus(GSM_MODEM_ACTIVE, NULL);
			free(eventMessage);
			return COMMAND_TIMEOUT;
		}
	}
	if (gsmModem->status == GSM_MODEM_CMD_ERROR)
	{
		printf("command %s failed\n", gsmModem->waitingCommand);
		sprintf(eventMessage, "command.%s.error", gsmModem->waitingCommand);
		GsmActorPublishGsmErrorEvent(eventMessage);
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
	}
	else
	{
		char* commandStatus = GsmModemHandleCommandMessage(gsmModem->waitingCommand, message);
		if(commandStatus != NULL)
		{
			GsmModemSetCmdStatus(commandStatus);
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
	//static LONG nCount = CHECK_BILLING_PERIOD - 1;
	//nCount++;
	//if (nCount == (10 * CHECK_BILLING_PERIOD))
		//nCount = 0;
	//if ((nCount % CHECK_BILLING_PERIOD) != 0)
		//return COMMAND_SUCCESS;
	BYTE result;
	char* command = malloc(50);
	memset(command, 50, 0);
	strcpy(command, "ATD");
	strcat(command, "*101#;");
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
			GsmActorPublishGsmErrorEvent(eventMessage);
			GsmModemSetStatus(GSM_MODEM_ACTIVE, NULL);
			free(eventMessage);
			return COMMAND_TIMEOUT;
		}
	}
	if (gsmModem->status == GSM_MODEM_CMD_ERROR)
	{
		printf("send sms failed %s\n", gsmModem->waitingCommand);
		sprintf(eventMessage, "command.%s.error", gsmModem->waitingCommand);
		GsmActorPublishGsmErrorEvent(eventMessage);
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
	memset(command, 50, 0);
	strcpy(command, "ATD");
	strcat(command, number);
	strcat(command, ";");
	result = GsmModemExecuteCommand(command);
	free(command);
	GsmModemCheckBilling();
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
		GsmReportCarrier();
		sleep(1);
	}
}

static VOID GsmModemInitIo()
{
	wiringPiSetupSys();
	pinMode(GSM_POWER_PIN, OUTPUT);
	pinMode(GSM_STATUS_PIN, INPUT);
	digitalWrite(GSM_POWER_PIN, HIGH);
}

BOOL GsmModemPowerOn()
{
	BYTE nRetry = 3;
	while (digitalRead(GSM_STATUS_PIN) == LOW)
	{
		digitalWrite(GSM_POWER_PIN, LOW);
		usleep(1100000);
		digitalWrite(GSM_POWER_PIN, HIGH);
		usleep(2200000);
		if (nRetry > 0)
			nRetry--;
		else
			return (FALSE);
	}
	return TRUE;
}


BOOL GsmModemInit(char* SerialPort, int ttl)
{
	static pthread_t SerialProcessThread;
	static pthread_t SerialOutputThread;
	static pthread_t SerialHandleThread;
	BYTE bCommandState = COMMAND_SUCCESS;
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
	gsmModem->serialPort = pSerialPort;
	atRegisterIncommingProc(GsmModemProcessIncoming, (void*)gsmModem);
	GsmModemInitIo();
#ifdef PI_RUNNIN
	if (!GsmModemPowerOn())
	{
		printf("start gsm modem failed");
		return FALSE;
	}
#endif
	//atSendCommand("AT", gsmModem->serialPort); //send this command to flush all data from buffer
	sleep(2);
	bCommandState += GsmModemExecuteCommand("ATE0"); //turn of echo
	bCommandState += GsmModemExecuteCommand("ATV1"); //turn of echo
	bCommandState += GsmModemExecuteCommand("AT+CSCLK=0"); // no sleep
	bCommandState += GsmModemExecuteCommand("AT+CLIP=1"); // get caller information
	bCommandState += GsmModemExecuteCommand("AT+CMGF=1"); // sms text mode
	bCommandState += GsmModemExecuteCommand("AT+CNMI=3,2,0,1,0");
	bCommandState += GsmModemExecuteCommand("AT+CMGDA=\"DEL ALL\"");

	if (bCommandState != COMMAND_SUCCESS)
	{
		GsmActorPublishGsmStartedEvent("failure");
		printf("start gsm gsmModem failed\n");
		return FALSE;
	}
	GsmGetPhoneNumber();
	//check network status
	GsmActorPublishGsmStartedEvent("success");
	sleep(5);
	GsmModemProcessLoop();
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

