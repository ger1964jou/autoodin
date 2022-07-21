/*******************************************************************************
** dataServer.c
** UDP packet logger as data logger for OD device
** David Andrew Crawford Beck
** dacb@uw.edu
** Original:
**	Mon Apr 1 10:30:16 PDT 2013
** Modified: Nov 23 2018 Jessica Hardwicke
*******************************************************************************/

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h> /* memset */

#include "packet.h"

#define TIME_SIZE 64

int main(int argc, char *argv[]) {
	// logging variables
	FILE *fp;
	time_t now;
	struct tm* nowlocal;
	unsigned int total_packets = 0, i;
	char *log_file, time_text[TIME_SIZE];
	packet_t p;
	unsigned int port;

	// UDP socket server variables
	int sock_fd, rlen;
	struct sockaddr_in servaddr, cliaddr;
	socklen_t len;

	// argument checking & handling
	if (argc != 3) {
		fprintf(stderr, "usage: %s <UDP port #> <output filename>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	port = atoi(argv[1]);
	log_file = argv[2];
	if ((fp = fopen(log_file, "w")) == NULL) {
		fprintf(stderr, "%s: unable to open output file '%s'\n", argv[0], log_file);
		exit(EXIT_FAILURE);
	}

	// initial setup of logging
	now = time(NULL);
	nowlocal = localtime(&now);
	strftime(time_text, TIME_SIZE, "%m/%d/%y %H:%M:%S %z", nowlocal);
	fprintf(fp, "# %s - logging started at %s on UDP port %d\n", log_file, time_text, port);
	fprintf(fp, "# time\terror_flag\tsequenceID");
	for (i = 0; i < ADCs; ++i) {
		fprintf(fp, "\tADC_%d", i);
	}
	fprintf(fp, "\n");
	fflush(fp);

	// initial setup of UDP server
	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);
	bind(sock_fd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	len = sizeof(cliaddr);

	fprintf(stderr, "UDP server listening on port %d\n", port);

	// main capture loop
	while (1) {
		uint8_t error = 0;
		rlen = recvfrom(sock_fd, &p, sizeof(packet_t), 0, (struct sockaddr *)&cliaddr, &len);
		now = time(NULL);
		nowlocal = localtime(&now);
		++total_packets;
		if (rlen != sizeof(packet_t)) {
			fprintf(stderr, "%s: invalid packet received of size %d\n", argv[0], rlen);
			memset(&p, 0, sizeof(packet_t));
			error = 1;
		}
		// time, error flag, sequence ID, values
		strftime(time_text, TIME_SIZE, "%m/%d/%y %H:%M:%S %z", nowlocal);
		fprintf(fp, "%s\t%d\t%u", time_text, error, p.sequenceID);
		for (i = 0; i < ADCs; ++i)
		fprintf(fp, "\t%d", p.adc[i]);
		fprintf(fp, "\n");
		if (total_packets % 1000 == 0) fprintf(stderr, "%s: at %s - %d total packets received\n", argv[0], time_text, total_packets);
		fflush(fp);
	}

	exit(EXIT_SUCCESS);
}
