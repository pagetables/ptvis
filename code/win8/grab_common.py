#!/usr/bin/python

import os
import sys

maps = {}

for x in range(1,11):
    fdata = open("win8%d/rwx_maps"%x,'r')
    for line in fdata:
        x = line.find("virt=")
        address = line[x:].split()[0].split("=")[1]
        if address not in maps:
            maps[address] = [x]
        else:
            maps[address] += [x]

for x in maps:
    count = len(maps[x]) 
    if count == 10:
        print x, count