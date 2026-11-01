/* EEEPC.CMD : adapte ArcaOS … l'Eee PC 701. · lancer une fois, depuis le disque
   d'apport (lecteur indiqu‚ en argument, par d‚faut celui du script).
   - clavier fran‡ais et pages de code 850/1004 ;
   - cl‚s USB et carte SD ;
   - d‚mon des touches Fn (EEEKEYS.EXE) lanc‚ au d‚marrage du bureau ;
   - Panorama : ‚cran 800x480 (r‚solution native de l'‚cran du 701). */
call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs
parse source . . self
src = filespec('drive', self) || filespec('path', self)
boot = left(value('COMSPEC',, 'OS2ENVIRONMENT'), 2)   /* C: */
cfg = boot'\CONFIG.SYS'
log = boot'\VAR\LOG\EEEPC.LOG'
call lineout log, date() time() 'adaptation Eee PC 701'

/* 1. Le d‚mon des touches Fn. */
'@copy' src'EEEKEYS.EXE' boot'\OS2\APPS\EEEKEYS.EXE >nul'
call note 'EEEKEYS.EXE copi‚ :' rc

/* 2. CONFIG.SYS : lu en entier, ligne par ligne r‚‚crite. */
lines.0 = 0
do while lines(cfg)
  n = lines.0 + 1; lines.n = linein(cfg); lines.0 = n
end
call stream cfg, 'c', 'close'
'@copy' cfg boot'\CONFIG.EEE >nul'          /* copie de sauvegarde */
do i = 1 to lines.0
  l = lines.i; u = translate(l)
  select
    when abbrev(u, 'COUNTRY=') then lines.i = 'COUNTRY=033,'boot'\OS2\SYSTEM\COUNTRY.SYS'
    when abbrev(u, 'CODEPAGE=') then lines.i = 'CODEPAGE=850,1004'
    when abbrev(u, 'DEVINFO=KBD') then lines.i = 'DEVINFO=KBD,FR189,'boot'\OS2\KEYBOARD.DCP'
    /* cl‚s USB et carte SD (le lecteur SD du 701 est en USB) : 4 lettres */
    when abbrev(u, 'BASEDEV=USBMSD.ADD') then lines.i = 'BASEDEV=USBMSD.ADD /REMOVABLES:4 /FLOPPIES:0'
    otherwise nop
  end
end
call SysFileDelete cfg
do i = 1 to lines.0
  call lineout cfg, lines.i
end
call stream cfg, 'c', 'close'
call note 'CONFIG.SYS r‚‚crit ('lines.0' lignes)'

/* 3. Le d‚mon d‚marre avec le bureau (dossier D‚marrage) : c'est un programme
   Presentation Manager, il ne peut pas ˆtre lanc‚ par RUN= dans CONFIG.SYS. */
ok = SysCreateObject('WPProgram', 'Touches Fn Eee PC', '<WP_START>', ,
  'EXENAME='boot'\OS2\APPS\EEEKEYS.EXE;PARAMETERS=-q;MINIMIZED=YES;OBJECTID=<EEEKEYS>;', 'R')
call note 'objet de d‚marrage EEEKEYS :' ok

/* 4. Panorama : r‚solution manuelle 800x480 (l'EDID de l'‚cran interne du 701 est
   souvent absent), puis mode 800x480 choisi pour le Presentation Manager. */
pano = SysSearchPath('PATH', 'PANOUTIL.EXE')
if pano = '' then call note 'PANOUTIL.EXE introuvable'
else do
  '@'pano '-c 800 480' boot '>>' log
  call note 'PanoUtil -c 800 480 :' rc
  '@'pano '-p 800 480' boot '>>' log
  call note 'PanoUtil -p 800 480 :' rc
end
call note 'termin‚'
exit 0

note: procedure expose log
  call lineout log, '  ' arg(1)
  say arg(1)
  return
