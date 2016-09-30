/*
 * GsmActor.c
 *
 *  Created on: Jun 7, 2016
 *      Author: ChauNM
 */
#include <string.h>
#include <jansson.h>
#include <pthread.h>
#include <unistd.h>
#include "actor.h"
#include "GsmActor.h"
#include "GsmModem.h"
#include "universal.h"
#include "common/ActorParser.h"

static PACTOR pGsmActor;
static pthread_t gsmActorThread;

void GsmActorPublishSmsReceivedEvent(char* from, char* message)
{
	if (pGsmActor == NULL) return;
	json_t* eventJson = json_object();
	json_t* paramsJson = json_object();
	json_t* fromJson = json_string(from);
	json_t* messageJson = json_string(message);
	json_object_set(paramsJson, "from", fromJson);
	json_object_set(paramsJson, "message", messageJson);
	json_object_set(eventJson, "params", paramsJson);
	char* eventMessage = json_dumps(eventJson, JSON_INDENT(4) | JSON_REAL_PRECISION(4));
	char* topicName = ActorMakeTopicName(pGsmActor->guid, "/:event/sms_received");
	ActorSend(pGsmActor, topicName, eventMessage, NULL, FALSE);
	json_decref(fromJson);
	json_decref(messageJson);
	json_decref(paramsJson);
	json_decref(eventJson);
	free(topicName);
	free(eventMessage);
}

void GsmActorPublishCallReceivedEvent(char* from)
{
	if (pGsmActor == NULL) return;
	json_t* eventJson = json_object();
	json_t* paramsJson = json_object();
	json_t* fromJson = json_string(from);
	json_object_set(paramsJson, "from", fromJson);
	json_object_set(eventJson, "params", paramsJson);
	char* eventMessage = json_dumps(eventJson, JSON_INDENT(4) | JSON_REAL_PRECISION(4));
	char* topicName = ActorMakeTopicName(pGsmActor->guid, "/:event/call_received");
	ActorSend(pGsmActor, topicName, eventMessage, NULL, FALSE);
	json_decref(fromJson);
	json_decref(paramsJson);
	json_decref(eventJson);
	free(topicName);
	free(eventMessage);
}

void GsmActorPublishGsmStartedEvent(char* result)
{
	if (pGsmActor == NULL) return;
	json_t* eventJson = json_object();
	json_t* paramsJson = json_object();
	json_t* resultJson = json_string(result);
	json_object_set(paramsJson, "status", resultJson);
	json_object_set(eventJson, "params", paramsJson);
	char* eventMessage = json_dumps(eventJson, JSON_INDENT(4) | JSON_REAL_PRECISION(4));
	char* topicName = ActorMakeTopicName(pGsmActor->guid, "/:event/gsm_start");
	ActorSend(pGsmActor, topicName, eventMessage, NULL, FALSE);
	json_decref(resultJson);
	json_decref(paramsJson);
	json_decref(eventJson);
	free(topicName);
	free(eventMessage);
}

void GsmActorPublishGsmErrorEvent(char* error, char* message)
{
	if (pGsmActor == NULL) return;
	json_t* eventJson = json_object();
	json_t* paramsJson = json_object();
	json_t* errorJson = NULL;
	json_t* errorMesgJson = NULL;
	char* errorMessage = malloc(250);
	sprintf(errorMessage, "error.%s", error);
	errorJson = json_string(errorMessage);
	json_object_set(paramsJson, "error", errorJson);
	if (message != NULL)
	{
		errorMesgJson = json_string(message);
		json_object_set(paramsJson, "error_message", errorMesgJson);
		json_decref(errorMesgJson);
	}
	json_object_set(eventJson, "params", paramsJson);
	char* eventMessage = json_dumps(eventJson, JSON_INDENT(4) | JSON_REAL_PRECISION(4));
	char* topicName = ActorMakeTopicName(pGsmActor->guid, "/:event/gsm_error");
	ActorSend(pGsmActor, topicName, eventMessage, NULL, FALSE);
	free(errorMessage);
	json_decref(errorJson);
	json_decref(paramsJson);
	json_decref(eventJson);
	free(topicName);
	free(eventMessage);
}

