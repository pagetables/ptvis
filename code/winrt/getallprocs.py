import os

procs = {}
prev = None
for line in open('proclist.txt'):
  if "PROCESS" in line:
    prev = line.split()
  if 'Image' in line:
    if not prev:
      print 'wut',line
    else:
      procs[prev[1].strip()] = line.split()[-1].strip()

print procs
ASIDS = open("ASIDs").read().split()

for line in open('allprocs.txt'):
  #print line.strip()
  if '_KPROCESS' in line:
    proc_name = procs[line.split()[4]]
    print proc_name
  if 'PageDirectory :' in line:
    os.system( "./rtdump %s > dumps/%s"%(line.split()[3], proc_name) )
    print line.split()[3]
    pass #ignore for now
  if 'Asid' in line and 'Page' not in line:
    target = ASIDS[int(line.split()[-1],16)]
    print '0x'+target
    os.system( "./rtdump 0x%s > dumps/%s_asid"%(target, proc_name) )
