/*
 ============================================================================
 Name        : ZigbeeHost.c
 Author      : ChauNM
 Version     :
 Copyright   :
 Description : C Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "serialcommunication.h"
#include "universal.h"
#include "GsmModem.h"
#include "GsmActor.h"

void PrintHelpMenu() {
	printf("program: ZigbeeHostAMA\n"
			"using ./ZigbeeHostAMA --port [] --id [] --token []\n"
			"--serial: Serial port used to communicate with ZNP device (ex.: ttyUSB0, ttyAMA0..)\n"
			"--id: guid of the znp actor\n"
			"--token: pasword to the broker of the znp actor, this option can be omitted\n"
			"--host: mqtt server address - if omitted will use default is 127.0.0.1\n"
			"--port: mqtt port - if omitted will use port 1883 as default\n"
			"--update: time for updating online message to system");
}

int main(int argc, char* argv[])
{
	PACTOROPTION gsmActorOpt = malloc(sizeof(ACTOROPTION));
	/* get option */
	int opt = 0;
	char *token = NULL;
	char *guid = NULL;
	char *SerialPort = NULL;
	char *mqttHost = NULL;
	WORD mqttPort = 0;
	WORD ttl = 0;

	// specific the expected option
	static struct option long_options[] = {
			{"id",      required_argument, 0, 'i' },
			{"token", 	required_argument, 0, 't' },
			{"serial",  required_argument, 0, 's' },
			{"update", 	required_argument, 0, 'u' },
			{"host", 	required_argument, 0, 'H' },
			{"port", 	required_argument, 0, 'p' }
	};
	int long_index;
	/* Process option */
	while ((opt = getopt_long(argc, argv,":hi:t:s:u:H:p:",
			long_options, &long_index )) != -1) {
		switch (opt) {
		case 'h' :
			PrintHelpMenu();
			return EXIT_SUCCESS;
			break;
		case 's' :
			SerialPort = StrDup(optarg);
			break;
		case 'i':
			guid = StrDup(optarg);
			break;
		case 't':
			token = StrDup(optarg);
			break;
		case 'u':
			ttl = atoi(optarg);
			break;
		case 'H':
			mqttHost = StrDup(optarg);
			break;
		case 'p':
			mqttPort = atoi(optarg);
			break;
		case ':':
			if ((optopt == 'i') || optopt == 'p')
			{
				printf("invalid option(s), using -h for help\n");
				return EXIT_FAILURE;
			}
			break;
		default:
			break;
		}
	}
	if ((SerialPort == NULL) || (guid == NULL))
	{
		printf("invalid options, using -h for help\n");
		return EXIT_FAILURE;
	}
	/* All option valid, start program */

	puts("Program start");
	/* start actor */
	gsmActorOpt->guid = guid;
	gsmActorOpt->psw = token;
	gsmActorOpt->host = mqttHost;
	gsmActorOpt->port = mqttPort;
	GsmActorStart(gsmActorOpt);
	// GsmModem
	if (GsmModemInit(SerialPort, ttl) == FALSE)
		exit(0);
	GsmModemDeInit();
	return EXIT_SUCCESS;
}
