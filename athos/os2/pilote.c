/*
 * pilote.c : pilote réseau NDIS 2.0 pour OS/2 / ArcaOS de l'Atheros AR2425 (Eee PC
 * 701), au-dessus du cœur athos (core/athos.c, core/wlan.c) et du kit Drv32 de David
 * Azarewicz (88watts.net), qui fournit l'interface NDIS (ndis.c) et les services du
 * noyau. Le kit n'est pas distribué avec ces sources : voir LISEZMOI.
 *
 * PROTOCOL.INI :
 *   [ATHOS_nif]
 *     DriverName = ATHOS$
 *     SSID = "Livebox-F550"
 *
 * Le journal du pilote se lit avec athoslog.exe (IOCtl catégorie 0x91).
 */
#include <i86.h>
#include "Dev32lib.h"
#include "Dev32Help.h"
#include "Dev32ndis.h"
#include "osdep.h"
#include "athos.h"

/* Tables NDIS définies par le module ndis.c du kit. */
extern struct CommChar AdapterCC;
extern struct MACSpecChar AdapterSC;
extern struct MACSpecStat AdapterSS;

#define ATHOS_VENDOR 0x168c
#define ATHOS_DEVICE 0x001c
#define NRXIND 8                        /* trames reçues prêtées à la pile réseau */

#define IOCTL_CATEGORIE 0x91
#define IOCTL_JOURNAL   0x01
#define IOCTL_ETAT      0x02

char cDevName[] = "ATHOS$";
static char ssid_voulu[33] = "";

static struct athos carte;
static PCI_DEVICEINFO pci;
static int irq_posee;
static ULONG minuterie_id;

struct indication {
	RxBufOne desc;                  /* décrit la trame pour NdisRcvChain */
	UCHAR *tampon;
	int prete;                      /* tenue par la pile jusqu'à NdisDriverReleaseRx */
};
static struct indication ind[NRXIND];
static UCHAR indiquer;

extern char athos_journal[];
extern unsigned long athos_journal_pos;
extern int athos_journal_ecran;

/* --- Réception : trame Ethernet venue du cœur -> pile réseau ----------------- */
static void recevoir(void *ctx, const uint8_t *eth, int len)
{
	int i;
	ULONG rc;

	(void)ctx;
	for (i = 0; i < NRXIND && ind[i].prete; i++)
		;
	if (i == NRXIND) {
		AdapterSS.MssRFLack++;
		return;
	}
	memcpy(ind[i].tampon, eth, len);
	ind[i].desc.RxDataCount = 1;
	ind[i].desc.RxDataBlk.RxDataLen = len;
	ind[i].desc.RxDataBlk.RxDataPtr = MemFar16Adr(ind[i].tampon);
	AdapterSS.MssFR++;
	AdapterSS.MssFRByt += len;
	if (eth[0] & 1) {
		if (eth[0] == 0xff)
			AdapterSS.MssFRBC++;
		else
			AdapterSS.MssFRMC++;
	}
	indiquer = 0xff;
	ind[i].prete = 1;
	rc = NdisRcvChain(&indiquer, &ind[i].desc, i, len);
	if (rc != WAIT_FOR_RELEASE)
		ind[i].prete = 0;               /* la pile a déjà copié la trame */
}

int __syscall NdisDriverReleaseRx(USHORT pos)
{
	if (pos < NRXIND)
		ind[pos].prete = 0;
	return SUCCESS;
}

/* Lecture par TransferData : non utilisée avec ReceiveChain, mais prévue. */
int __syscall NdisDriverXferRx(USHORT *pcopied, USHORT frameOffset, struct TDBufDesc *pd)
{
	(void)frameOffset;
	(void)pd;
	*pcopied = 0;
	return GENERAL_FAILURE;
}

/* --- Émission : la pile réseau donne une trame Ethernet en morceaux ---------- */
static UCHAR trame_tx[ETH_FRAME_LEN];

int __syscall NdisDriverStartXmit(struct TxBufDesc *pDsc, ULONG ProtID_ReqHandle)
{
	int len = 0, i;

	(void)ProtID_ReqHandle;
	if (pDsc->TxImmedLen) {
		memcpy(trame_tx, pDsc->TxImmedPtr, pDsc->TxImmedLen);
		len = pDsc->TxImmedLen;
	}
	for (i = 0; i < pDsc->TxDataCount && i < MAX_TX_DATABLK; i++) {
		struct TxDataBlock *b = &pDsc->TxDataBlk[i];
		if (len + b->TxDataLen > ETH_FRAME_LEN)
			return INVALID_PARAMETER;
		if (b->TxPtrType == 0) {        /* adresse physique */
			void *lin;
			if (Dev32Help_PhysToLin((ULONG)b->TxDataPtr, b->TxDataLen, &lin))
				return GENERAL_FAILURE;
			memcpy(trame_tx + len, lin, b->TxDataLen);
		} else {                        /* adresse 16:16 */
			memcpy(trame_tx + len, b->TxDataPtr, b->TxDataLen);
		}
		len += b->TxDataLen;
	}
	if (len < ETH_HLEN)
		return INVALID_PARAMETER;
	/* La file d'émission est aussi touchée par la scrutation (interruption) : on
	 * masque les interruptions le temps de la mise en file. */
	_disable();
	i = wlan_envoyer(&carte, trame_tx, len);
	_enable();
	if (i) {
		AdapterSS.MssSFHW++;
		return OUT_OF_RESOURCE;
	}
	AdapterSS.MssFS++;
	AdapterSS.MssFSByt += len;
	return SUCCESS;                         /* trame copiée : pas de TransmitConfirm */
}

