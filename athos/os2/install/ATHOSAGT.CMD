/* ATHOSAGT.CMD : agent de maintenance à distance d'ArcaOS (dossier Démarrage).
   À chaque démarrage, si la clé USB (partition FAT16 « ATHOS ») est là :
   1. note l'état du pilote Wi-Fi et du réseau dans \ATHOS\ETAT.TXT ;
   2. exécute \ATHOS\FAIRE.CMD s'il existe (sortie dans \ATHOS\SORTIE.TXT), puis le
      renomme FAIT.CMD pour ne pas le refaire ;
   3. installe une nouvelle version du pilote ou de l'agent trouvée sur la clé ;
   4. si \ATHOS\RETOUR existe, redémarre (le 701 revient alors sur Linux, sur la clé).
   Chaque étape est notée dans \ATHOS\AGENT.TXT.                                  */
call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs
boot = left(value('COMSPEC',, 'OS2ENVIRONMENT'), 2)
call SysSleep 20
cle = ''
do essai = 1 to 12 while cle = ''
  lecteurs = SysDriveMap('A:', 'USED')
  do i = 1 to words(lecteurs) while cle = ''
    d = word(lecteurs, i)
    if d = 'A:' | d = 'B:' | d = boot then iterate
    if stream(d'\ATHOS\ATHOSINS.CMD', 'c', 'query exists') <> '' then cle = d
  end
  if cle = '' then call SysSleep 10
end
if cle = '' then exit 1
a = cle'\ATHOS'
journal = a'\AGENT.TXT'
call SysFileDelete journal
call note 'agent démarré' date() time() ', clé' cle', lecteurs' SysDriveMap('A:', 'USED')

/* 1. État */
e = a'\ETAT.TXT'
call SysFileDelete e
'@athoslog.exe >' e '2>&1'
call note 'athoslog :' rc
'@ifconfig lan0 >>' e '2>&1'
'@netstat -r >>' e '2>&1'
'@ping 192.168.1.1 56 3 >>' e '2>&1'
call note 'état écrit'

/* 2. Commandes */
if stream(a'\FAIRE.CMD', 'c', 'query exists') <> '' then do
  call SysFileDelete a'\FAIT.CMD'
  '@copy' a'\FAIRE.CMD' a'\FAIT.CMD >nul'
  call SysFileDelete a'\FAIRE.CMD'
  call note 'exécution de FAIRE.CMD'
  '@call' a'\FAIT.CMD >' a'\SORTIE.TXT 2>&1'
  call note 'FAIRE.CMD terminé :' rc
end

/* 3. Mises à jour */
call maj 'ATHOS.OS2', boot'\IBMCOM\MACS'
call maj 'ATHOSLOG.EXE', boot'\OS2\APPS'
call maj 'ATHOSAGT.CMD', boot'\OS2\APPS'

/* 4. Retour sur Linux */
if stream(a'\RETOUR', 'c', 'query exists') <> '' then do
  call note 'retour sur Linux : redémarrage'
  call SysSleep 5
  '@setboot /B'
end
call note 'fin'
exit 0

maj: procedure expose a journal
  parse arg nom, dest
  neuf = a'\'nom; actuel = dest'\'nom
  if stream(neuf, 'c', 'query exists') = '' then return
  if stream(neuf, 'c', 'query size') = stream(actuel, 'c', 'query size') &,
     stream(neuf, 'c', 'query datetime') = stream(actuel, 'c', 'query datetime') then return
  '@copy' neuf dest'\ >nul'
  call note 'mise à jour de' nom ':' rc
  return

note: procedure expose journal
  call lineout journal, time() arg(1)
  call stream journal, 'c', 'close'
  return