void GsmActorPublishGsmBillingReport(char* report)
{
	if (pGsmActor == NULL) return;
	json_t* eventJson = json_object();
	json_t* paramsJson = json_object();
	json_t* reportJson = json_string(report);
	json_object_set(paramsJson, "report", reportJson);
	json_object_set(eventJson, "params", paramsJson);
	char* eventMessage = json_dumps(eventJson, JSON_INDENT(4) | JSON_REAL_PRECISION(4));
	char* topicName = ActorMakeTopicName(pGsmActor->guid, "/:event/billing_report");
	ActorSend(pGsmActor, topicName, eventMessage, NULL, FALSE);
	json_decref(reportJson);
	json_decref(paramsJson);
	json_decref(eventJson);
	free(topicName);
	free(eventMessage);
}

void GsmActorPublishGsmCarrier(char* carrier, BYTE signalStrength, char* number)
{
	if (pGsmActor == NULL) return;
	json_t* eventJson = json_object();
	json_t* paramsJson = json_object();
	json_t* carrierJson = json_string(carrier);
	json_t* numberJson;
	if (number != NULL)
		numberJson = json_string(number);
	else
		numberJson = json_string("can_not_get_number");
	char* signalReport = malloc(50);
	memset(signalReport, 0, 50);
	switch(signalStrength)
	{
	case NO_SIGNAL:
		signalReport = StrDup("status.no_signal");
		break;
	case SIGNAL_POOR:
		signalReport = StrDup("status.poor");
		break;
	case SIGNAL_FAIR:
		signalReport = StrDup("status.fair");
		break;
	case SIGNAL_GOOD:
		signalReport = StrDup("status.good");
		break;
	case SIGNAL_EXCELLENT:
		signalReport = StrDup("status.excellent");
		break;
	default:
		signalReport = StrDup("status.unknown");
		break;
	}
	json_t* rssiJson = json_string(signalReport);
	json_object_set(paramsJson, "carrier", carrierJson);
	json_object_set(paramsJson, "number", numberJson);
	json_object_set(paramsJson, "rssi", rssiJson);
	json_object_set(eventJson, "params", paramsJson);
	char* eventMessage = json_dumps(eventJson, JSON_INDENT(4) | JSON_REAL_PRECISION(4));
	char* topicName = ActorMakeTopicName(pGsmActor->guid, "/:event/carrier_report");
	ActorSend(pGsmActor, topicName, eventMessage, NULL, FALSE);
	json_decref(carrierJson);
	json_decref(numberJson);
	json_decref(rssiJson);
	json_decref(paramsJson);
	json_decref(eventJson);
	free(signalReport);
	free(topicName);
	free(eventMessage);
}

void GsmActorPublishSignalStrength(BYTE signalStrength)
{
	if (pGsmActor == NULL) return;
	json_t* eventJson = json_object();
	json_t* paramsJson = json_object();
	json_t* reportJson = NULL;
	char* signalReport = malloc(50);
	memset(signalReport, 0, 50);
	switch(signalStrength)
	{
	case NO_SIGNAL:
		signalReport = StrDup("status.no_signal");
		break;
	case SIGNAL_POOR:
		signalReport = StrDup("status.poor");
		break;
	case SIGNAL_FAIR:
		signalReport = StrDup("status.fair");
		break;
	case SIGNAL_GOOD:
		signalReport = StrDup("status.good");
		break;
	case SIGNAL_EXCELLENT:
		signalReport = StrDup("status.excellent");
		break;
	default:
		signalReport = StrDup("status.unknown");
		break;
	}
	reportJson = json_string(signalReport);
	json_object_set(paramsJson, "report", reportJson);
	json_object_set(eventJson, "params", paramsJson);
	char* eventMessage = json_dumps(eventJson, JSON_INDENT(4) | JSON_REAL_PRECISION(4));
	char* topicName = ActorMakeTopicName(pGsmActor->guid, "/:event/rssi_report");
	ActorSend(pGsmActor, topicName, eventMessage, NULL, FALSE);
	free(signalReport);
	json_decref(reportJson);
	json_decref(paramsJson);
	json_decref(eventJson);
	free(topicName);
	free(eventMessage);
}

