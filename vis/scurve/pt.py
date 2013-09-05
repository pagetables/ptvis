
class Page:
  def __init__(self, args):
      # start, end, read_user, read_super, write_user, write_super, execute_user, execute_user):
      for x in args:
          setattr(self, x, args[x])

def sign_extend(x):
  if x  >= 0x800000000000L:
    x += 0xffff000000000000L
  return x

class x86_2mb_page(Page):
  def __init__(self, line):
    args = {}
    parts = line.split()
    #start_page = int(parts[-1],16) & 0xffffffff#phys
    start_page = sign_extend(int(parts[6].split('=')[1],16))
    end_page = start_page + 2*1024*1024
    rw = parts[8]
    super = parts[9]
    execute = parts[16]

    super = 'S' in super
    read =  'R' in rw or 'W' in rw #read or write its always readable
    write = 'W' in rw
    execute = 'XD' not in execute

    args['start'] = start_page
    args['end'] = end_page
    args['read_user'] = not super
    args['read_super'] = True
    args['write_user'] = not super and write
    args['write_super'] = write and super
    args['execute_user'] = execute and not super
    args['execute_super'] = execute and super
    Page.__init__(self, args)

class x86_4k_page(Page):
  def __init__(self, line):
    args = {}
    parts = line.split()
    #start_page = int(parts[-1], 16) & 0xffffffff #phys
    #print hex(start_page)
    #start_page = int(parts[0].split('=')[1],16)    
    start_page = sign_extend( int(parts[5].split('=')[1],16) )
    end_page = start_page + 4096
    rw = parts[7]
    super = parts[8]
    execute = parts[15]

    super = 'S' in super
    read =  1#read or write its always readable
    write = 'W' in rw
    execute = 'XD' not in execute

    args['start'] = start_page
    args['end'] = end_page
    args['read_user'] = not super
    args['read_super'] = True
    args['write_user'] = not super and write
    args['write_super'] = write and super
    args['execute_user'] = execute and not super
    args['execute_super'] = execute and super
    Page.__init__(self, args)

class arm_4k_page(Page):
  def __init__(self, line):
    args = {}
    parts = line.split()
    start_page = int(parts[0].split('=')[1],16)
    end_page = start_page + 4096

    args['start'] = start_page
    args['end'] = end_page
    args['read_user'] = '/X' not in line
    args['read_super'] = 'RO/' in line or 'RW/' in line
    args['write_user'] = '/RW' in line
    args['write_super'] = 'RW/' in line
    args['execute_user'] = 'XN=0' in line and (('/RO' in line) or ('/RW' in line))
    args['execute_super'] =  'XN=0' in line and '/X' in line #TODO PXN?
    Page.__init__(self, args)

class arm_1MB_page(Page):
  def __init__(self, line):
    args = {}
    parts = line.split()
    start_page = int(parts[0].split('=')[1],16)
    end_page = start_page + 1024*1024

    args['start'] = start_page
    args['end'] = end_page
    args['read_user'] = '/X' not in line
    args['read_super'] = 'RO/' in line or 'RW/' in line
    args['write_user'] = '/RW' in line
    args['write_super'] = 'RW/' in line
    args['execute_user'] = 'XN=0' in line and (('/RO' in line) or ('/RW' in line))
    args['execute_super'] = 'XN=0' in line and '/X' in line #TODO PXN?

    Page.__init__(self, args)
    
def h(n, shift=0, offset=0):
  return n

def parse_arm(strings):
  Pages = {}
  for line in strings.splitlines():
    line = line.strip()
    if not line: continue
    if 'ttbcr' in line: continue
    if 'ttbr' in line: continue
    if 'sctlr' in line: continue
    if '<<' in line: continue
    if '>>' in line: continue

    if 'page table base=' in line:
      #domain info may be interesting
      pass
    elif 'small base' in line:
      page = arm_4k_page(line)
      Pages[page.start] = page
    elif 'section base' in line:
      page = arm_1MB_page(line)
      #go ahead and add each 10 minor entry
      i = 0
      while i <= (page.end-page.start):
        Pages[page.start + i] = page
        i += 1*4096
    else:
      if 'bytes from' in line:
        continue
      print 'TODO', line
  return Pages

def parse_ia32e(strings):
    Pages = {}
    pml4s = []
    
    for line in strings.splitlines():
      line = line.strip()
      if not line: continue
      if 'Connected' in line: continue
      if '---' in line: continue
      if 'PML4E' in line and 'flags' in line:
        name = line.split()[1]
        #'attributes': line,
        #pml4s += [{'name': 'PML4E_'+h(name, 51), 'children': [], "size": 3938, "x":0}]
      elif 'PDE PDPTE' in line:
        name = line.split()[3]
        #'attributes': line, 
        entry = {'name': 'PDPTE_'+h(name), 'children': [], "size" : 3938, "x":0}
        #pml4s[-1]['children'].append( entry )
      elif 'PTE PDE flags' in line:
        parts = line.split()
        pde_name = parts[5]
        pde_entry = {'name': 'PDE_'+h(pde_name), 'children': [], "size": 3938, "x":0}
        #pml4s[-1]['children'][-1]['children'].append( pde_entry )
      elif 'PTE:' in line:
        parts = line.split()
        pde_name = parts[3]
        pte_name = parts[4]
        #now put in the pte entry
        pte_entry = {'name' : 'PTE_'+h(pte_name), 'children': [], "size": 3938, "x":0 }
        #pml4s[-1]['children'][-1]['children'][-1]['children'].append( pte_entry )
        page = x86_4k_page(line)
        Pages[page.start] = page
      elif '2MB PDE flags' in line:
        parts = line.split()
        pde_name = parts[5]
        pde_entry = {'name': '2MBPDE_'+h(pde_name), 'children': [], "size": 3938, "x":0}
        #pml4s[-1]['children'][-1]['children'].append( pde_entry )
        page = x86_2mb_page(line)
        Pages[page.start] = page
      else:
        print 'UNHANDLED', line
        pass      
    return Pages

