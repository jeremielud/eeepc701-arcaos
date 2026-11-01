/* ATHOSJRN.CMD : lancé au démarrage d'ArcaOS (dossier Démarrage). Attend que le
   réseau ait eu le temps de monter, puis écrit l'état du pilote athos, son journal et
   l'état du réseau dans \ATHOS\JOURNAL.TXT sur la clé USB (s'il y en a une).      */
call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs
call SysSleep 45
cle = ''
do essai = 1 to 6 while cle = ''
  lecteurs = SysDriveMap('D:', 'USED')
  do i = 1 to words(lecteurs) while cle = ''
    d = word(lecteurs, i)
    if stream(d'\ATHOS\ATHOSINS.CMD', 'c', 'query exists') <> '' then cle = d
  end
  if cle = '' then call SysSleep 10
end
if cle = '' then exit 1
f = cle'\ATHOS\JOURNAL.TXT'
call SysFileDelete f
call lineout f, '=== ArcaOS, ' date() time()
call stream f, 'c', 'close'
'@athoslog.exe >>' f '2>&1'
'@echo === ifconfig lan0 >>' f
'@ifconfig lan0 >>' f '2>&1'
'@echo === netstat -r >>' f
'@netstat -r >>' f '2>&1'
'@echo === ping 192.168.1.1 >>' f
'@ping 192.168.1.1 56 4 >>' f '2>&1'
'@echo === fin >>' f
call stream f, 'c', 'close'
/* Nouvelle version du pilote sur la clé : installée pour le prochain démarrage. */
boot = left(value('COMSPEC',, 'OS2ENVIRONMENT'), 2)
neuf = cle'\ATHOS\ATHOS.OS2'; actuel = boot'\IBMCOM\MACS\ATHOS.OS2'
if stream(neuf, 'c', 'query datetime') <> stream(actuel, 'c', 'query datetime') |,
   stream(neuf, 'c', 'query size') <> stream(actuel, 'c', 'query size') then do
  '@copy' neuf boot'\IBMCOM\MACS\ >nul'
  '@copy' cle'\ATHOS\ATHOSLOG.EXE' boot'\OS2\APPS\ >nul'
  '@copy' cle'\ATHOS\ATHOSJRN.CMD' boot'\OS2\APPS\ >nul'
  call lineout f, '=== nouvelle version du pilote installée (code' rc') : redémarrer ArcaOS'
  call stream f, 'c', 'close'
end
exit 0