/* --- Interruption ----------------------------------------------------------- */
#pragma aux interruption parm [eax]
static int interruption(ULONG donnee)
{
	(void)donnee;
	if (!athos_irq(&carte))
		return 0;                       /* pas pour nous (ligne partagée) */
	_disable();
	Dev32Help_EOI(pci.irq);
	_enable();
	if (!NdisIndicationsTestAndOff()) {
		athos_poll(&carte);
		NdisIndicationsOn();
		NdisIndicationsComplete();
	}
	return 1;
}

/* Minuterie de 31 ms : relances de l'association, réception si une interruption a
 * été perdue, et reconnexion après une désassociation. */
static void __syscall minuterie(ULONG a, ULONG b)
{
	(void)a;
	(void)b;
	if (!NdisIndicationsTestAndOff()) {
		athos_poll(&carte);
		NdisIndicationsOn();
		NdisIndicationsComplete();
	}
	Timer_StartTimerMS(&minuterie_id, 31, minuterie, 0);    /* minuterie à un coup : on la relance */
}

/* --- Appels du module NDIS du kit ----------------------------------------------- */
int __syscall NdisDriverProcessParms(struct ModCfg *pConfig)
{
	struct KeywordEntry *ke;
	char *nom;

	ke = FindKey(pConfig, "DRIVERNAME");
	nom = ke ? GetConfigString(ke) : 0;
	if (!nom || stricmp(nom, cDevName))
		return -1;                      /* section d'un autre pilote */
	memcpy(AdapterCC.CcName, pConfig->ModName, NAME_LEN);
	ke = FindKey(pConfig, "SSID");
	if (ke && (nom = GetConfigString(ke)) != 0)
		strlcpy(ssid_voulu, nom, sizeof(ssid_voulu));
	return 0;
}

/* Liaison avec la pile réseau : on rejoint le réseau configuré. */
int __syscall NdisDriverOpen(void)
{
	static const char *secu[] = { "ouvert", "WEP", "WPA", "WPA2" };
	int essai, n, i;

	if (!ssid_voulu[0]) {
		os_log("athos : aucun SSID dans PROTOCOL.INI\n");
		return 0;
	}
	for (essai = 1; essai <= 4; essai++) {
		n = wlan_rechercher(&carte, 400);
		os_log("athos : recherche %d, %d réseaux :\n", essai, n);
		for (i = 0; i < n; i++)
			os_log("athos :   %s, canal %d, %d dBm, %s\n", carte.reseaux[i].ssid[0] ? carte.reseaux[i].ssid : "(caché)",
			       carte.reseaux[i].canal, carte.reseaux[i].signal, secu[carte.reseaux[i].securite & 3]);
		if (wlan_rejoindre(&carte, ssid_voulu) == 0) {
			os_log("athos : associé à « %s »\n", ssid_voulu);
			return 0;
		}
	}
	os_log("athos : association à « %s » impossible\n", ssid_voulu);
	return 0;
}

int __syscall NdisDriverStartIrq(void)
{
	if (irq_posee)
		return 0;
	if (Dev32Help_SetIRQ((void *)interruption, pci.irq, 1, 0)) {
		os_log("athos : interruption %d indisponible\n", pci.irq);
		return 1;
	}
	irq_posee = 1;
	athos_irq_marche(&carte);
	Timer_StartTimerMS(&minuterie_id, 31, minuterie, 0);
	athos_journal_ecran = 0;
	return 0;
}

void __syscall NdisDriverSetMcast(ULONG Flags)
{
	(void)Flags;                    /* filtre ouvert : tout le multicast passe */
}

void __syscall NdisDriverGetHwStats(void)
{
}

void __syscall NdisDriverSetMac(UCHAR *Mac, ULONG ulLen)
{
	(void)Mac;
	(void)ulLen;                    /* l'adresse de la carte est gardée */
}

