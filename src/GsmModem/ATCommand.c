/*
 * ATCommand.c
 *
 *  Created on: Jun 30, 2016
 *      Author: ChauNM
 */
#include <string.h>
#include <jansson.h>
#include "universal.h"
#include "GsmModem.h"
#include "GsmActor.h"
#include "ATCommand.h"
#include "serialcommunication.h"
#include "queue.h"

BYTE g_atModemStatus;


ATHANDLEFN atHandleIncomingProc = NULL;
void* atHandleIncomingParam = NULL;

static BOOL atValidateIncomingMessage(PBYTE pData, BYTE nSize)
{
	if ((pData[0] != '\r') || (pData[1] != '\n')
		|| (pData[nSize - 2] != '\r') || (pData[nSize - 1] != '\n'))
		return FALSE;
	return TRUE;
}

VOID atRegisterIncommingProc(void (*function)(void* pParam, char* message), void* param)
{
	atHandleIncomingProc = function;
	atHandleIncomingParam = param;
}

VOID atProcessInputByte(BYTE nData, PWORD index, PBYTE pReceiveBuffer, PSERIAL pSerialPort)
{
	if (((*index == 0) && (nData != '\r'))
		|| ((*index == 1) && (nData != '\n')))
	{
		*index = 0;
		return;
	}
	//get data
	pReceiveBuffer[*index] = nData;
	// check if message finish
	if (((*index) > 3) && (nData == '\n'))
	{
		if (pReceiveBuffer[*index - 1] == '\r')
		{
			// message finished
			QueuePush((void *)pReceiveBuffer, *index + 1, pSerialPort->pInputQueue);
			*index = 0;
			return;
		}
	}
	*index = *index + 1;
	if (*index  == MAX_SERIAL_PACKAGE_SIZE)
		*index = 0;
	return;
}

VOID atHandleMessage(PBYTE pInData, BYTE nSize)
{
	BYTE index;
	PBYTE pData;
	if ((pInData[0] != '\r') && (pInData[1] != '\n' ))
	{
		printf("process message with echoes\n");
		for(index = 1; index <  nSize; index++)
		{
			if((pInData[index] == 0x0A) && (pInData[index - 1] == 0x0D)
				&& (pInData[index + 2] == 0x0A) && (pInData[index + 1] == 0x0D))
			{
				pData = pInData + index + 1;
				nSize = nSize - index - 1;
				break;
			}
		}
	}
	else
		pData = pInData;
	if (atValidateIncomingMessage(pData, nSize) == FALSE)
		return;
	// counting number of at message
	char* messageContent = malloc(nSize - 3);
	BYTE messageCount = 1;
	BYTE messageIndex;
	BYTE messageStart[5];
	BYTE messageStop[5];
	messageStart[0] = 2;
	for (index = 2; index < nSize - 2; index++)
	{
		if((pData[index] == 0x0A) && (pData[index - 1] == 0x0D)
				&& (pData[index - 2] == 0x0A) && (pData[index - 3] == 0x0D))
		{
			messageStop[messageCount - 1] = index - 3;
			messageStart[messageCount] = index + 1;
			messageCount++;
		}
	}
	messageStop[messageCount - 1] = nSize - 2;
	for (messageIndex = 0; messageIndex < messageCount; messageIndex++)
	{
		memset(messageContent, 0, nSize - 3);
		for (index = messageStart[messageIndex]; index < messageStop[messageIndex]; index++)
		{
			messageContent[index - messageStart[messageIndex]] = pData[index];
		}
		if (atHandleIncomingProc != NULL)
			atHandleIncomingProc(atHandleIncomingParam, messageContent);
	}
	free(messageContent);

}

VOID atSendCommand(char* command, PSERIAL serialPort)
{
	char* outCommand = malloc(50);
	strcpy(outCommand, command);
	strcat(outCommand, "\r\n");
	SerialOutput(serialPort, (PBYTE)outCommand, strlen(outCommand));
	free(outCommand);
}

void atHandleClipEvent(char* message)
{
	BYTE index;
	BYTE numberIndex = 0;
	BYTE firstColon = 0xFF;
	if (message == NULL) return;
	char* number = malloc(20);
	memset(number, 0, 20);
	for (index = 0; index < strlen(message); index++)
	{
		if (firstColon == 1)
		{
			if (message[index] != '"')
			{
				number[numberIndex] = message[index];
				numberIndex++;
			}
			else
				break;
		}
		if ((firstColon == 0xFF) && (message[index] == '"'))
			firstColon = 1;
	}
	printf("call from %s\n", number);
	GsmActorPublishCallReceivedEvent(number);
	free(number);
}

VOID atHandleCmtEvent(char* message)
{
	BYTE index;
	BYTE smsStart = 0;
	BYTE numberIndex = 0;
	BYTE nColonCount = 0;
	char* number = malloc(20);
	char* sms = malloc(250);
	memset(number, 0, 20);
	memset(sms, 0, 250);
	for (index = 0; index < strlen(message); index++)
	{
		if ((nColonCount == 1) && (message[index] != '"'))
		{
			number[numberIndex] = message[index];
			numberIndex++;
		}
		if (message[index] == '"')
		{
			nColonCount++;
			if (nColonCount == 6) smsStart = index + 3;
		}
		if ((index >= smsStart)  && (smsStart > 0))
		{
			sms[index - smsStart] = message[index];
		}
	}
	printf("SMS from %s, content:\n%s\n", number, sms);
	GsmActorPublishSmsReceivedEvent(number, sms);
	free(number);
	free(sms);

}

VOID atHandleCusdEvent(char *message)
{
	BYTE index;
	BYTE reportStart = 0;
	char* report = malloc(255);
	memset(report, 0, 255);
	for (index = 0; index < strlen(message); index++)
	{
		if ((reportStart > 0) && (message[index] != '"'))
		{
			report[index - reportStart] = message[index];
		}
		if (message[index] == '"')
		{

			if (reportStart == 0)
				reportStart = index + 1;
			else
				break;
		}
	}
	if (reportStart > 0)
		GsmActorPublishGsmBillingReport(report);
	free(report);
}

char* atHandleCnumEvent(char* message)
{
	BYTE index;
	BYTE markCount = 0;
	BYTE phoneStart = 0;
	if ((message == NULL) || (strstr(message, "+CNUM: ") == 0))
		return NULL;
	char* phoneNumber = malloc(20);
	memset(phoneNumber, 0, 20);
	for (index = 0; index < strlen(message); index++)
	{
		if ((phoneStart > 0) && (message[index] != '"'))
		{
			phoneNumber[index - phoneStart] = message[index];
		}
		if (message[index] == '"')
		{
			markCount++;
			if (markCount == 4)
				break;
		}
		if ((markCount == 3) && (phoneStart == 0))
			phoneStart = index + 1;
	}
	return phoneNumber;
}
