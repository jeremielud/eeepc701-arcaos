/*
 * connecter.c : essai complet du pilote athos sous Linux (carte détachée d'ath5k).
 * Recherche les réseaux, rejoint le réseau ouvert donné, puis envoie une demande
 * DHCP (DISCOVER) et attend l'offre du serveur : si elle arrive, l'émission,
 * l'association et la réception de données fonctionnent.
 *
 *   connecter SSID [journal]
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "osdep.h"
#include "athos.h"

#define RES "/sys/bus/pci/devices/0000:01:00.0/resource0"

extern FILE *os_journal;
static struct athos carte;
static int offre_recue;
static uint32_t xid = 0x41544f53;       /* « ATOS » */

static const char *secu[] = { "ouvert", "WEP", "WPA", "WPA2" };

static uint16_t somme_ip(const uint8_t *p, int n)
{
	uint32_t s = 0;
	for (int i = 0; i < n; i += 2)
		s += (p[i] << 8) | (i + 1 < n ? p[i + 1] : 0);
	while (s >> 16)
		s = (s & 0xffff) + (s >> 16);
	return ~s;
}

static int dhcp_discover(uint8_t *eth, const uint8_t *mac)
{
	uint8_t *ip = eth + 14, *udp = ip + 20, *b = udp + 8;
	int blen = 240, len;

	memset(eth, 0, 600);
	memset(eth, 0xff, 6);
	memcpy(eth + 6, mac, 6);
	eth[12] = 0x08;
	b[0] = 1; b[1] = 1; b[2] = 6;                   /* demande, Ethernet, 6 octets */
	memcpy(b + 4, &xid, 4);
	b[10] = 0x80;                                   /* réponse en diffusion */
	memcpy(b + 28, mac, 6);
	b[236] = 99; b[237] = 130; b[238] = 83; b[239] = 99;
	b[blen++] = 53; b[blen++] = 1; b[blen++] = 1;   /* DHCPDISCOVER */
	b[blen++] = 55; b[blen++] = 3; b[blen++] = 1; b[blen++] = 3; b[blen++] = 6;
	b[blen++] = 255;
	len = 8 + blen;
	udp[1] = 68; udp[3] = 67;
	udp[4] = len >> 8; udp[5] = len & 0xff;
	ip[0] = 0x45; ip[8] = 64; ip[9] = 17;
	ip[2] = (20 + len) >> 8; ip[3] = (20 + len) & 0xff;
	memset(ip + 16, 0xff, 4);
	{
		uint16_t s = somme_ip(ip, 20);
		ip[10] = s >> 8; ip[11] = s & 0xff;
	}
	return 14 + 20 + len;
}

static void recevoir(void *ctx, const uint8_t *eth, int len)
{
	const uint8_t *ip = eth + 14, *udp = ip + 20, *b = udp + 8;
	(void)ctx;
	if (len < 14 + 20 + 8 + 240 || eth[12] != 0x08 || eth[13] != 0 || ip[9] != 17)
		return;
	if (udp[1] != 67 || udp[3] != 68 || memcmp(b + 4, &xid, 4))
		return;
	os_log("DHCP : offre de %d.%d.%d.%d par le serveur %d.%d.%d.%d\n", b[16], b[17], b[18], b[19],
	       ip[12], ip[13], ip[14], ip[15]);
	offre_recue = 1;
}

int main(int argc, char **argv)
{
	uint8_t eth[600];
	void *iobase;
	int fd, n;

	if (argc < 2) { fprintf(stderr, "usage : connecter SSID [journal]\n"); return 1; }
	if (argc > 2)
		os_journal = fopen(argv[2], "w");
	fd = open(RES, O_RDWR | O_SYNC);
	iobase = fd < 0 ? MAP_FAILED : mmap(NULL, 0x10000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (iobase == MAP_FAILED) { perror(RES); return 1; }

	if (athos_attacher(&carte, iobase))
		return 2;
	carte.recevoir = recevoir;
	os_log("carte prête, adresse %02x:%02x:%02x:%02x:%02x:%02x\n", carte.mac[0], carte.mac[1],
	       carte.mac[2], carte.mac[3], carte.mac[4], carte.mac[5]);

	n = wlan_rechercher(&carte, 250);
	os_log("%d réseaux (émis %lu, échecs %lu, reçus %lu) :\n", n, carte.tx_trames, carte.tx_echecs, carte.rx_trames);
	for (int i = 0; i < n; i++)
		os_log("  %-32s canal %2d  %4d dBm  %s\n", carte.reseaux[i].ssid[0] ? carte.reseaux[i].ssid : "(caché)",
		       carte.reseaux[i].canal, carte.reseaux[i].signal, secu[carte.reseaux[i].securite]);

	if (wlan_rejoindre(&carte, argv[1])) {
		os_log("connexion à « %s » impossible (émis %lu, échecs %lu)\n", argv[1], carte.tx_trames, carte.tx_echecs);
		athos_arreter(&carte);
		return 3;
	}
	os_log("associé à « %s », AID %d\n", argv[1], carte.aid);

	for (int essai = 0; essai < 3 && !offre_recue; essai++) {
		int r = wlan_envoyer(&carte, eth, dhcp_discover(eth, carte.mac));
		os_log("DHCPDISCOVER envoyé (%d)\n", r);
		for (int t = 0; t < 2000 && !offre_recue; t++) {
			athos_poll(&carte);
			os_udelay(1000);
		}
	}
	os_log("%s ; émis %lu, échecs %lu, reçus %lu, erreurs %lu\n", offre_recue ? "RÉUSSI" : "pas d'offre DHCP",
	       carte.tx_trames, carte.tx_echecs, carte.rx_trames, carte.rx_erreurs);
	athos_arreter(&carte);
	return offre_recue ? 0 : 4;
}
