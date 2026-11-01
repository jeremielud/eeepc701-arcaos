/*
 * wlan.c : station 802.11 minimale du pilote athos. Recherche active et passive,
 * authentification ouverte, association, et passage des trames de données
 * 802.11 + LLC/SNAP <-> Ethernet II. Pas encore de chiffrement (réseaux ouverts).
 */
#include "ath5k.h"
#include "athos.h"

#define FC_ASSOC_REQ   0x00
#define FC_ASSOC_RESP  0x10
#define FC_PROBE_REQ   0x40
#define FC_PROBE_RESP  0x50
#define FC_BEACON      0x80
#define FC_DISASSOC    0xa0
#define FC_AUTH        0xb0
#define FC_DEAUTH      0xc0
#define FC_DATA        0x08
#define FC_QOS_DATA    0x88

#define DEBIT_GESTION  2        /* 1 Mbit/s : compris de tous les points d'accès */
#define DEBIT_DONNEES  22       /* 11 Mbit/s */
#define DELAI_REPONSE  300      /* ms avant de relancer une demande */
#define ESSAIS         4

static const uint8_t diffusion[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/* En-tête de gestion : type, destinataire, BSSID ; rend sa longueur (24). */
static int entete(struct athos *a, uint8_t *f, uint8_t type, const uint8_t *da, const uint8_t *bssid)
{
	memset(f, 0, 24);
	f[0] = type;
	f[2] = 0x3a;            /* durée : 314 µs (SIFS + accusé à 1 Mbit/s) */
	f[3] = 0x01;
	memcpy(f + 4, da, 6);
	memcpy(f + 10, a->mac, 6);
	memcpy(f + 16, bssid, 6);
	f[22] = (a->seq << 4) & 0xff;
	f[23] = (a->seq >> 4) & 0xff;
	a->seq = (a->seq + 1) & 0xfff;
	return 24;
}

static int ie_ssid(uint8_t *p, const char *ssid)
{
	int l = (int)strlen(ssid);
	p[0] = 0;
	p[1] = l;
	memcpy(p + 2, ssid, l);
	return 2 + l;
}

/* Débits : les 8 premiers dans l'élément 1, le reste dans l'élément 50. */
static int ie_debits(uint8_t *p, const uint8_t *debits, int n)
{
	int k = n > 8 ? 8 : n, len = 0;
	p[len++] = 1;
	p[len++] = k;
	memcpy(p + len, debits, k);
	len += k;
	if (n > 8) {
		p[len++] = 50;
		p[len++] = n - 8;
		memcpy(p + len, debits + 8, n - 8);
		len += n - 8;
	}
	return len;
}

static const uint8_t debits_athos[] = { 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c };

static void sonder(struct athos *a)
{
	uint8_t f[64];
	int len = entete(a, f, FC_PROBE_REQ, diffusion, diffusion);
	len += ie_ssid(f + len, "");
	len += ie_debits(f + len, debits_athos, sizeof(debits_athos));
	athos_tx(a, f, len, DEBIT_GESTION);
}

static void authentifier(struct athos *a)
{
	uint8_t f[32];
	int len = entete(a, f, FC_AUTH, a->cible.bssid, a->cible.bssid);
	memset(f + len, 0, 6);
	f[len + 2] = 1;                 /* algorithme 0 (ouvert), séquence 1, statut 0 */
	athos_tx(a, f, len + 6, DEBIT_GESTION);
	a->etat = ATHOS_AUTHENTIFICATION;
	a->echeance = os_jiffies() + DELAI_REPONSE;
}

static void associer(struct athos *a)
{
	uint8_t f[128];
	int len = entete(a, f, FC_ASSOC_REQ, a->cible.bssid, a->cible.bssid);
	uint16_t capacite = 0x0001 | 0x0020 | 0x0400;   /* ESS, préambule court, intervalle court */

	f[len++] = capacite & 0xff;
	f[len++] = capacite >> 8;
	f[len++] = 10;                  /* intervalle d'écoute : 10 balises */
	f[len++] = 0;
	len += ie_ssid(f + len, a->cible.ssid);
	if (a->cible.ndebits)
		len += ie_debits(f + len, a->cible.debits, a->cible.ndebits);
	else
		len += ie_debits(f + len, debits_athos, sizeof(debits_athos));
	athos_tx(a, f, len, DEBIT_GESTION);
	a->etat = ATHOS_ASSOCIATION;
	a->echeance = os_jiffies() + DELAI_REPONSE;
}

static void noter_reseau(struct athos *a, const uint8_t *f, int len, int signal)
{
	struct athos_reseau r, *dest = NULL;
	int pos = 24 + 12;

	memset(&r, 0, sizeof(r));
	memcpy(r.bssid, f + 16, 6);
	r.canal = a->canal;
	r.signal = signal;
	r.securite = (f[24 + 10] & 0x10) ? ATHOS_SECU_WEP : ATHOS_SECU_OUVERT;
	while (pos + 2 <= len) {
		int id = f[pos], l = f[pos + 1];
		const uint8_t *v = f + pos + 2;
		if (pos + 2 + l > len)
			break;
		if (id == 0 && l <= 32) {
			memcpy(r.ssid, v, l);
			r.ssid[l] = 0;
		} else if ((id == 1 || id == 50) && r.ndebits + l <= (int)sizeof(r.debits)) {
			memcpy(r.debits + r.ndebits, v, l);
			r.ndebits += l;
		} else if (id == 3 && l == 1) {
			r.canal = v[0];
		} else if (id == 48) {
			r.securite = ATHOS_SECU_WPA2;
		} else if (id == 221 && l >= 4 && !memcmp(v, "\x00\x50\xf2\x01", 4) && r.securite != ATHOS_SECU_WPA2) {
			r.securite = ATHOS_SECU_WPA;
		}
		pos += 2 + l;
	}
	/* Une balise reçue hors de son canal (canaux voisins) ne compte pas. */
	if (r.canal != a->canal)
		return;
	r.vu = os_jiffies();
	for (int i = 0; i < a->nreseaux; i++)
		if (!memcmp(a->reseaux[i].bssid, r.bssid, 6))
			dest = &a->reseaux[i];
	if (!dest && a->nreseaux < ATHOS_MAX_RESEAUX)
		dest = &a->reseaux[a->nreseaux++];
	if (!dest)
		return;
	if (dest->vu && dest->signal > r.signal && !r.ssid[0])
		r.signal = dest->signal;
	if (!r.ssid[0] && dest->ssid[0])        /* réseau caché : garder le nom sondé */
		memcpy(r.ssid, dest->ssid, sizeof(r.ssid));
	*dest = r;
}

/* Trame de données reçue -> trame Ethernet II. */
static void donnees(struct athos *a, const uint8_t *f, int len)
{
	int hdr = (f[0] == FC_QOS_DATA) ? 26 : 24;
	uint8_t eth[1514];
	const uint8_t *da = f + 4, *sa = f + 16;
	int charge;

	if ((f[1] & 0x03) != 0x02)              /* de la distribution vers nous seulement */
		return;
	if (memcmp(f + 10, a->cible.bssid, 6))
		return;
	if (len < hdr + 8 || memcmp(f + hdr, "\xaa\xaa\x03\x00\x00\x00", 6))
		return;
	charge = len - hdr - 8;
	if (charge > (int)sizeof(eth) - 14)
		return;
	memcpy(eth, da, 6);
	memcpy(eth + 6, sa, 6);
	memcpy(eth + 12, f + hdr + 6, 2);
	memcpy(eth + 14, f + hdr + 8, charge);
	if (a->recevoir)
		a->recevoir(a->ctx, eth, 14 + charge);
}

void wlan_recevoir(struct athos *a, const uint8_t *f, int len, int signal)
{
	uint8_t type = f[0] & 0xfc;
	int pour_moi = !memcmp(f + 4, a->mac, 6);

	if (len < 24)
		return;
	if (f[1] & 0x40)                        /* trame chiffrée : pas encore gérée */
		return;
	switch (type) {
	case FC_BEACON:
	case FC_PROBE_RESP:
		noter_reseau(a, f, len, signal);
		break;
	case FC_AUTH:
		if (pour_moi && a->etat == ATHOS_AUTHENTIFICATION && !memcmp(f + 10, a->cible.bssid, 6) && len >= 30) {
			int seq = f[26] | f[27] << 8, statut = f[28] | f[29] << 8;
			os_log("athos : authentification, séquence %d, statut %d\n", seq, statut);
			if (seq == 2 && statut == 0) {
				a->essais = 0;
				associer(a);
			}
		}
		break;
	case FC_ASSOC_RESP:
		if (pour_moi && a->etat == ATHOS_ASSOCIATION && len >= 30) {
			int statut = f[26] | f[27] << 8;
			a->aid = (f[28] | f[29] << 8) & 0x3fff;
			os_log("athos : association, statut %d, AID %d\n", statut, a->aid);
			if (statut == 0) {
				a->ah->common.curaid = a->aid;
				ath5k_hw_set_bssid(a->ah);
				a->etat = ATHOS_ASSOCIE;
			}
		}
		break;
	case FC_DEAUTH:
	case FC_DISASSOC:
		if (pour_moi && !memcmp(f + 10, a->cible.bssid, 6) && a->etat >= ATHOS_AUTHENTIFICATION) {
			os_log("athos : %s par le point d'accès (raison %d)\n",
			       type == FC_DEAUTH ? "désauthentifié" : "désassocié", len >= 26 ? f[24] | f[25] << 8 : 0);
			a->essais = 0;
			authentifier(a);
		}
		break;
	case FC_DATA:
	case FC_QOS_DATA:
		if (pour_moi && a->etat == ATHOS_ASSOCIE) {
			static const uint8_t codes[12] = { 0x1b, 0x1a, 0x19, 0x18, 0x0b, 0x0f, 0x0a, 0x0e, 0x09, 0x0d, 0x08, 0x0c };
			uint16_t seq = (f[22] | f[23] << 8) >> 4;
			a->rx_donnees++;
			if (f[1] & 0x08)
				a->rx_reemises++;
			if (seq == a->rx_der_seq)
				a->rx_doublons++;
			a->rx_der_seq = seq;
			for (int k = 0; k < 12; k++)
				if ((a->rx_debit_courant & 0x1b) == codes[k] || a->rx_debit_courant == codes[k])
					a->rx_debit[k]++;
		}
		if (a->etat == ATHOS_ASSOCIE && (pour_moi || (f[4] & 1)))
			donnees(a, f, len);
		break;
	}
}

void athos_poll(struct athos *a)
{
	athos_rx(a);
	athos_tx_fini(a);
	if (a->etat == ATHOS_ASSOCIE)
		athos_calibrer(a);
	if ((a->etat == ATHOS_AUTHENTIFICATION || a->etat == ATHOS_ASSOCIATION) && os_jiffies() >= a->echeance) {
		if (++a->essais > ESSAIS) {
			os_log("athos : pas de réponse du point d'accès\n");
			a->etat = ATHOS_ARRETE;
		} else if (a->etat == ATHOS_AUTHENTIFICATION) {
			authentifier(a);
		} else {
			associer(a);
		}
	}
}

static void attendre(struct athos *a, int ms)
{
	unsigned long fin = os_jiffies() + ms;
	while (os_jiffies() < fin) {
		athos_poll(a);
		os_udelay(1000);
	}
}

int wlan_rechercher(struct athos *a, int ms_par_canal)
{
	a->nreseaux = 0;
	a->etat = ATHOS_RECHERCHE;
	for (int c = 1; c <= 13; c++) {
		if (athos_canal(a, c))
			continue;
		sonder(a);
		attendre(a, ms_par_canal);
	}
	a->etat = ATHOS_ARRETE;
	return a->nreseaux;
}

int wlan_rejoindre(struct athos *a, const char *ssid)
{
	struct athos_reseau *meilleur = NULL;
	unsigned long fin;

	for (int i = 0; i < a->nreseaux; i++)
		if (!strcmp(a->reseaux[i].ssid, ssid) && (!meilleur || a->reseaux[i].signal > meilleur->signal))
			meilleur = &a->reseaux[i];
	if (!meilleur) {
		os_log("athos : réseau « %s » introuvable\n", ssid);
		return -ENOENT;
	}
	if (meilleur->securite != ATHOS_SECU_OUVERT) {
		os_log("athos : « %s » est chiffré, pas encore géré\n", ssid);
		return -EOPNOTSUPP;
	}
	a->cible = *meilleur;
	if (athos_canal(a, a->cible.canal))
		return -EIO;
	memcpy(a->ah->common.curbssid, a->cible.bssid, 6);
	a->ah->common.curaid = 0;
	ath5k_hw_set_bssid(a->ah);
	a->essais = 0;
	authentifier(a);
	fin = os_jiffies() + 5000;
	while (os_jiffies() < fin && a->etat != ATHOS_ASSOCIE && a->etat != ATHOS_ARRETE) {
		athos_poll(a);
		os_udelay(1000);
	}
	return a->etat == ATHOS_ASSOCIE ? 0 : -ETIMEDOUT;
}

int wlan_envoyer(struct athos *a, const uint8_t *eth, int longueur)
{
	uint8_t f[24 + 8 + 1500];
	int charge = longueur - 14;

	if (a->etat != ATHOS_ASSOCIE)
		return -ENOTCONN;
	if (charge < 0 || charge > 1500)
		return -EINVAL;
	memset(f, 0, 24);
	f[0] = FC_DATA;
	f[1] = 0x01;                            /* vers la distribution */
	memcpy(f + 4, a->cible.bssid, 6);
	memcpy(f + 10, a->mac, 6);
	memcpy(f + 16, eth, 6);                 /* destination finale */
	f[22] = (a->seq << 4) & 0xff;
	f[23] = (a->seq >> 4) & 0xff;
	a->seq = (a->seq + 1) & 0xfff;
	memcpy(f + 24, "\xaa\xaa\x03\x00\x00\x00", 6);
	memcpy(f + 30, eth + 12, 2);
	memcpy(f + 32, eth + 14, charge);
	return athos_tx(a, f, 32 + charge, DEBIT_DONNEES);
}