void GsmActorPublishPhoneNumber(char* number)
{
	if (pGsmActor == NULL) return;
	json_t* eventJson = json_object();
	json_t* paramsJson = json_object();
	json_t* numberJson;
	if (number != NULL)
		numberJson = json_string(number);
	else
		numberJson = json_string("can_not_get_number");
	json_object_set(paramsJson, "number", numberJson);
	json_object_set(eventJson, "params", paramsJson);
	char* eventMessage = json_dumps(eventJson, JSON_INDENT(4) | JSON_REAL_PRECISION(4));
	char* topicName = ActorMakeTopicName(pGsmActor->guid, "/:event/number_report");
	ActorSend(pGsmActor, topicName, eventMessage, NULL, FALSE);
	json_decref(numberJson);
	json_decref(paramsJson);
	json_decref(eventJson);
	free(topicName);
	free(eventMessage);
}

static void GsmActorOnRequestSendSms(PVOID pParam)
{
	char* message = (char*)pParam;
	char **znpSplitMessage;
	if (pGsmActor == NULL) return;
	json_t* payloadJson = NULL;
	json_t* paramsJson = NULL;
	json_t* numberJson = NULL;
	json_t* smsJson = NULL;
	json_t* responseJson = NULL;
	json_t* statusJson = NULL;
	PACTORHEADER header;
	const char* number;
	const char* sms;
	char* responseTopic;
	char* responseMessage;
	znpSplitMessage = ActorSplitMessage(message);
	if (znpSplitMessage == NULL)
		return;
	// parse header to get origin of message
	header = ActorParseHeader(znpSplitMessage[0]);
	if (header == NULL)
	{
		ActorFreeSplitMessage(znpSplitMessage);
		return;
	}
	//parse payload
	payloadJson = json_loads(znpSplitMessage[1], JSON_DECODE_ANY, NULL);
	if (payloadJson == NULL)
	{
		ActorFreeSplitMessage(znpSplitMessage);
		ActorFreeHeaderStruct(header);
		return;
	}
	paramsJson = json_object_get(payloadJson, "params");
	if (paramsJson == NULL)
	{
		json_decref(payloadJson);
		ActorFreeSplitMessage(znpSplitMessage);
		ActorFreeHeaderStruct(header);
		return;
	}
	numberJson = json_object_get(paramsJson, "number");
	if (numberJson == NULL)
	{
		json_decref(paramsJson);
		json_decref(payloadJson);
		ActorFreeSplitMessage(znpSplitMessage);
		ActorFreeHeaderStruct(header);
		return;
	}
	smsJson = json_object_get(paramsJson, "message");
	if (smsJson == NULL)
	{
		json_decref(numberJson);
		json_decref(paramsJson);
		json_decref(payloadJson);
		ActorFreeSplitMessage(znpSplitMessage);
		ActorFreeHeaderStruct(header);
		return;
	}
	number = json_string_value(numberJson);
	sms = json_string_value(smsJson);
	int result = GsmModemSendSms(number, sms);
	json_decref(numberJson);
	json_decref(smsJson);
	json_decref(paramsJson);
	json_decref(payloadJson);
	//make response package
	responseJson = json_object();
	statusJson = json_object();
	json_t* requestJson = json_loads(znpSplitMessage[1], JSON_DECODE_ANY, NULL);
	json_object_set(responseJson, "request", requestJson);
	json_decref(requestJson);
	json_t* resultJson = NULL;
	json_t* receivedJson = NULL;
	switch(result)
	{
	case COMMAND_TIMEOUT:
		resultJson = json_string("status.error.command_timeout");
		receivedJson = json_string("status.not_delivered");
		break;
	case COMMAND_ERROR:
		resultJson = json_string("status.error.command_failure");
		receivedJson = json_string("status.not_delivered");
		break;
	case COMMAND_SUCCESS:
		resultJson = json_string("status.success");
		receivedJson = json_string("status.delivered");
		break;
	}
	json_object_set(statusJson, "status", resultJson);
	json_object_set(statusJson, "received", receivedJson);
	json_decref(resultJson);
	json_decref(receivedJson);
	json_object_set(responseJson, "response", statusJson);
	json_decref(statusJson);
	responseMessage = json_dumps(responseJson, JSON_INDENT(4) | JSON_REAL_PRECISION(4));
	responseTopic = ActorMakeTopicName(header->origin, "/:response");
	ActorFreeHeaderStruct(header);
	json_decref(responseJson);
	ActorFreeSplitMessage(znpSplitMessage);
	ActorSend(pGsmActor, responseTopic, responseMessage, NULL, FALSE);
	free(responseMessage);
	free(responseTopic);
}

