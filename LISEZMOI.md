# ArcaOS 5.0.7 pour l'Eee PC 701 (SSD interne de 4 Go)

## Fichiers

- `eeepc701-arcaos.img` : image disque brute, 3 898 782 720 octets (une partition
  JFS amorçable C: de 3,6 Go, géométrie 255 têtes × 63 secteurs). Elle tient sur le
  SSD de 4 Go du 701 (~4,0 Go).
- `eeepc701-arcaos.img.gz` : la même, compressée (≈ 530 Mo).
- `eeepc701-arcaos.img.sha256` : empreinte de l'image brute.
- `sauvegarde-apres-installation.img` : état juste après l'installation, avant les
  réglages Eee PC (à supprimer quand l'image sera validée sur la machine).

## Cloner sur l'Eee PC

1. Démarrer le 701 sur une clé USB Linux (Clonezilla, ou n'importe quel système
   « live » : Échap au démarrage pour choisir la clé).
2. Repérer le SSD interne (`lsblk` : le disque d'environ 3,7 à 3,8 Gio, en général
   `/dev/sda` quand on démarre sur la clé — vérifier, la clé apparaît aussi).
3. Copier :
   ```
   zcat eeepc701-arcaos.img.gz | dd of=/dev/sdX bs=4M status=progress conv=fsync
   ```
   (ou `dd if=eeepc701-arcaos.img of=/dev/sdX bs=4M status=progress conv=fsync`).
4. Retirer la clé et redémarrer.

## Ce qui est réglé

| Matériel | Pilote |
|---|---|
| Vidéo Intel GMA 900 | Panorama (VESA), résolution manuelle **800×480** en 65 536 couleurs |
| Son Realtek ALC662 (HDA) | Uniaud (UNIAUD32/UNIAUD16) |
| Ethernet Attansic L2 | MultiMac **MMAE**, TCP/IP en DHCP |
| USB (4 UHCI + 1 EHCI) | clavier, souris, clés USB, **lecteur SD** (4 lettres réservées) |
| ACPI | ACPI.PSD + AcpiDaemon (bouton marche, couvercle…) |
| Touches Fn | **eeekeys** (voir plus bas) |

Système : clavier **français** (FR189), pages de code 850/1004, pays 033, fuseau
France avec heure d'été, machine « EEEPC701 », utilisateur « jeremie ».
Logiciels allégés pour 512 Mo / 4 Go : sans OpenJDK ni Thunderbird (Firefox, Odin32,
Qt4, DOS/Win-OS/2, XWorkplace conservés).

## Touches Fn : eeekeys

Les touches Fn de l'Eee PC ne passent pas par le clavier : le BIOS envoie des
notifications ACPI du périphérique ASUS010 (`\_SB.ATKD`). Rien n'existait sous OS/2 ;
`eeekeys/eeekeys.c` est un petit démon écrit pour cela (kit ACPI d'Arca Noae,
compilé avec OpenWatcom par `eeekeys/construire.sh`). Il démarre avec le bureau
(objet « Touches Fn Eee PC » du dossier Démarrage) et fait :

- Fn+F10 : sourdine ; Fn+F11 / Fn+F12 : volume − / + (mixage général MMPM/2) ;
- Fn+F3 / Fn+F4 : luminosité (le BIOS la règle ; le niveau est réappliqué) ;
- Fn+F9 : liste des fenêtres ;
- Fn+F2 (Wi-Fi) et Fn+F8 (écran externe) restent gérés par le BIOS.

Toute touche non reconnue est notée dans `C:\VAR\LOG\EEEKEYS.LOG` avec son code :
il suffit de l'ajouter dans `eeekeys.c` et de recompiler.

## Wi-Fi

La carte Wi-Fi interne (Atheros AR5007EG / AR2425) **n'a aucun pilote sous ArcaOS**.
Solutions : remplacer la carte mini-PCIe par une Intel 3945ABG ou 4965AGN (gérées par
GenMac, à ajouter ensuite via MPTS), ou brancher un pont Wi-Fi → Ethernet sur la
prise réseau. Les clés USB Wi-Fi ne sont pas gérées.

## Non vérifié (testé seulement en machine virtuelle)

L'installation a été faite et vérifiée dans QEMU, pas sur le 701 lui-même :

- **800×480** : la machine virtuelle n'a pas ce mode ; sur le 701, Panorama doit
  modifier la table des modes du BIOS vidéo Intel. Si l'écran est illisible au premier
  démarrage : au logo, **Alt+F1** puis **V** (affichage VGA), ou Ctrl+Alt+Suppr → C
  (ligne de commande) et `panoutil -r` puis `panoutil -p 800 600`.
- Les **touches Fn** : le démon démarre et parle au pilote ACPI, mais le périphérique
  ASUS010 n'existe qu'en vrai. Regarder `C:\VAR\LOG\EEEKEYS.LOG` après quelques appuis.
- Le **son** (Uniaud doit reconnaître l'ALC662) et le **réseau** (MMAE ne trouve pas
  de carte en machine virtuelle et affiche un avertissement à chaque démarrage ; sur le
  701 la carte est là).

## Refaire ou modifier

- `vm.sh` : machine QEMU (512 Mo, disque IDE de la taille de l'image). ArcaOS
  redémarre en boucle sous KVM sur ce processeur AMD : l'installation a été faite en
  émulation (`ACCEL="-accel tcg,thread=single" CPU=pentium3`), lente mais fiable.
- `disque.sh` (démarrer sur l'image ; `MEDIA=iso` pour garder l'ISO dans le lecteur,
  nécessaire pendant la seconde phase d'installation ; `APPORT=1` pour la disquette).
- `apport/` + `apport.img` : disquette d'apport ; `EEEPC.CMD` applique les réglages
  Eee PC (CONFIG.SYS, objet de démarrage, Panorama).
- Outils de pilotage : `ecran.sh`, `touche.sh`, `souris.py`, `tape.py` (clavier US),
  `tape-fr.py` (clavier français, une fois l'image réglée).