/* --- Initialisation --------------------------------------------------------- */
static int trouver_carte(void)
{
	USHORT bdf = PciFindDevice(ATHOS_VENDOR, ATHOS_DEVICE, 0);
	void *iobase;
	int i;

	if (bdf == 0xffff) {
		os_log("athos : carte Atheros %04x:%04x introuvable\n", ATHOS_VENDOR, ATHOS_DEVICE);
		return -1;
	}
	if (PciGetDeviceInfo(bdf, &pci))
		return -1;
	PciSetPowerStateD0(&pci);
	PciSetBusMaster(&pci);
	iobase = MapPhysToLin(pci.bars[0].start, 0x10000);
	if (!iobase) {
		os_log("athos : registres inaccessibles\n");
		return -1;
	}
	os_log("athos : carte %02x:%02x.%d, registres %x, interruption %d\n", PCI_BUS_FROM_BDF(bdf),
	       PCI_DEV_FROM_BDF(bdf), PCI_FUNC_FROM_BDF(bdf), pci.bars[0].start, pci.irq);
	if (athos_attacher(&carte, iobase))
		return -1;
	carte.recevoir = recevoir;
	for (i = 0; i < NRXIND; i++) {
		ind[i].tampon = MemAlloc(ETH_FRAME_LEN);
		if (!ind[i].tampon)
			return -1;
	}
	return 0;
}

static void StrategyInit(REQPACKET *prp)
{
	int rc;

	UtSetDriverName(cDevName);
	TimeInit();
	Timer_InitTimer(2);
	os_log("athos : pilote Wi-Fi AR2425 pour OS/2 (essai)\n");
	rc = NdisInit("ATHOS.MSG");
	if (rc < 0) {
		os_log("athos : section ATHOS$ absente de PROTOCOL.INI (%d)\n", rc);
		goto echec;
	}
	{
		/* Précision des délais du kit (ils servent à régler la radio). */
		ULONG t0 = TimeGetMs();
		os_udelay(200000);
		os_log("athos : étalonnage, 200 ms d'attente = %lu ms\n", TimeGetMs() - t0);
	}
	if (trouver_carte())
		goto echec;
	memcpy(AdapterSC.MscPermStnAdr, carte.mac, 6);
	memcpy(AdapterSC.MscCurrStnAdr, carte.mac, 6);
	AdapterSC.MscLinkSpd = 11000000;
	AdapterSS.MssStatus = HW_OK;
	if (NdisRegisterDriver()) {
		os_log("athos : enregistrement NDIS refusé\n");
		goto echec;
	}
	os_log("athos : prêt, adresse %02x:%02x:%02x:%02x:%02x:%02x, réseau « %s »\n", carte.mac[0],
	       carte.mac[1], carte.mac[2], carte.mac[3], carte.mac[4], carte.mac[5], ssid_voulu);
	prp->usStatus = RPDONE;
	return;
echec:
	prp->usStatus = RPDONE | RPERR | RPERR_GENERAL;
}

static void StrategyIoctl(REQPACKET *prp)
{
	UCHAR *donnees = prp->ioctl.pvData;
	USHORT taille = prp->ioctl.usDataLen;

	if (prp->ioctl.bCategory != IOCTL_CATEGORIE) {
		prp->usStatus |= RPERR | RPERR_BADCOMMAND;
		return;
	}
	switch (prp->ioctl.bFunction) {
	case IOCTL_JOURNAL: {
		/* Les derniers octets du journal ; le premier ULONG donne la position totale. */
		ULONG debut = athos_journal_pos > 16384 ? athos_journal_pos - 16384 : 0, i, n = 0;
		if (taille < 8)
			break;
		*(ULONG *)donnees = athos_journal_pos;
		for (i = debut; i < athos_journal_pos && n < (ULONG)taille - 4; i++, n++)
			donnees[4 + n] = athos_journal[i % 16384];
		break;
	}
	case IOCTL_ETAT:
		if (taille >= sizeof(ULONG) * 6) {
			ULONG *e = (ULONG *)donnees;
			e[0] = carte.etat;
			e[1] = carte.canal;
			e[2] = carte.rx_trames;
			e[3] = carte.tx_trames;
			e[4] = carte.tx_echecs;
			e[5] = carte.rx_erreurs;
		}
		break;
	default:
		prp->usStatus |= RPERR | RPERR_BADCOMMAND;
	}
}

void StrategyHandler(REQPACKET *prp)
{
	prp->usStatus = RPDONE;
	switch (prp->bCommand) {
	case STRATEGY_INIT:
		StrategyInit(prp);
		break;
	case STRATEGY_OPEN:
	case STRATEGY_CLOSE:
	case STRATEGY_DEINSTALL:
	case STRATEGY_INITCOMPLETE:
	case STRATEGY_SAVERESTORE:
		break;
	case STRATEGY_GENIOCTL:
		StrategyIoctl(prp);
		break;
	default:
		prp->usStatus = RPDONE | RPERR | RPERR_GENERAL;
	}
}
