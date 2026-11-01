/*
 * athos.h : pilote de l'Atheros AR2425 (AR5007EG) de l'Eee PC 701, indépendant du
 * système. Deux couches :
 *   - athos.c : le matériel (initialisation, canal, réception, émission), au-dessus
 *     des fichiers matériels d'ath5k ;
 *   - wlan.c  : la station 802.11 (recherche, authentification ouverte, association,
 *     trames de données <-> trames Ethernet).
 * Le système fournit les services de osdep.h et appelle athos_poll() régulièrement
 * (ou sur interruption).
 */
#ifndef ATHOS_H
#define ATHOS_H

#include <stdint.h>

#ifndef ATHOS_NRX
#define ATHOS_NRX 32
#endif
#ifndef ATHOS_NTX
#define ATHOS_NTX 16
#endif
#define ATHOS_MAX_RESEAUX 32

struct ath5k_hw;
struct ath5k_desc;

struct athos_tampon {
	struct ath5k_desc *desc;
	uint32_t desc_phys;
	uint8_t *data;
	uint32_t data_phys;
	int occupe;                     /* émission : en attente du compte rendu */
};

/* Réseau vu pendant une recherche. */
struct athos_reseau {
	uint8_t bssid[6];
	char ssid[33];
	int canal;
	int signal;                     /* dBm, approximatif */
	int securite;                   /* ATHOS_SECU_* */
	uint8_t debits[16];             /* débits annoncés (codes 802.11, ½ Mbit/s) */
	int ndebits;
	unsigned long vu;               /* os_jiffies() de la dernière balise */
};

#define ATHOS_SECU_OUVERT 0
#define ATHOS_SECU_WEP    1
#define ATHOS_SECU_WPA    2
#define ATHOS_SECU_WPA2   3

enum athos_etat {
	ATHOS_ARRETE,
	ATHOS_RECHERCHE,
	ATHOS_AUTHENTIFICATION,
	ATHOS_ASSOCIATION,
	ATHOS_ASSOCIE,
};

struct athos {
	struct ath5k_hw *ah;
	uint8_t mac[6];
	int canal;

	struct athos_tampon rx[ATHOS_NRX];
	int rx_suivant;
	struct athos_tampon tx[ATHOS_NTX];
	int tx_suivant, tx_ancien;
	int txq;                        /* file d'émission matérielle des données */
	uint32_t *tx_lien;              /* ds_link du dernier descripteur en file */

	/* Station 802.11 */
	enum athos_etat etat;
	struct athos_reseau reseaux[ATHOS_MAX_RESEAUX];
	int nreseaux;
	struct athos_reseau cible;      /* réseau rejoint ou en cours */
	uint16_t aid;
	uint16_t seq;
	unsigned long echeance;         /* os_jiffies() de la prochaine relance */
	int essais;

	/* Trame Ethernet reçue (le système la récupère par ce rappel). */
	void (*recevoir)(void *ctx, const uint8_t *trame, int longueur);
	void *ctx;

	/* Compteurs */
	unsigned long rx_trames, rx_erreurs, tx_trames, tx_echecs;
};

/* --- athos.c : matériel --------------------------------------------------- */
int athos_attacher(struct athos *a, void *iobase);
int athos_canal(struct athos *a, int canal);
/* Lit les trames reçues ; appelle wlan_recevoir() pour chacune. Rend leur nombre. */
int athos_rx(struct athos *a);
/* Émet une trame 802.11 complète (en-tête compris, sans FCS) au débit donné
 * (en ½ Mbit/s : 2 = 1 Mbit/s). Rend 0 si elle est mise en file. */
int athos_tx(struct athos *a, const uint8_t *trame, int longueur, int debit);
/* Récupère les comptes rendus d'émission ; rend le nombre de trames terminées. */
int athos_tx_fini(struct athos *a);
void athos_arreter(struct athos *a);
void athos_irq_marche(struct athos *a);
int athos_irq(struct athos *a);

/* --- wlan.c : station 802.11 ---------------------------------------------- */
void wlan_recevoir(struct athos *a, const uint8_t *f, int len, int signal);
/* Recherche sur les canaux 1 à 13 (ms par canal) ; remplit a->reseaux. */
int wlan_rechercher(struct athos *a, int ms_par_canal);
/* Rejoint un réseau ouvert (authentification et association). */
int wlan_rejoindre(struct athos *a, const char *ssid);
/* Émet une trame Ethernet (destination, source, type, données). */
int wlan_envoyer(struct athos *a, const uint8_t *trame, int longueur);
/* À appeler régulièrement : réception, relances, comptes rendus. */
void athos_poll(struct athos *a);

#endif
