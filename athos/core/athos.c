/*
 * athos.c : couche matérielle du pilote athos (AR2425), au-dessus des fichiers
 * matériels d'ath5k. Calquée sur ath5k/base.c (ath5k_init_hw, ath5k_reset,
 * ath5k_rxbuf_setup, ath5k_txbuf_setup), réduite à une station sur 2,4 GHz,
 * sans interruptions (tout se lit par scrutation).
 */
#include "ath5k.h"
#include "reg.h"
#include "athos.h"
#include "ani.h"

extern const struct ath_bus_ops athos_pci_bus_ops;
void athos_radio_on(struct ath5k_hw *ah);

static const struct ieee80211_rate athos_debits[] = {
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

static struct ieee80211_channel athos_canaux[13];
static struct pci_dev athos_pdev = { .vendor = 0x168c, .device = 0x001c };

/* Code matériel d'un débit 802.11 (½ Mbit/s). */
static u16 code_debit(int debit)
{
	for (unsigned i = 0; i < ARRAY_SIZE(athos_debits); i++)
		if (athos_debits[i].bitrate == debit * 5)
			return athos_debits[i].hw_value;
	return ATH5K_RATE_CODE_1M;
}

static int tampon(struct athos_tampon *t, int taille)
{
	t->desc = os_dma_alloc(sizeof(struct ath5k_desc), &t->desc_phys);
	t->data = os_dma_alloc(taille, &t->data_phys);
	return t->desc && t->data ? 0 : -ENOMEM;
}

int athos_attacher(struct athos *a, void *iobase)
{
	struct ath5k_hw *ah;
	struct ath5k_txq_info qi = {
		.tqi_aifs = AR5K_TUNE_AIFS,
		.tqi_cw_min = AR5K_TUNE_CWMIN,
		.tqi_cw_max = AR5K_TUNE_CWMAX,
		.tqi_flags = AR5K_TXQ_FLAG_TXEOLINT_ENABLE | AR5K_TXQ_FLAG_TXDESCINT_ENABLE,
		.tqi_subtype = AR5K_WME_AC_BE,
	};
	u8 mac[ETH_ALEN];
	int ret;

	memset(a, 0, sizeof(*a) - 0);
	ah = kzalloc(sizeof(*ah), GFP_KERNEL);
	if (!ah)
		return -ENOMEM;
	a->ah = ah;
	ah->iobase = iobase;
	ah->pdev = &athos_pdev;
	ah->dev = &athos_pdev.dev;
	ah->opmode = NL80211_IFTYPE_STATION;
	ah->bintval = 1000;
	ah->common.ah = ah;
	ah->common.priv = ah;
	ah->common.bus_ops = &athos_pci_bus_ops;
	ah->common.clockrate = 40;
	ah->common.cachelsz = 64;
	memset(ah->common.bssidmask, 0xff, ETH_ALEN);

	ret = ath5k_hw_init(ah);
	if (ret) {
		os_log("athos : initialisation de la puce impossible (%d)\n", ret);
		return ret;
	}
	if (athos_pci_bus_ops.eeprom_read_mac(ah, mac) == 0)
		memcpy(ah->common.macaddr, mac, ETH_ALEN);
	memcpy(a->mac, ah->common.macaddr, ETH_ALEN);
	ah->ah_ack_bitrate_high = true;
	ah->ah_txpower.txp_requested = 20;   /* dBm, limité ensuite par l'EEPROM */

	ah->sbands[NL80211_BAND_2GHZ].band = NL80211_BAND_2GHZ;
	ah->sbands[NL80211_BAND_2GHZ].bitrates = (struct ieee80211_rate *)athos_debits;
	ah->sbands[NL80211_BAND_2GHZ].n_bitrates = ARRAY_SIZE(athos_debits);
	for (int c = 0; c < 13; c++) {
		memset(&athos_canaux[c], 0, sizeof(athos_canaux[c]));
		athos_canaux[c].band = NL80211_BAND_2GHZ;
		athos_canaux[c].center_freq = 2412 + 5 * c;
		athos_canaux[c].hw_value = AR5K_MODE_11G;
		athos_canaux[c].max_power = 20;
	}

	ah->common.rx_bufsize = roundup(IEEE80211_MAX_FRAME_LEN, ah->common.cachelsz);
	for (int i = 0; i < ATHOS_NRX; i++)
		if (tampon(&a->rx[i], ah->common.rx_bufsize))
			return -ENOMEM;
	for (int i = 0; i < ATHOS_NTX; i++)
		if (tampon(&a->tx[i], IEEE80211_MAX_FRAME_LEN))
			return -ENOMEM;

	/* La file des données, prise en compte à chaque réinitialisation. */
	a->txq = ath5k_hw_setup_tx_queue(ah, AR5K_TX_QUEUE_DATA, &qi);
	if (a->txq < 0) {
		os_log("athos : file d'émission impossible (%d)\n", a->txq);
		return a->txq;
	}
	return 0;
}

/* Réinitialise la puce sur un canal (1 à 13) et relance la réception. */
int athos_canal(struct athos *a, int canal)
{
	struct ath5k_hw *ah = a->ah;
	u32 *lien = NULL;
	int ret;

	if (canal < 1 || canal > 13)
		return -EINVAL;
	ath5k_hw_set_imr(ah, 0);
	ath5k_hw_stop_rx_pcu(ah);
	ath5k_hw_dma_stop(ah);
	for (int i = 0; i < ATHOS_NTX; i++)
		a->tx[i].occupe = 0;
	a->tx_suivant = a->tx_ancien = 0;
	a->tx_lien = NULL;
	a->tx_ancre = -1;

	ah->curchan = &athos_canaux[canal - 1];
	ret = ath5k_hw_reset(ah, NL80211_IFTYPE_STATION, ah->curchan, false, false);
	if (ret) {
		os_log("athos : réinitialisation sur le canal %d impossible (%d)\n", canal, ret);
		return ret;
	}
	athos_radio_on(ah);
	a->canal = canal;
	/* Immunité au bruit automatique et échéances du calibrage (ath5k_reset). */
	ath5k_ani_init(ah, ATH5K_ANI_MODE_AUTO);
	a->cal_court = os_jiffies() + ATH5K_TUNE_CALIBRATION_INTERVAL_SHORT;
	a->cal_complet = os_jiffies() + ATH5K_TUNE_CALIBRATION_INTERVAL_FULL;
	a->cal_ani = os_jiffies() + ATH5K_TUNE_CALIBRATION_INTERVAL_ANI;

	/* Anneau de réception : chaque descripteur pointe sur le suivant, le dernier
	 * sur lui-même (ath5k_rxbuf_setup). */
	for (int i = 0; i < ATHOS_NRX; i++) {
		struct ath5k_desc *ds = a->rx[i].desc;
		memset(ds, 0, sizeof(*ds));
		ds->ds_link = a->rx[i].desc_phys;
		ds->ds_data = a->rx[i].data_phys;
		ath5k_hw_setup_rx_desc(ah, ds, ah->common.rx_bufsize, 0);
		if (lien)
			*lien = a->rx[i].desc_phys;
		lien = &ds->ds_link;
	}
	a->rx_suivant = 0;
	a->rx_dernier = ATHOS_NRX - 1;
	ath5k_hw_set_rxdp(ah, a->rx[0].desc_phys);
	ath5k_hw_start_rx_dma(ah);
	ath5k_hw_set_opmode(ah, NL80211_IFTYPE_STATION);
	ath5k_hw_set_lladdr(ah, a->mac);
	ath5k_hw_set_bssid(ah);
	ath5k_hw_set_rx_filter(ah, AR5K_RX_FILTER_UCAST | AR5K_RX_FILTER_BCAST | AR5K_RX_FILTER_MCAST |
				   AR5K_RX_FILTER_BEACON | AR5K_RX_FILTER_PROM);
	ath5k_hw_start_rx_pcu(ah);
	if (a->irq_actif)
		athos_irq_marche(a);
	return 0;
}

int athos_rx(struct athos *a)
{
	struct ath5k_hw *ah = a->ah;
	int n = 0;

	for (;;) {
		struct athos_tampon *t = &a->rx[a->rx_suivant];
		struct ath5k_rx_status rs;
		int ret;

		/* La puce écrit encore dans ce descripteur (dernier de la chaîne) : on le
		 * laisse (ath5k_tasklet_rx). */
		if (ath5k_hw_get_rxdp(ah) == t->desc_phys)
			break;
		memset(&rs, 0, sizeof(rs));
		ret = ah->ah_proc_rx_desc(ah, t->desc, &rs);
		if (ret == -EINPROGRESS)
			break;
		if (!ret && !rs.rs_status && rs.rs_datalen >= 10 + FCS_LEN) {
			a->rx_trames++;
			wlan_recevoir(a, t->data, rs.rs_datalen - FCS_LEN, rs.rs_rssi - 95);
		} else {
			a->rx_erreurs++;
			if (rs.rs_status & AR5K_RXERR_CRC)
				a->rx_crc++;
			else if (rs.rs_status & AR5K_RXERR_PHY)
				a->rx_phy++;
			else if (rs.rs_status & AR5K_RXERR_FIFO)
				a->rx_fifo++;
			else
				a->rx_autres++;
		}
		/* Le descripteur rendu devient la nouvelle queue de la chaîne : auto-lié, et
		 * l'ancienne queue pointe sur lui (ath5k_rxbuf_setup). */
		memset(&t->desc->ud, 0, sizeof(t->desc->ud));
		t->desc->ds_link = t->desc_phys;
		ath5k_hw_setup_rx_desc(ah, t->desc, ah->common.rx_bufsize, 0);
		a->rx[a->rx_dernier].desc->ds_link = t->desc_phys;
		a->rx_dernier = a->rx_suivant;
		a->rx_suivant = (a->rx_suivant + 1) % ATHOS_NRX;
		n++;
	}
	return n;
}

int athos_tx(struct athos *a, const uint8_t *trame, int longueur, int debit)
{
	struct ath5k_hw *ah = a->ah;
	struct athos_tampon *t = &a->tx[a->tx_suivant];
	struct ath5k_desc *ds = t->desc;
	unsigned flags = AR5K_TXDESC_INTREQ | AR5K_TXDESC_CLRDMASK;
	int ret;

	if (t->occupe || longueur > IEEE80211_MAX_FRAME_LEN - FCS_LEN)
		return -EBUSY;
	memcpy(t->data, trame, longueur);
	if (trame[4] & 1)                       /* destinataire de groupe : pas d'accusé */
		flags |= AR5K_TXDESC_NOACK;
	memset(ds, 0, sizeof(*ds));
	ret = ah->ah_setup_tx_desc(ah, ds, longueur, 24, 0, AR5K_PKT_TYPE_NORMAL,
				   ah->ah_txpower.txp_requested * 2, code_debit(debit), 4,
				   AR5K_TXKEYIX_INVALID, ah->ah_tx_ant, flags, 0, 0);
	if (ret)
		return ret;
	ds->ds_link = 0;
	ds->ds_data = t->data_phys;
	t->occupe = 1;
	/* La file reste accrochée à son dernier descripteur, même terminé : la puce
	 * relit son lien quand on relance la file (ath5k_txbuf_setup). */
	if (a->tx_lien) {
		*a->tx_lien = t->desc_phys;
		if (a->tx_ancre >= 0 && a->tx[a->tx_ancre].occupe == 2)
			a->tx[a->tx_ancre].occupe = 0;
	} else {
		ath5k_hw_set_txdp(ah, a->txq, t->desc_phys);
	}
	a->tx_lien = &ds->ds_link;
	a->tx_ancre = a->tx_suivant;
	ath5k_hw_start_tx_dma(ah, a->txq);
	a->tx_suivant = (a->tx_suivant + 1) % ATHOS_NTX;
	return 0;
}

int athos_tx_fini(struct athos *a)
{
	struct ath5k_hw *ah = a->ah;
	int n = 0;

	for (int k = 0; k < ATHOS_NTX; k++) {
		struct athos_tampon *t = &a->tx[a->tx_ancien];
		struct ath5k_tx_status ts;

		if (t->occupe != 1 && a->tx_ancien == a->tx_suivant)
			break;                  /* rien d'autre en file */
		if (t->occupe == 1) {
			memset(&ts, 0, sizeof(ts));
			if (ah->ah_proc_tx_desc(ah, t->desc, &ts) == -EINPROGRESS)
				break;
			if (ts.ts_status)
				a->tx_echecs++;
			else
				a->tx_trames++;
			/* Le dernier de la file reste réservé comme point d'accroche. */
			t->occupe = (a->tx_ancien == a->tx_ancre) ? 2 : 0;
			n++;
		}
		a->tx_ancien = (a->tx_ancien + 1) % ATHOS_NTX;
	}
	return n;
}

void athos_arreter(struct athos *a)
{
	ath5k_hw_set_imr(a->ah, 0);
	ath5k_hw_stop_rx_pcu(a->ah);
	ath5k_hw_dma_stop(a->ah);
	a->etat = ATHOS_ARRETE;
}

/* --- Interruptions --------------------------------------------------------- */

/* Autorise les interruptions de réception et d'émission. */
void athos_irq_marche(struct athos *a)
{
	a->irq_actif = 1;
	ath5k_hw_set_imr(a->ah, AR5K_INT_RXOK | AR5K_INT_RXERR | AR5K_INT_RXEOL | AR5K_INT_RXORN |
			 AR5K_INT_TXOK | AR5K_INT_TXERR | AR5K_INT_TXDESC | AR5K_INT_TXEOL |
			 AR5K_INT_FATAL | AR5K_INT_GLOBAL);
}

/* À appeler depuis le gestionnaire d'interruption : rend 1 si l'interruption vient de
 * la carte (elle est alors acquittée), 0 sinon (ligne partagée). */
int athos_irq(struct athos *a)
{
	enum ath5k_int etat;

	if (!a->ah || !ath5k_hw_is_intr_pending(a->ah))
		return 0;
	ath5k_hw_get_isr(a->ah, &etat);         /* lit et efface l'état */
	if (etat & AR5K_INT_RXEOL)
		a->irq_rxeol++;
	if (etat & AR5K_INT_RXORN)
		a->irq_rxorn++;
	return 1;
}

/* --- Calibrage périodique (ath5k_calibrate_work, ath5k_tasklet_ani) --------- */
void athos_calibrer(struct athos *a)
{
	struct ath5k_hw *ah = a->ah;
	unsigned long t = os_jiffies();

	if (!ah->curchan)
		return;
	if (t >= a->cal_ani) {
		a->cal_ani = t + ATH5K_TUNE_CALIBRATION_INTERVAL_ANI;
		ah->ah_cal_mask |= AR5K_CALIBRATION_ANI;
		ath5k_ani_calibration(ah);
		ah->ah_cal_mask &= ~AR5K_CALIBRATION_ANI;
	}
	if (t < a->cal_court && t < a->cal_complet)
		return;
	if (t >= a->cal_complet) {
		a->cal_complet = t + ATH5K_TUNE_CALIBRATION_INTERVAL_FULL;
		ah->ah_cal_mask |= AR5K_CALIBRATION_FULL;
		if (ath5k_hw_gainf_calibrate(ah) == AR5K_RFGAIN_NEED_CHANGE)
			os_log("athos : gain radio à revoir (canal %d)\n", a->canal);
	} else {
		ah->ah_cal_mask |= AR5K_CALIBRATION_SHORT;
	}
	a->cal_court = t + ATH5K_TUNE_CALIBRATION_INTERVAL_SHORT;
	if (ath5k_hw_phy_calibrate(ah, ah->curchan))
		os_log("athos : calibrage du canal %d en échec\n", a->canal);
	ah->ah_cal_mask &= ~(AR5K_CALIBRATION_FULL | AR5K_CALIBRATION_SHORT);
}
