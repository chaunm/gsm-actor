/*
 * GsmActor.h
 *
 *  Created on: Jun 30, 2016
 *      Author: ChauNM
 */

#ifndef GSMACTOR_H_
#define GSMACTOR_H_

#include "actor.h"

void GsmActorPublishSmsReceivedEvent(char* from, char* message);
void GsmActorPublishCallReceivedEvent(char* from);
void GsmActorPublishGsmStartedEvent(char* result);
void GsmActorPublishGsmErrorEvent(char* error);
void GsmActorPublishGsmBillingReport(char* report);
void GsmActorPublishGsmCarrier(char* carrier, BYTE signalStrength);
void GsmActorPublishSignalStrength(BYTE signalStrength);
void GsmActorStart(PACTOROPTION option);

#endif /* GSMACTOR_H_ */
