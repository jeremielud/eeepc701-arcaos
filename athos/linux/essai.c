/*
 * essai.c : premier essai du cœur athos sous Linux (la carte détachée d'ath5k).
 * Initialise l'AR2425, puis écoute les balises (beacons) canal par canal et affiche
 * les réseaux vus : nom, BSSID, canal, signal, chiffrement.
 *
 *   essai [durée par canal en ms] [journal]
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "ath5k.h"
#include "reg.h"
#include "vider.h"

#define RES "/sys/bus/pci/devices/0000:01:00.0/resource0"
#define NRX 32

extern FILE *os_journal;
extern FILE *os_trace;
extern void *os_trace_base;
extern const struct ath_bus_ops athos_pci_bus_ops;
void athos_radio_on(struct ath5k_hw *ah);

static const struct ieee80211_rate rates[] = {
	{ .bitrate = 10, .hw_value = ATH5K_RATE_CODE_1M },
	{ .bitrate = 20, .hw_value = ATH5K_RATE_CODE_2M, .hw_value_short = ATH5K_RATE_CODE_2M | AR5K_SET_SHORT_PREAMBLE, .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 55, .hw_value = ATH5K_RATE_CODE_5_5M, .hw_value_short = ATH5K_RATE_CODE_5_5M | AR5K_SET_SHORT_PREAMBLE, .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 110, .hw_value = ATH5K_RATE_CODE_11M, .hw_value_short = ATH5K_RATE_CODE_11M | AR5K_SET_SHORT_PREAMBLE, .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 60, .hw_value = ATH5K_RATE_CODE_6M },
	{ .bitrate = 90, .hw_value = ATH5K_RATE_CODE_9M },
	{ .bitrate = 120, .hw_value = ATH5K_RATE_CODE_12M },
	{ .bitrate = 180, .hw_value = ATH5K_RATE_CODE_18M },
	{ .bitrate = 240, .hw_value = ATH5K_RATE_CODE_24M },
	{ .bitrate = 360, .hw_value = ATH5K_RATE_CODE_36M },
	{ .bitrate = 480, .hw_value = ATH5K_RATE_CODE_48M },
	{ .bitrate = 540, .hw_value = ATH5K_RATE_CODE_54M },
};

struct rxbuf {
	struct ath5k_desc *desc;
	u32 daddr;          /* adresse physique du descripteur */
	u8 *data;
	u32 paddr;          /* adresse physique des données */
};

static struct rxbuf rx[NRX];

struct reseau {
	u8 bssid[6];
	char ssid[33];
	int canal, rssi, vu;
	const char *secu;
};
static struct reseau reseaux[64];
static int nreseaux;

static void noter_balise(const u8 *f, int len, int rssi, int canal_ecoute)
{
	int pos = 24 + 12, canal = canal_ecoute, prive = (f[24 + 10] & 0x10) != 0;
	const char *secu = prive ? "WEP" : "ouvert";
	char ssid[33] = "";
	struct reseau *r = NULL;

	while (pos + 2 <= len) {
		int id = f[pos], l = f[pos + 1];
		if (pos + 2 + l > len)
			break;
		if (id == 0 && l <= 32) {
			memcpy(ssid, f + pos + 2, l);
			ssid[l] = 0;
		} else if (id == 3 && l == 1) {
			canal = f[pos + 2];
		} else if (id == 48) {
			secu = "WPA2";
		} else if (id == 221 && l >= 4 && !memcmp(f + pos + 2, "\x00\x50\xf2\x01", 4) && strcmp(secu, "WPA2")) {
			secu = "WPA";
		}
		pos += 2 + l;
	}
	for (int i = 0; i < nreseaux; i++)
		if (!memcmp(reseaux[i].bssid, f + 16, 6))
			r = &reseaux[i];
	if (!r && nreseaux < 64) {
		r = &reseaux[nreseaux++];
		memcpy(r->bssid, f + 16, 6);
		r->rssi = -999;
	}
	if (!r)
		return;
	strcpy(r->ssid, ssid);
	r->canal = canal;
	r->secu = secu;
	r->vu++;
	if (rssi > r->rssi)
		r->rssi = rssi;
}

