/* ATHOSINS.CMD : installe (ou met à jour) le pilote Wi-Fi athos dans ArcaOS, depuis
   le dossier \ATHOS de la clé USB. À lancer d'un double-clic, puis redémarrer.
   - copie ATHOS.OS2 et ATHOS.NIF dans C:\IBMCOM\MACS, ATHOSLOG.EXE et ATHOSJRN.CMD
     dans C:\OS2\APPS ;
   - CONFIG.SYS : ajoute DEVICE=C:\IBMCOM\MACS\ATHOS.OS2 après le pilote MMAE ;
   - PROTOCOL.INI : section [ATHOS_nif] (SSID), et TCP/IP rattaché à ATHOS_nif au
     lieu de la carte filaire ;
   - dossier Démarrage : ATHOSJRN.CMD, qui recopie le journal sur la clé.
   Copies de secours (la première fois seulement) : C:\CONFIG.ATH et
   C:\IBMCOM\PROTOCOL.ATH. En cas de souci au démarrage : Alt+F1 au logo, puis
   ligne de commande, et  copy C:\CONFIG.ATH C:\CONFIG.SYS                       */
call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs
parse source . . self
src = filespec('drive', self) || filespec('path', self)
boot = left(value('COMSPEC',, 'OS2ENVIRONMENT'), 2)
ssid = 'Livebox-F550'
if stream(src'SSID.TXT', 'c', 'query exists') <> '' then do
  ssid = strip(linein(src'SSID.TXT')); call stream src'SSID.TXT', 'c', 'close'
end
log = src'INSTALL.LOG'
call SysFileDelete log
call note date() time() 'installation athos depuis' src', SSID' ssid

/* 1. Fichiers */
'@copy' src'ATHOS.OS2' boot'\IBMCOM\MACS\ >nul'; call note 'ATHOS.OS2 :' rc
'@copy' src'ATHOS.NIF' boot'\IBMCOM\MACS\ >nul'; call note 'ATHOS.NIF :' rc
'@copy' src'ATHOSLOG.EXE' boot'\OS2\APPS\ >nul'; call note 'ATHOSLOG.EXE :' rc
'@copy' src'ATHOSJRN.CMD' boot'\OS2\APPS\ >nul'; call note 'ATHOSJRN.CMD :' rc

/* 2. CONFIG.SYS */
cfg = boot'\CONFIG.SYS'
if stream(boot'\CONFIG.ATH', 'c', 'query exists') = '' then '@copy' cfg boot'\CONFIG.ATH >nul'
call lire cfg
deja = 0; apres = 0
do i = 1 to l.0
  u = translate(l.i)
  if pos('ATHOS.OS2', u) > 0 then deja = 1
  if pos('MMAE.OS2', u) > 0 then apres = i
  if apres = 0 & pos('PROTMAN.OS2', u) > 0 then apres = i
end
if \deja then do
  if apres = 0 then apres = l.0
  call inserer apres, 'DEVICE='boot'\IBMCOM\MACS\ATHOS.OS2'
  call ecrire cfg
  call note 'CONFIG.SYS : ligne DEVICE ajoutée après la ligne' apres
end
else call note 'CONFIG.SYS : déjà en place'

/* 3. PROTOCOL.INI */
ini = boot'\IBMCOM\PROTOCOL.INI'
'@copy' ini src'PROTOCOL.AVT >nul'
if stream(boot'\IBMCOM\PROTOCOL.ATH', 'c', 'query exists') = '' then '@copy' ini boot'\IBMCOM\PROTOCOL.ATH >nul'
call lire ini
section = ''; filaire = ''; athos = 0
do i = 1 to l.0
  t = strip(l.i)
  if left(t, 1) = '[' then section = strip(translate(t, '  ', '[]'))
  parse var t cle '=' val
  if translate(strip(cle)) = 'DRIVERNAME' & translate(strip(val)) = 'MMAE$' then filaire = section
  if translate(section) = 'ATHOS_NIF' then athos = 1
end
call note 'PROTOCOL.INI : section filaire' filaire', athos déjà là :' athos
do i = 1 to l.0
  parse var l.i cle '=' val
  if translate(strip(cle)) = 'BINDINGS' & filaire <> '' & pos(translate(filaire), translate(val)) > 0 then do
    p = pos(translate(filaire), translate(l.i))
    l.i = left(l.i, p - 1) || 'ATHOS_nif' || substr(l.i, p + length(filaire))
    call note '  liaison :' strip(l.i)
  end
  if athos & translate(strip(cle)) = 'SSID' then l.i = '  SSID = "'ssid'"'
end
if \athos then do
  n = l.0
  n = n + 1; l.n = ''
  n = n + 1; l.n = '[ATHOS_nif]'
  n = n + 1; l.n = ''
  n = n + 1; l.n = '  DriverName = ATHOS$'
  n = n + 1; l.n = '  SSID = "'ssid'"'
  l.0 = n
end
call ecrire ini
'@copy' ini src'PROTOCOL.APR >nul'

/* 4. Journal recopié sur la clé au démarrage */
ok = SysCreateObject('WPProgram', 'Journal athos', '<WP_START>', ,
  'EXENAME='boot'\OS2\APPS\ATHOSJRN.CMD;MINIMIZED=YES;OBJECTID=<ATHOSJRN>;', 'R')
call note 'objet de démarrage :' ok
call note 'terminé : redémarrez ArcaOS (clé retirée ou non).'
say ''
say 'Installation terminée. Redémarrez ArcaOS.'
'@pause'
exit 0

lire: procedure expose l.
  parse arg f
  l.0 = 0
  do while lines(f)
    n = l.0 + 1; l.n = linein(f); l.0 = n
  end
  call stream f, 'c', 'close'
  return

ecrire: procedure expose l.
  parse arg f
  call SysFileDelete f
  do i = 1 to l.0
    call lineout f, l.i
  end
  call stream f, 'c', 'close'
  return

inserer: procedure expose l.
  parse arg apres, texte
  do i = l.0 to apres + 1 by -1
    j = i + 1; l.j = l.i
  end
  j = apres + 1; l.j = texte
  l.0 = l.0 + 1
  return

note: procedure expose log
  call lineout log, arg(1)
  say arg(1)
  return