static void GsmActorOnRequestMakeCall(PVOID pParam)
{
	char* message = (char*)pParam;
	char **znpSplitMessage;
	if (pGsmActor == NULL) return;
	json_t* payloadJson = NULL;
	json_t* paramsJson = NULL;
	json_t* numberJson = NULL;
	json_t* responseJson = NULL;
	json_t* statusJson = NULL;
	PACTORHEADER header;
	const char* number;
	char* responseTopic;
	char* responseMessage;
	znpSplitMessage = ActorSplitMessage(message);
	if (znpSplitMessage == NULL)
		return;
	// parse header to get origin of message
	header = ActorParseHeader(znpSplitMessage[0]);
	if (header == NULL)
	{
		ActorFreeSplitMessage(znpSplitMessage);
		return;
	}
	//parse payload
	payloadJson = json_loads(znpSplitMessage[1], JSON_DECODE_ANY, NULL);
	if (payloadJson == NULL)
	{
		ActorFreeSplitMessage(znpSplitMessage);
		ActorFreeHeaderStruct(header);
		return;
	}
	paramsJson = json_object_get(payloadJson, "params");
	if (paramsJson == NULL)
	{
		json_decref(payloadJson);
		ActorFreeSplitMessage(znpSplitMessage);
		ActorFreeHeaderStruct(header);
		return;
	}
	numberJson = json_object_get(paramsJson, "number");
	if (numberJson == NULL)
	{
		json_decref(paramsJson);
		json_decref(payloadJson);
		ActorFreeSplitMessage(znpSplitMessage);
		ActorFreeHeaderStruct(header);
		return;
	}
	number = json_string_value(numberJson);
	int result = GsmModemMakeCall(number);
	json_decref(numberJson);
	json_decref(paramsJson);
	json_decref(payloadJson);
	//make response package
	responseJson = json_object();
	statusJson = json_object();
	json_t* requestJson = json_loads(znpSplitMessage[1], JSON_DECODE_ANY, NULL);
	json_object_set(responseJson, "request", requestJson);
	json_decref(requestJson);
	json_t* resultJson = NULL;
	switch(result)
	{
	case COMMAND_TIMEOUT:
		resultJson = json_string("status.error.command_timeout");
		break;
	case COMMAND_ERROR:
		resultJson = json_string("status.error.command_failure");
		break;
	case COMMAND_SUCCESS:
		resultJson = json_string("status.success");
		break;
	}
	json_object_set(statusJson, "status", resultJson);
	json_decref(resultJson);
	json_object_set(responseJson, "response", statusJson);
	json_decref(statusJson);
	responseMessage = json_dumps(responseJson, JSON_INDENT(4) | JSON_REAL_PRECISION(4));
	responseTopic = ActorMakeTopicName(header->origin, "/:response");
	ActorFreeHeaderStruct(header);
	json_decref(responseJson);
	ActorFreeSplitMessage(znpSplitMessage);
	ActorSend(pGsmActor, responseTopic, responseMessage, NULL, FALSE);
	free(responseMessage);
	free(responseTopic);
}

