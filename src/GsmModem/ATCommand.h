/*
 * ATCommand.h
 *
 *  Created on: Jun 30, 2016
 *      Author: ChauNM
 */

#ifndef ATCOMMAND_H_
#define ATCOMMAND_H_
#include "serialcommunication.h"

typedef void(*ATHANDLEFN)(void* param, char* message);

VOID atProcessInputByte(BYTE nData, PWORD index, PBYTE pReceiveBuffer, PSERIAL pSerialPort);
VOID atHandleMessage (PBYTE pData, BYTE nSize);
VOID atSendCommand(char* command, PSERIAL serialPort);
VOID atRegisterIncommingProc(void (*function)(void* pParam, char* message), void* param);
VOID atHandleClipEvent(char* message);
VOID atHandleCmtEvent(char* message);
VOID atHandleCusdEvent(char *message);
char* atHandleCnumEvent(char* message);
#endif /* ATCOMMAND_H_ */
