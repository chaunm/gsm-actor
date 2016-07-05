/*
 * GsmActor.h
 *
 *  Created on: Jun 30, 2016
 *      Author: ChauNM
 */

#ifndef GSMACTOR_H_
#define GSMACTOR_H_

#pragma pack(1)
typedef struct tagGSMACTOROPTION {
	char* guid;
	char* psw;
}GSMACTOROPTION, *PGSMACTOROPTION;

void GsmActorPublishSmsReceivedEvent(char* from, char* message);
void GsmActorPublishCallReceivedEvent(char* from);
void GsmActorPublishGsmStartedEvent(char* result);
void GsmActorPublishGsmErrorEvent(char* error);
void GsmActorPublishGsmBillingReport(char* report);
void GsmActorPublishGsmCarrier(char* carrier);
void GsmActorPublishSignalStrength(BYTE signalStrength);
void GsmActorStart(PGSMACTOROPTION option);

#endif /* GSMACTOR_H_ */
