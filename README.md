# ArcaOS sur l'Eee PC 701

Faire tourner **ArcaOS 5.0.7** (le successeur d'OS/2) sur un **Asus Eee PC 701** (Celeron M
900 MHz, 512 Mo, SSD de 4 Go, écran 800×480), avec tout le matériel en état de marche,
y compris la carte Wi-Fi Atheros AR2425, pour laquelle aucun pilote OS/2 n'existe.

## État

| Matériel | État |
|---|---|
| Vidéo Intel GMA 900 en 800×480 (Panorama) | ✅ vérifié sur la machine |
| Son Realtek ALC662 (Uniaud) | ✅ vérifié |
| Luminosité aux touches Fn | ✅ vérifiée |
| Volume / sourdine aux touches Fn (`eeekeys`) | ⏳ à confirmer |
| Ethernet Attansic L2 (MultiMac MMAE) | ⏳ non testé (pas de câble) |
| Clés USB et lecteur SD | configurés |
| **Wi-Fi Atheros AR2425** | 🚧 pilote **athos** en cours d'écriture |

## Contenu

- **`eeekeys/`** : démon OS/2 des touches Fn. Le BIOS de l'Eee PC les signale par des
  notifications ACPI du périphérique `ASUS010` ; `eeekeys` les reçoit par le kit ACPI
  d'Arca Noae et règle le volume (MMPM/2) et la luminosité. Compilé avec OpenWatcom.
- **`athos/`** : le pilote Wi-Fi (voir plus bas).
- **`cle/`** : clé USB Alpine Linux qui, au démarrage du 701, se connecte toute seule au
  Wi-Fi, ouvre un accès SSH (par clé uniquement) pour travailler à distance, et installe
  ArcaOS sur le disque interne (avec confirmation et vérification). Construite sans droits
  root (`preparer-racine.sh` puis `construire-cle.sh`), testable dans QEMU (`test-vm.sh`).
- **Racine** : scripts d'installation d'ArcaOS en machine virtuelle (`vm.sh`, `disque.sh`),
  outils de pilotage de la VM (`ecran.sh`, `touche.sh`, `souris.py`, `tape.py`), réglages
  Eee PC appliqués dans ArcaOS (`apport/EEEPC.CMD`). Détails dans [LISEZMOI.md](LISEZMOI.md).

L'image d'ArcaOS n'est pas publiée : elle contient une licence nominative. Il faut son
propre exemplaire d'ArcaOS.

## Le pilote Wi-Fi athos

Aucun pilote OS/2 ne gère l'Atheros AR2425 (AR5007EG). La méthode :

1. **Cœur matériel** : les fichiers « matériel » du pilote Linux `ath5k` (licence ISC :
   initialisation, radio, EEPROM, DMA, files), repris presque tels quels dans
   `athos/core/`, avec une couche de compatibilité (`athos/compat/lxcompat.h`) qui remplace
   l'API du noyau Linux par quelques services de plate-forme (`osdep.h`).
2. **Mise au point sous Linux** (`athos/linux/`) : sur le 701 démarré sur la clé, la carte
   est retirée à `ath5k` et pilotée par le cœur depuis l'espace utilisateur (registres par
   sysfs, DMA dans des pages verrouillées). Le Wi-Fi étant la seule liaison, chaque essai
   tourne seul (`lancer.sh`) : la carte est détachée, l'essai s'exécute, puis la carte est
   rendue à Linux et le Wi-Fi se reconnecte ; une sécurité redémarre le 701 en cas de blocage.
   Les accès du vrai pilote `ath5k` sont relevés par `mmiotrace` (`tracer.sh`) et comparés
   à ceux du cœur (`traces/comparer.py`).
3. **Pilote OS/2** : le même cœur, enveloppé en pilote réseau NDIS 2.0, plus une pile 802.11
   réduite (recherche, association, WPA2).
4. **Outils OS/2** : un utilitaire graphique (Presentation Manager) de choix du réseau et
   du mot de passe, et un accès SSH à ArcaOS pour la maintenance à distance.

Avancement :

- [x] Couche de compatibilité ; les fichiers matériels d'ath5k compilent et se lient hors de Linux
- [x] Initialisation de la puce : révisions, EEPROM, adresse MAC, réinitialisation sur les 13 canaux
- [x] Banc d'essai autonome sur le 701 et traces comparées avec `ath5k`
- [x] Réception : la puce reçoit les balises ; recherche des réseaux sur les 13 canaux
- [x] Émission, authentification, association (réseau ouvert) et données : le 701 obtient une offre DHCP de la box
- [ ] WPA2
- [ ] Enveloppe NDIS 2.0 pour OS/2, utilitaire graphique, accès SSH dans ArcaOS
