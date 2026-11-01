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
	ULONG action, taille, etat[6];
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
