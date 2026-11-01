/*
 * athoslog.c : affiche l'état et le journal du pilote Wi-Fi athos (ATHOS$) sous
 * OS/2 / ArcaOS, par IOCtl (catégorie 0x91). Sortie sur la console : rediriger vers
 * un fichier au besoin (athoslog > journal.txt).
 */
#define INCL_DOSFILEMGR
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#include <os2.h>
#include <stdio.h>

#define IOCTL_CATEGORIE 0x91
#define IOCTL_JOURNAL   0x01
#define IOCTL_ETAT      0x02

static const char *etats[] = { "arrêté", "recherche", "authentification", "association", "associé" };
static unsigned char tampon[16384 + 4];

int main(void)
{
	HFILE h;
	ULONG action, taille, etat[35];
	APIRET rc;

	rc = DosOpen("ATHOS$", &h, &action, 0, 0, OPEN_ACTION_OPEN_IF_EXISTS,
		     OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY, 0);
	if (rc) {
		printf("athoslog : pilote ATHOS$ absent (code %lu)\n", rc);
		return 1;
	}
	taille = sizeof(etat);
	if (!DosDevIOCtl(h, IOCTL_CATEGORIE, IOCTL_ETAT, 0, 0, 0, etat, sizeof(etat), &taille))
		printf("état : %s, canal %lu, reçues %lu, émises %lu, échecs %lu, erreurs %lu\n",
		       etat[0] < 5 ? etats[etat[0]] : "?", etat[1], etat[2], etat[3], etat[4], etat[5]);
	if (!DosDevIOCtl(h, IOCTL_CATEGORIE, IOCTL_ETAT, 0, 0, 0, etat, sizeof(etat), &taille) && taille >= sizeof(etat))
		printf("pile : remises %lu (gardées %lu, refusées %lu, perdues faute de place %lu) ; à émettre %lu (refusées %lu)\n",
		       etat[6], etat[7], etat[8], etat[9], etat[10], etat[11]);
	if (taille >= sizeof(etat))
		printf("interruptions %lu, tops de minuterie %lu\nerreurs : CRC %lu, PHY %lu, FIFO %lu, autres %lu ; plus de descripteurs %lu, débordements %lu\n",
		       etat[12], etat[13], etat[14], etat[15], etat[16], etat[17], etat[18], etat[19]);
	if (taille >= sizeof(etat)) {
		static const char *debits[12] = { "1", "2", "5,5", "11", "6", "9", "12", "18", "24", "36", "48", "54" };
		int k;
		printf("données pour nous %lu, réémises %lu, doublons %lu\ndébits (Mbit/s) :", etat[20], etat[21], etat[22]);
		for (k = 0; k < 12; k++)
			printf(" %s=%lu", debits[k], etat[23 + k]);
		printf("\n");
	}
	taille = sizeof(tampon);
	rc = DosDevIOCtl(h, IOCTL_CATEGORIE, IOCTL_JOURNAL, 0, 0, 0, tampon, sizeof(tampon), &taille);
	if (rc) {
		printf("athoslog : lecture du journal impossible (code %lu)\n", rc);
	} else {
		ULONG total = *(ULONG *)tampon, n = total < sizeof(tampon) - 4 ? total : sizeof(tampon) - 4;
		printf("--- journal (%lu octets depuis le démarrage) ---\n", total);
		fwrite(tampon + 4, 1, n, stdout);
	}
	DosClose(h);
	return 0;
}
