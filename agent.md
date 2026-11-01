# agent.md — journal de travail et guide pour les agents

Projet : **ArcaOS 5.0.7 sur l'Asus Eee PC 701**, avec un pilote Wi-Fi OS/2 écrit pour
l'Atheros AR2425 (« athos »). Dépôt public : https://github.com/jeremielud/eeepc701-arcaos
Ce fichier dit ce qui a été fait, comment c'est organisé, comment travailler, et les
pièges connus. Le tenir à jour à chaque étape.

## Le matériel

- Eee PC 701 : Celeron M 900 MHz (i686, pas de 64 bits), 512 Mo, SSD SiliconMotion de
  4 001 292 288 octets (IDE), écran 800×480.
- Wi-Fi : Atheros AR2425 (AR5007EG), PCI 168c:001c, carte AzureWave AW-GE780
  [1a3b:1026], BAR0 64 Ko, interruption 18 sous ArcaOS. Révisions : MAC 0xe2, PHY 0x70,
  radio 0xa2, EEPROM 0x5003, domaine 0x60. Interrupteur radio = GPIO 0, polarité 0.
- Ethernet : Attansic L2 (pilote MultiMac MMAE). Pas de câble chez l'utilisateur.
- Réseau de l'utilisateur : Livebox « Livebox-F550 », **ouverte**, canal 11 ; le 701
  obtient 192.168.1.11 (même adresse sous Linux et sous ArcaOS).

## Ce qui est fait

1. **Image ArcaOS** (`eeepc701-arcaos.img`, non publiée : licence nominative) installée
   en QEMU puis écrite sur le SSD du 701 (vérifiée par sha256). Vidéo Panorama 800×480,
   son Uniaud, luminosité Fn vérifiés sur la machine.
2. **eeekeys** (`eeekeys/`) : démon OS/2 des touches Fn (notifications ACPI ASUS010).
3. **Clé USB d'accès** (`cle/`) : Alpine Linux x86 ; se connecte seule au Wi-Fi, ouvre SSH
   (clé `~/.ssh/eeepc_ed25519`, alias `ssh eeepc`), menu d'installation d'ArcaOS.
   Partition 1 FAT32 (Linux, chargeur extlinux), partition 2 **FAT16 « ATHOS »**
   (256 Mo) lisible par ArcaOS, qui sert d'échange avec ArcaOS.
4. **Pilote athos** (`athos/`) :
   - cœur indépendant du système = fichiers matériels d'ath5k (licence ISC) +
     `compat/lxcompat.h` (API Linux) + `core/athos.c` (matériel) + `core/wlan.c`
     (station 802.11 : recherche, authentification ouverte, association, données) ;
   - mis au point **sous Linux en espace utilisateur** sur le 701 (`athos/linux/`) :
     DHCP OK ;
   - **pilote OS/2 NDIS** `athos.os2` (`athos/os2/`, kit Drv32 de David Azarewicz) :
     **dans ArcaOS, association, DHCP (192.168.1.11), ping de la box et d'Internet,
     DNS, HTTP fonctionnent** (commit 217ba67).
5. **Banc d'essai automatique** : le PC pilote des allers-retours Linux ⇄ ArcaOS sans
   intervention (voir « Travailler »).

## ⏸ Projet en pause (2026-11-01)

L'utilisateur a mis ArcaOS en pause pour installer ReactOS à la place sur le 701 (voir
le projet ReactOS). État au moment de la pause : pilote athos fonctionnel dans ArcaOS
(DHCP, ping, HTTP ~1 Mo/s par moments), lien instable sur la durée (téléchargements qui
calent), essai d'endurance (`endurance.cmd`) lancé, sshd non installé (dépôts yum
injoignables : Arca Noae en panne, Netlabs qui cale). L'image ArcaOS reste dans
`eeepc701-arcaos.img` ; le SSD du 701 sera écrasé par ReactOS.

## État actuel et prochaines étapes

- Débit faible : ~10 Ko/s en HTTP (4,8 avant calibrage + ANI). La radio rate des trames
  (balises comprises) sans les voir : piste sensibilité / calibrage / ANI / accusés de
  réception matériels. Comparer avec ath5k (mmiotrace) pendant un flux.
- Le dépôt yum d'Arca Noae (repos.arcanoae.com) est en panne ; Netlabs répond
  (rpm.netlabs.org en HTTP).
