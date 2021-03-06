#define _GNU_SOURCE 1
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <check.h>
#include "../src/lldpd.h"
#include "common.h"

int dump = -1;
char *filename = NULL;
struct pkts_t pkts;
char macaddress[ETH_ALEN] = { 0x5e, 0x10, 0x8e, 0xe7, 0x84, 0xad };
struct lldpd_hardware hardware;
struct lldpd_chassis chassis;

int
pcap_send(struct lldpd *cfg, struct lldpd_hardware *hardware,
    char *buffer, size_t size)
{
	struct pcaprec_hdr hdr;
	struct packet *pkt;
	int n;

	/* Write pcap record header */
	hdr.ts_sec = time(NULL);
	hdr.ts_usec = 0;
	hdr.incl_len = hdr.orig_len = size;
	n = write(dump, &hdr, sizeof(hdr));
	if (n == 1) {
		fail("unable to write pcap record header to %s", filename);
		return -1;
	}

	/* Write data */
	n = write(dump, buffer, size);
	if (n == -1) {
		fail("unable to write pcap data to %s", filename);
		return -1;
	}

	/* Append to list of packets */
	pkt = (struct packet *)malloc(size + sizeof(TAILQ_HEAD(,packet)) + sizeof(int));
	if (!pkt) {
		fail("unable to allocate packet");
		return -1;
	}
	memcpy(pkt->data, buffer, size);
	pkt->size = size;
	TAILQ_INSERT_TAIL(&pkts, pkt, next);
	return 0;
}

struct lldpd_ops pcap_ops = {
	.send = pcap_send,
	.recv = NULL,		/* Won't be used */
	.cleanup = NULL,	/* Won't be used */
};


void
pcap_setup()
{
	static int serial = 0;
	struct pcap_hdr hdr;
	int n;
	/* Prepare packet buffer */
	TAILQ_INIT(&pkts);
	/* Open a new dump file */
	n = asprintf(&filename, "%s_%04d.pcap", filenameprefix, serial++);
	if (n == -1) {
		fail("unable to compute filename");
		return;
	}
	dump = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (dump == -1) {
		fail("unable to open %s", filename);
		return;
	}
	/* Write a PCAP header */
	hdr.magic_number = 0xa1b2c3d4;
	hdr.version_major = 2;
	hdr.version_minor = 4;
	hdr.thiszone = 0;
	hdr.sigfigs = 0;
	hdr.snaplen = 65535;
	hdr.network = 1;
	n = write(dump, &hdr, sizeof(hdr));
	if (n == -1) {
		fail("unable to write pcap header to %s", filename);
		return;
	}
	/* Prepare hardware */
	memset(&hardware, 0, sizeof(struct lldpd_hardware));
	TAILQ_INIT(&hardware.h_rports);
#ifdef ENABLE_DOT1
	TAILQ_INIT(&hardware.h_lport.p_vlans);
#endif
	hardware.h_mtu = 1500;
	hardware.h_ifindex = 4;
	strcpy(hardware.h_ifname, "test");
	memcpy(hardware.h_lladdr, macaddress, ETH_ALEN);
	hardware.h_ops = &pcap_ops;
	/* Prepare chassis */
	memset(&chassis, 0, sizeof(struct lldpd_chassis));
	hardware.h_lport.p_chassis = &chassis;
	chassis.c_ttl = 180;
}

void
pcap_teardown()
{
	struct packet *npkt, *pkt;
	for (pkt = TAILQ_FIRST(&pkts);
	    pkt != NULL;
	    pkt = npkt) {
		npkt = TAILQ_NEXT(pkt, next);
		TAILQ_REMOVE(&pkts, pkt, next);
		free(pkt);
	}
	if (dump != -1) {
		close(dump);
		dump = -1;
	}
	if (filename) {
		free(filename);
		filename = NULL;
	}
}