static void GsmActorOnHiRequest(PVOID pParam)
{
	PGSMMODEM gsmInfo = GsmGetInfo();
	char* message = (char*) pParam;
	char **znpSplitMessage;
	if (pGsmActor == NULL) return;
	json_t* responseJson = NULL;
	json_t* statusJson = NULL;
	PACTORHEADER header;
	char* responseTopic;
	char* responseMessage;
	znpSplitMessage = ActorSplitMessage(message);
	if (znpSplitMessage == NULL)
		return;
	// parse header to get origin of message
	header = ActorParseHeader(znpSplitMessage[0]);
	if (header == NULL)
	{
		ActorFreeSplitMessage(znpSplitMessage);
		return;
	}
	//make response package
	responseJson = json_object();
	statusJson = json_object();
	json_t* requestJson = json_loads(znpSplitMessage[1], JSON_DECODE_ANY, NULL);
	json_object_set(responseJson, "request", requestJson);
	json_decref(requestJson);
	json_t* resultJson = json_string("status.success");
	json_object_set(statusJson, "status", resultJson);
	json_decref(resultJson);
	json_t* stateJson;
	json_t* networkJson;
	if (gsmInfo->carrier != NULL)
	{
		stateJson = json_string("connected");
		networkJson = json_string(gsmInfo->carrier);
	}
	else
	{
		stateJson = json_string("disconnected");
		networkJson = json_string("no network");
	}
	json_object_set(statusJson, "state", stateJson);
	json_object_set(statusJson, "network", networkJson);
	json_decref(stateJson);
	json_decref(networkJson);
	json_t* signalJson = NULL;
	switch (gsmInfo->signalStrength)
	{
	case NO_SIGNAL:
		signalJson = json_string("status.no_signal");
		break;
	case SIGNAL_POOR:
		signalJson = json_string("status.poor");
		break;
	case SIGNAL_FAIR:
		signalJson = json_string("status.fair");
		break;
	case SIGNAL_GOOD:
		signalJson = json_string("status.good");
		break;
	case SIGNAL_EXCELLENT:
		signalJson = json_string("status.excellent");
		break;
	default:
		signalJson = json_string("status.unknown");
		break;
	}
	json_object_set(statusJson, "signal", signalJson);
	json_decref(signalJson);
	json_t* numberJson;
	if (gsmInfo->phoneNumber != NULL)
		numberJson = json_string(gsmInfo->phoneNumber);
	else
		numberJson = json_string("unknown");
	json_object_set(statusJson, "number", numberJson);
	json_decref(numberJson);
	json_t* ballanceJson = json_string("unknown");
	json_object_set(statusJson, "ballance", ballanceJson);
	json_decref(ballanceJson);

	json_object_set(responseJson, "response", statusJson);
	json_decref(statusJson);
	responseMessage = json_dumps(responseJson, JSON_INDENT(4) | JSON_REAL_PRECISION(4));
	responseTopic = ActorMakeTopicName(header->origin, "/:response");
	ActorFreeHeaderStruct(header);
	json_decref(responseJson);
	ActorFreeSplitMessage(znpSplitMessage);
	ActorSend(pGsmActor, responseTopic, responseMessage, NULL, FALSE);
	free(responseMessage);
	free(responseTopic);
}
static void GsmActorCreate(char* guid, char* psw, char* host, WORD port)
{
	pGsmActor = ActorCreate(guid, psw, host, port);
	//Register callback to handle request package
	if (pGsmActor == NULL)
	{
		printf("Couldn't create actor\n");
		return;
	}
	ActorRegisterCallback(pGsmActor, ":request/send_sms", GsmActorOnRequestSendSms, CALLBACK_RETAIN);
	ActorRegisterCallback(pGsmActor, ":request/make_call", GsmActorOnRequestMakeCall, CALLBACK_RETAIN);
	ActorRegisterCallback(pGsmActor, ":request/Hi", GsmActorOnHiRequest, CALLBACK_RETAIN);
}

static void ZnpActorProcess(PACTOROPTION option)
{
	mosquitto_lib_init();
	GsmActorCreate(option->guid, option->psw, option->host, option->port);
	if (pGsmActor == NULL)
	{
		mosquitto_lib_cleanup();
		return;
	}
	while(1)
	{
		ActorProcessEvent(pGsmActor);
		mosquitto_loop(pGsmActor->client, 0, 1);
		usleep(10000);
	}
	mosquitto_disconnect(pGsmActor->client);
	mosquitto_destroy(pGsmActor->client);
	mosquitto_lib_cleanup();
}

void GsmActorStart(PACTOROPTION option)
{
	pthread_create(&gsmActorThread, NULL, (void*)&ZnpActorProcess, (void*)option);
	pthread_detach(gsmActorThread);
}
