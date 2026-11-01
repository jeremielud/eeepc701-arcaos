/*
 * glue.c : ce que les fichiers matériels d'ath5k attendent du reste du pilote Linux
 * (base.c, pci.c, ani.c, et ath/hw.c, ath/regd.c), réduit à l'AR2425 en PCI Express,
 * en station sur 2,4 GHz, réglementation européenne (ETSI).
 *
 * Repris de Linux (drivers/net/wireless/ath/ath5k/pci.c, ath/hw.c), licence ISC.
 */
#include <stdarg.h>
#include <stdio.h>
#include "ath5k.h"
#include "reg.h"
#include "../regd.h"

/* Registres du masque de BSSID (ath/reg.h : AR_BSSMSKL / AR_BSSMSKU). */
#define ATHOS_BSSMSKL 0x80e0
#define ATHOS_BSSMSKU 0x80e4

int ath5k_hw_read_srev(struct ath5k_hw *ah)
{
	ah->ah_mac_srev = ath5k_hw_reg_read(ah, AR5K_SREV);
	return 0;
}

void _ath5k_printk(const struct ath5k_hw *ah, const char *level, const char *fmt, ...)
{
	char line[256];
	va_list args;

	(void)ah;
	va_start(args, fmt);
	vsnprintf(line, sizeof(line), fmt, args);
	va_end(args);
	os_log("%s%s", level, line);
}

/* Compteurs de cycles de la radio, pour l'ANI (ath/hw.c). */
void ath_hw_cycle_counters_update(struct ath_common *common)
{
	struct ath5k_hw *ah = common->ah;
	u32 cycles, busy, rx, tx;

	ath5k_hw_reg_write(ah, 0x00000002, 0x0040);     /* AR_MIBC_FMC : figer */
	cycles = ath5k_hw_reg_read(ah, 0x80f8);         /* AR_CCCNT */
	busy = ath5k_hw_reg_read(ah, 0x80f4);           /* AR_RCCNT */
	rx = ath5k_hw_reg_read(ah, 0x80f0);             /* AR_RFCNT */
	tx = ath5k_hw_reg_read(ah, 0x80ec);             /* AR_TFCNT */
	ath5k_hw_reg_write(ah, 0, 0x80f8);
	ath5k_hw_reg_write(ah, 0, 0x80f0);
	ath5k_hw_reg_write(ah, 0, 0x80f4);
	ath5k_hw_reg_write(ah, 0, 0x80ec);
	ath5k_hw_reg_write(ah, 0, 0x0040);              /* libérer */
	common->cc_ani.cycles += cycles;
	common->cc_ani.rx_busy += busy;
	common->cc_ani.rx_frame += rx;
	common->cc_ani.tx_frame += tx;
}

int32_t ath_hw_get_listen_time(struct ath_common *common)
{
	struct ath_cycle_counters *cc = &common->cc_ani;
	int32_t listen_time = (cc->cycles - cc->rx_frame - cc->tx_frame) / (common->clockrate * 1000);

	memset(cc, 0, sizeof(*cc));
	return listen_time;
}

void ath_hw_setbssidmask(struct ath_common *common)
{
	struct ath5k_hw *ah = common->ah;
	u32 id1;

	ath5k_hw_reg_write(ah, get_unaligned_le32(common->macaddr), AR5K_STA_ID0);
	id1 = ath5k_hw_reg_read(ah, AR5K_STA_ID1) & ~AR5K_STA_ID1_ADDR_U16;
	id1 |= get_unaligned_le16(common->macaddr + 4);
	ath5k_hw_reg_write(ah, id1, AR5K_STA_ID1);
	ath5k_hw_reg_write(ah, get_unaligned_le32(common->bssidmask), ATHOS_BSSMSKL);
	ath5k_hw_reg_write(ah, get_unaligned_le16(common->bssidmask + 4), ATHOS_BSSMSKU);
}

bool ath_is_49ghz_allowed(u16 regdomain)
{
	(void)regdomain;
	return false;
}

/* Limites de puissance d'émission : celles de l'ETSI (Europe). */
u32 ath_regd_get_band_ctl(struct ath_regulatory *reg, enum nl80211_band band)
{
	(void)reg;
	(void)band;
	return CTL_ETSI;
}

/* --- Accès PCI (pci.c) ------------------------------------------------------- */

static void athos_read_cachesize(struct ath_common *common, int *csz)
{
	(void)common;
	*csz = 64 >> 2;                 /* ligne de cache de 64 octets, en mots de 32 bits */
}

static bool athos_eeprom_read(struct ath_common *common, u32 offset, u16 *data)
{
	struct ath5k_hw *ah = common->ah;
	u32 status, timeout;

	ath5k_hw_reg_write(ah, offset, AR5K_EEPROM_BASE);
	AR5K_REG_ENABLE_BITS(ah, AR5K_EEPROM_CMD, AR5K_EEPROM_CMD_READ);
	for (timeout = AR5K_TUNE_REGISTER_TIMEOUT; timeout > 0; timeout--) {
		status = ath5k_hw_reg_read(ah, AR5K_EEPROM_STATUS);
		if (status & AR5K_EEPROM_STAT_RDDONE) {
			if (status & AR5K_EEPROM_STAT_RDERR)
				return false;
			*data = (u16)(ath5k_hw_reg_read(ah, AR5K_EEPROM_DATA) & 0xffff);
			return true;
		}
		usleep_range(15, 20);
	}
	return false;
}

static int athos_eeprom_read_mac(struct ath5k_hw *ah, u8 *mac)
{
	u8 mac_d[ETH_ALEN] = { 0 };
	u32 total, offset;
	u16 data;
	int octet;

	AR5K_EEPROM_READ(0x20, data);
	for (offset = 0x1f, octet = 0, total = 0; offset >= 0x1d; offset--) {
		AR5K_EEPROM_READ(offset, data);
		total += data;
		mac_d[octet + 1] = data & 0xff;
		mac_d[octet] = data >> 8;
		octet += 2;
	}
	if (!total || total == 3 * 0xffff)
		return -EINVAL;
	memcpy(mac, mac_d, ETH_ALEN);
	return 0;
}

const struct ath_bus_ops athos_pci_bus_ops = {
	.ath_bus_type = ATH_PCI,
	.read_cachesize = athos_read_cachesize,
	.eeprom_read = athos_eeprom_read,
	.eeprom_read_mac = athos_eeprom_read_mac,
};

/* --- Interrupteur radio (rfkill.c) ------------------------------------------- */

/* Met la radio en marche : la broche GPIO indiquée par l'EEPROM passe à l'état
 * « non bloqué ». Le pilote ath5k de Linux la laisse « bloquée » quand il rend la
 * carte (ath5k_rfkill_hw_stop). */
void athos_radio_on(struct ath5k_hw *ah)
{
	u8 pin = ah->ah_capabilities.cap_eeprom.ee_rfkill_pin;
	bool pol = ah->ah_capabilities.cap_eeprom.ee_rfkill_pol;

	ath5k_hw_set_gpio_output(ah, pin);
	ath5k_hw_set_gpio(ah, pin, !pol);
	/* Puis la broche repasse en entrée, comme le fait ath5k (ath5k_rfkill_set_intr) :
	 * sur l'Eee PC 701, la laisser en sortie garde la radio coupée. */
	ath5k_hw_set_gpio_input(ah, pin);
	ah->ah_ack_bitrate_high = true;
}