/* Prépare l'anneau de réception (descripteurs liés en boucle, dernier auto-lié). */
static int rx_demarrer(struct ath5k_hw *ah)
{
	u32 *rxlink = NULL;

	for (int i = 0; i < NRX; i++) {
		struct ath5k_desc *ds = rx[i].desc;
		memset(ds, 0, sizeof(*ds));
		ds->ds_link = rx[i].daddr;
		ds->ds_data = rx[i].paddr;
		if (ath5k_hw_setup_rx_desc(ah, ds, ah->common.rx_bufsize, 0))
			return -1;
		if (rxlink)
			*rxlink = rx[i].daddr;
		rxlink = &ds->ds_link;
	}
	ath5k_hw_set_rxdp(ah, rx[0].daddr);
	ath5k_hw_start_rx_dma(ah);
	ath5k_hw_set_opmode(ah, NL80211_IFTYPE_STATION);
	ath5k_hw_set_rx_filter(ah, AR5K_RX_FILTER_UCAST | AR5K_RX_FILTER_BCAST | AR5K_RX_FILTER_MCAST |
				   AR5K_RX_FILTER_BEACON | AR5K_RX_FILTER_PROM);
	ath5k_hw_start_rx_pcu(ah);
	return 0;
}

/* Lit les trames reçues pendant ms millisecondes. */
static int rx_ecouter(struct ath5k_hw *ah, int canal, int ms)
{
	unsigned long fin = os_jiffies() + ms;
	int i = 0, trames = 0, erreurs = 0;

	while (os_jiffies() < fin) {
		struct ath5k_rx_status rs;
		struct ath5k_desc *ds = rx[i].desc;
		int ret;

		memset(&rs, 0, sizeof(rs));
		ret = ah->ah_proc_rx_desc(ah, ds, &rs);
		if (ret == -EINPROGRESS) {
			os_udelay(500);
			continue;
		}
		/* Le dernier descripteur reste auto-lié : on ne le recycle pas avant que le
		 * suivant soit libre (comme ath5k). */
		if (!ret && !rs.rs_status && rs.rs_datalen >= 24) {
			const u8 *f = rx[i].data;
			trames++;
			if ((f[0] & 0xfc) == 0x80 || (f[0] & 0xfc) == 0x50)   /* balise ou réponse de sonde */
				noter_balise(f, rs.rs_datalen - 4, rs.rs_rssi - 95, canal);
		} else {
			erreurs++;
		}
		/* Rendre le descripteur au matériel. */
		memset(&ds->ud, 0, sizeof(ds->ud));
		ath5k_hw_setup_rx_desc(ah, ds, ah->common.rx_bufsize, 0);
		i = (i + 1) % NRX;
	}
	return trames * 1000 + erreurs;
}

