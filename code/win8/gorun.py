data="""
0: kd>  dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR
   +0x000 NumberOfRuns     : 4
   +0x008 NumberOfPages    : 0x3ff7d
   +0x010 Run              : [1] _PHYSICAL_MEMORY_RUN
0: kd>  dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR -a Run[0].
   +0x010 Run     : [0] 
      +0x000 BasePage : 1
      +0x008 PageCount : 0x9e
0: kd>  dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR -a Run[1].
   +0x010 Run     : [1] 
      +0x000 BasePage : 0x100
      +0x008 PageCount : 2
0: kd>  dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR -a Run[2].
   +0x010 Run     : [2] 
      +0x000 BasePage : 0x103
      +0x008 PageCount : 0x3fddd
0: kd>  dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR -a Run[3].
   +0x010 Run     : [3] 
      +0x000 BasePage : 0x3ff00
      +0x008 PageCount : 0x100
0: kd>  dt poi(nt!MmPhysicalMemoryBlock)  nt!_PHYSICAL_MEMORY_DESCRIPTOR -a Run[4].
"""

cur_file_offset = 0xb000
base = 0
pageCount = 0
for line in data.splitlines():
	if 'BasePage' in line:
		base = line.split()[-1]+'000'
	elif 'PageCount' in line:
		size = line.split()[-1]+'000'
		print '{',base, ',', hex(cur_file_offset), ',', size, '},'
		cur_file_offset += int(size, 16)
