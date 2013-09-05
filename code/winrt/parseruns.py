data="""
0: kd> dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR
   +0x000 NumberOfRuns     : 0xd
   +0x004 NumberOfPages    : 0x7c6d0
   +0x008 Run              : [1] _PHYSICAL_MEMORY_RUN
0: kd> dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR -a Run[0].
   +0x008 Run     : [0]
      +0x000 BasePage : 0x82006
      +0x004 PageCount : 0x416
0: kd> dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR -a Run[1].
   +0x008 Run     : [1]
      +0x000 BasePage : 0x82420
      +0x004 PageCount : 0x79fe0
0: kd> dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR -a Run[2].
   +0x008 Run     : [2]
      +0x000 BasePage : 0xfd400
      +0x004 PageCount : 0x1b1
0: kd> dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR -a Run[3].
   +0x008 Run     : [3]
      +0x000 BasePage : 0xfd5b5
      +0x004 PageCount : 0xe
0: kd> dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR -a Run[4].
   +0x008 Run     : [4]
      +0x000 BasePage : 0xfd5c6
      +0x004 PageCount : 0xb2
0: kd> dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR -a Run[5].
   +0x008 Run     : [5]
      +0x000 BasePage : 0xfd687
      +0x004 PageCount : 0xaa
0: kd> dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR -a Run[6].
   +0x008 Run     : [6]
      +0x000 BasePage : 0xfd7b1
      +0x004 PageCount : 0x12f
0: kd> dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR -a Run[7].
   +0x008 Run     : [7]
      +0x000 BasePage : 0xfd8ec
      +0x004 PageCount : 0x1b
0: kd> dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR -a Run[8].
   +0x008 Run     : [8]
      +0x000 BasePage : 0xfd909
      +0x004 PageCount : 1
0: kd> dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR -a Run[9].
   +0x008 Run     : [9]
      +0x000 BasePage : 0xfd90e
      +0x004 PageCount : 1
0: kd> dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR -a Run[a].
   +0x008 Run     : [10]
      +0x000 BasePage : 0xfd910
      +0x004 PageCount : 0x9dc
0: kd> dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR -a Run[b].
   +0x008 Run     : [11]
      +0x000 BasePage : 0xfe2ed
      +0x004 PageCount : 0xd78
0: kd> dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR -a Run[c].
   +0x008 Run     : [12]
      +0x000 BasePage : 0xff0dd
      +0x004 PageCount : 0x71f
"""

cur_file_offset = 0x21000
base = 0
pageCount = 0
for line in data.splitlines():
  if 'BasePage' in line:
    base = line.split()[-1]+'000'
  elif 'PageCount' in line:
    size = line.split()[-1]+'000'
    print '{',base, ',', hex(cur_file_offset), ',', size, '},'
    cur_file_offset += int(size,16)
    