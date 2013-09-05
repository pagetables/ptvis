#!/usr/bin/python

p = {}
dirbase = ""
for line in open("procs.txt"):
    if 'DirBase' in line:
        parts = line.split()
        dirbase = parts[1]    
    if 'Image' in line:
        imgname = line.split()[1]
        p[imgname] = dirbase

import os
for z in p:
    os.system ( "../a.out 0x%s > dumps/%s"%(p[z], z) )