int main(int argc, char **argv)
{
	int ms = argc > 1 ? atoi(argv[1]) : 300;
	struct ieee80211_channel canaux[13];
	struct pci_dev pdev = { .vendor = 0x168c, .device = 0x001c };
	struct ath5k_hw *ah;
	void *iobase;
	int fd, ret;

	if (argc > 2)
		os_journal = fopen(argv[2], "w");
	fd = open(RES, O_RDWR | O_SYNC);
	if (fd < 0) { perror(RES); return 1; }
	iobase = mmap(NULL, 0x10000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (iobase == MAP_FAILED) { perror("mmap"); return 1; }

	os_trace = fopen("/root/athos/trace-athos.txt", "w");
	os_trace_base = iobase;
	ah = calloc(1, sizeof(*ah));
	ah->iobase = iobase;
	ah->pdev = &pdev;
	ah->dev = &pdev.dev;
	ah->opmode = NL80211_IFTYPE_STATION;
	ah->bintval = 1000;
	ah->common.ah = ah;
	ah->common.priv = ah;
	ah->common.bus_ops = &athos_pci_bus_ops;
	ah->common.clockrate = 40;
	ah->common.cachelsz = 64;
	memset(ah->common.bssidmask, 0xff, ETH_ALEN);

	ret = ath5k_hw_init(ah);
	os_log("ath5k_hw_init : %d\n", ret);
	if (ret)
		return 2;
	{
		u8 mac[ETH_ALEN];
		if (athos_pci_bus_ops.eeprom_read_mac(ah, mac) == 0)
			memcpy(ah->common.macaddr, mac, ETH_ALEN);
	}
	athos_radio_on(ah);
	os_log("interrupteur radio : GPIO %d, polarité %d\n", ah->ah_capabilities.cap_eeprom.ee_rfkill_pin,
	       ah->ah_capabilities.cap_eeprom.ee_rfkill_pol);
	os_log("MAC srev 0x%x, version %d, radio 0x%x, phy 0x%x\n", ah->ah_mac_srev, ah->ah_version,
	       ah->ah_radio_5ghz_revision, ah->ah_phy_revision);
	os_log("adresse %02x:%02x:%02x:%02x:%02x:%02x, EEPROM version 0x%x, domaine 0x%x\n",
	       ah->common.macaddr[0], ah->common.macaddr[1], ah->common.macaddr[2], ah->common.macaddr[3],
	       ah->common.macaddr[4], ah->common.macaddr[5], ah->ah_capabilities.cap_eeprom.ee_version,
	       ah->ah_capabilities.cap_eeprom.ee_regdomain);

	ah->sbands[NL80211_BAND_2GHZ].band = NL80211_BAND_2GHZ;
	ah->sbands[NL80211_BAND_2GHZ].bitrates = (struct ieee80211_rate *)rates;
	ah->sbands[NL80211_BAND_2GHZ].n_bitrates = ARRAY_SIZE(rates);
	for (int c = 0; c < 13; c++) {
		memset(&canaux[c], 0, sizeof(canaux[c]));
		canaux[c].band = NL80211_BAND_2GHZ;
		canaux[c].center_freq = 2412 + 5 * c;
		canaux[c].hw_value = AR5K_MODE_11G;
		canaux[c].max_power = 20;
	}

	ah->common.rx_bufsize = roundup(IEEE80211_MAX_FRAME_LEN, ah->common.cachelsz);
	for (int i = 0; i < NRX; i++) {
		rx[i].desc = os_dma_alloc(sizeof(struct ath5k_desc), &rx[i].daddr);
		rx[i].data = os_dma_alloc(ah->common.rx_bufsize, &rx[i].paddr);
		if (!rx[i].desc || !rx[i].data) { os_log("mémoire DMA\n"); return 2; }
	}

	int premier = argc > 3 ? atoi(argv[3]) - 1 : 0, dernier = argc > 3 ? atoi(argv[3]) - 1 : 12;
	for (int c = premier; c <= dernier; c++) {
		ath5k_hw_set_imr(ah, 0);
		ath5k_hw_stop_rx_pcu(ah);
		ath5k_hw_dma_stop(ah);
		ah->curchan = &canaux[c];
		ret = ath5k_hw_reset(ah, NL80211_IFTYPE_STATION, &canaux[c], false, false);
		if (ret) {
			os_log("canal %d : reset %d\n", c + 1, ret);
			continue;
		}
		athos_radio_on(ah);
		if (rx_demarrer(ah)) {
			os_log("canal %d : réception impossible\n", c + 1);
			continue;
		}
		ath5k_hw_set_lladdr(ah, ah->common.macaddr);
		if (os_trace)
			fprintf(os_trace, "M ecoute canal %d\n", c + 1);
		ret = rx_ecouter(ah, c + 1, ms);
		if (c == 0 || c == 10) {
			u32 *w = (u32 *)rx[0].desc;
			os_log("  CR %08x RXDP %08x (desc0 %08x) PISR %08x RXCFG %08x RXFILT %08x DIAG %08x\n",
			       ath5k_hw_reg_read(ah, 0x0008), ath5k_hw_reg_read(ah, 0x000c), rx[0].daddr,
			       ath5k_hw_reg_read(ah, AR5K_PISR), ath5k_hw_reg_read(ah, AR5K_RXCFG),
			       ath5k_hw_reg_read(ah, AR5K_RX_FILTER_5211), ath5k_hw_reg_read(ah, AR5K_DIAG_SW_5211));
			{
				unsigned char cfg[8] = { 0 };
				FILE *c = fopen("/sys/bus/pci/devices/0000:01:00.0/config", "rb");
				if (c) { fread(cfg, 1, 8, c); fclose(c); }
				os_log("  PCI COMMAND %02x%02x STATUS %02x%02x\n", cfg[5], cfg[4], cfg[7], cfg[6]);
			}
			if (c == 10) {
				FILE *v = fopen("/root/athos/regs-athos.txt", "w");
				if (v) { athos_vider(ah->iobase, v); fclose(v); }
			}
			os_log("  GPIO CR %08x DO %08x DI %08x\n", ath5k_hw_reg_read(ah, 0x4014), ath5k_hw_reg_read(ah, 0x4018), ath5k_hw_reg_read(ah, 0x401c));
			os_log("  desc0 : %08x %08x %08x %08x %08x %08x  données %08x\n", w[0], w[1], w[2], w[3], w[4], w[5], rx[0].paddr);
		}
		os_log("canal %2d : %d trames, %d erreurs\n", c + 1, ret / 1000, ret % 1000);
	}
	ath5k_hw_stop_rx_pcu(ah);
	ath5k_hw_dma_stop(ah);

	if (os_trace)
		fclose(os_trace);
	os_trace = NULL;
	os_log("\n%d réseaux :\n", nreseaux);
	for (int i = 0; i < nreseaux; i++) {
		struct reseau *r = &reseaux[i];
		os_log("  %-32s  %02x:%02x:%02x:%02x:%02x:%02x  canal %2d  %4d dBm  %-6s  (%d balises)\n",
		       r->ssid[0] ? r->ssid : "(caché)", r->bssid[0], r->bssid[1], r->bssid[2], r->bssid[3],
		       r->bssid[4], r->bssid[5], r->canal, r->rssi, r->secu, r->vu);
	}
	return 0;
}