- À faire : débit, **sshd dans ArcaOS** (accès direct sans redémarrages), utilitaire PM
  de choix du réseau (demandé par l'utilisateur), WPA2, publier l'avancement.

## Organisation du dépôt

| Chemin | Rôle |
|---|---|
| `vm.sh`, `disque.sh`, `installe.sh`, `ecran.sh`, `touche.sh`, `souris.py`, `tape*.py` | installation d'ArcaOS en QEMU et pilotage de la VM |
| `apport/EEEPC.CMD` | réglages Eee PC appliqués dans ArcaOS |
| `eeekeys/` | démon des touches Fn |
| `cle/` | clé USB Alpine (`preparer-racine.sh`, `construire-cle.sh`, `ovl/`, `test-vm.sh`) |
| `athos/core/` | cœur du pilote (ath5k + glue + athos + wlan) |
| `athos/compat/` | `lxcompat.h` (API Linux, branches GCC et OpenWatcom), `osdep.h` |
| `athos/linux/` | banc Linux : `envoyer.sh`, `lancer.sh`, `essai.c`, `connecter.c`, `tracer.sh` (mmiotrace) |
| `athos/traces/` | `comparer.py` (accès registres ath5k ⇄ athos) |
| `athos/os2/` | pilote OS/2 : `pilote.c`, `osdep_os2.c`, `athoslog.c`, `construire.sh`, `preparer-cle.sh`, `cycle.sh`, `install/` |
| `releves/` | relevés matériels (BSSID masqué) |

Hors dépôt (non redistribuables ou volumineux) : `watcom/` (OpenWatcom),
`telechargements/` (kit ACPI, Drv32, MiniDDK, ISO Alpine), images, `ecrans/`.

## Travailler

Le 701 démarre sur la clé (BIOS : clé USB en premier). Sous Linux il est joignable par
`ssh eeepc` (192.168.1.11).

- **Essai du cœur sous Linux** : `athos/linux/envoyer.sh` (copie + compile sur le 701),
  puis sur le 701 `./linux/lancer.sh ./linux/connecter Livebox-F550` lancé détaché
  (setsid nohup) ; il retire la carte à ath5k, fait l'essai, la rend et reconnecte le
  Wi-Fi ; garde-fou : `reboot -f` après 180 s. Résultat dans `/root/athos/essai.log`.
- **Pilote OS/2** : `athos/os2/construire.sh` (OpenWatcom sous Linux) → `athos.os2`,
  `athoslog.exe`.
- **Cycle ArcaOS automatique** : `athos/os2/cycle.sh [--pilote] [commandes.cmd]` :
  dépose le pilote (avec `--pilote`) et le fichier de commandes OS/2 (FAIRE.CMD) sur la
  partition ATHOS, demande à extlinux un démarrage unique sur l'entrée « arcaos »
  (`chain.c32 hd1 1 swap setbpb`), redémarre ; dans ArcaOS l'agent `ATHOSAGT.CMD`
  (dossier Démarrage) écrit `ETAT.TXT` (athoslog, ifconfig, netstat, ping), exécute
  FAIRE.CMD (→ `SORTIE.TXT`), installe le nouveau pilote **pour le démarrage suivant**,
  puis redémarre (fichier `RETOUR`) ; le 701 revient sur Linux et `cycle.sh` affiche
  les résultats. Un cycle ≈ 3 min. Pour essayer un nouveau pilote : un cycle
  `--pilote`, puis un cycle d'essai.
- Commit avec l'identité locale jeremie <jeremielud@gmail.com>, push par SSH
  (`git@github.com:jeremielud/eeepc701-arcaos.git`).

## Pièges connus (ne pas les refaire)

- ArcaOS redémarre en boucle sous KVM (Ryzen) : installer en TCG pentium3.
- La 2e phase d'installation d'ArcaOS exige l'ISO dans le lecteur.
- Linux ne monte pas le JFS d'OS/2 (EINVAL) ; ArcaOS ne lit pas le FAT32 sans module :
  échanger par la partition FAT16 « ATHOS ».
- REXX exige CRLF ; fichiers pour OS/2 en cp850 (`iconv -t cp850//TRANSLIT`).
- `__BIG_ENDIAN` est défini par la libc : le retirer (sinon descripteurs inversés).
- L'interrupteur radio GPIO 0 : 1 puis repasser en entrée.
- **Ne jamais lire tous les registres pendant qu'ath5k tient la carte** (soft lockup).
- La clé tourne en RAM : `apk add` à refaire après chaque démarrage (gcc, syslinux,
  sfdisk) ; agrandir le tmpfs racine pour gcc (`mount -o remount,size=380M /`).
- Après un redémarrage, la clé est montée sur `/media/sdb1` (pas `/media/usb`).
- Anneau de réception : chaque descripteur rendu redevient la queue ; ne jamais traiter
  celui pointé par RXDP. File d'émission : garder le dernier descripteur comme ancre.
- La pile NDIS ouvre la carte (interruptions) avant la liaison ; la recherche et
  l'association réinitialisent la puce : rétablir l'IMR après chaque changement de canal.
- Licences : kit Drv32/MiniDDK acceptés par l'utilisateur, **binaires pour usage
  personnel seulement** ; ne jamais publier le kit ni `athos.os2`. Ne jamais publier de
  BSSID ni la licence ArcaOS.
