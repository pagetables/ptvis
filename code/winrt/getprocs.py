
prev = None
for line in open('proclist.txt'):
  if "PROCESS" in line:
    prev = line.split()
  if 'Image' in line:
    if not prev:
      print 'wut',line
    else:
      print 'dt nt!_KPROCESS %s Asid PageDirectory'%prev[1]

  #,'#',line.strip(), '@', prev[1]
      
