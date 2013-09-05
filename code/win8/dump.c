#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>

struct ptregs
{
    uint64_t cr0,cr1,cr2,cr3,cr4;
    uint64_t efer_a, efer_d;
    uint32_t acip_id;
};

int fd;

typedef struct {
	uint64_t				BasePage;
	uint64_t				PageCount;
} _PHYSICAL_MEMORY_RUN64;
 
typedef struct {
	uint64_t				NumberOfRuns;
	uint64_t				NumberOfPages;
	_PHYSICAL_MEMORY_RUN64		Run[9];
} _PHYSICAL_MEMORY_DESCRIPTOR64;



void
DumpCR(uint8_t num, uint64_t value){
    switch(num){
        case 0:

            printf("CR0: %.16llx (%s %s %s %s %s %s %s %s %s %s %s)\n",
                  value,
                  (value & (1<<31)) ? "PG" : "pg",
                  (value & (1<<30)) ? "CD" : "cd",
                  (value & (1<<29)) ? "NW" : "nw",
                  (value & (1<<18)) ? "AM" : "am",
                  (value & (1<<16)) ? "WP" : "wp",
                  (value & (1<<05)) ? "NE" : "ne",
                  (value & (1<<04)) ? "ET" : "et",
                  (value & (1<<03)) ? "TS" : "ts",
                  (value & (1<<02)) ? "EM" : "em",
                  (value & (1<<01)) ? "MP" : "mp",
                  (value & (1<<00)) ? "PE" : "pe");
            
            break;
        case 1:
            // ???
            printf("CR1: %.16llx\n", value);
            break;
        case 2:
            printf("CR2: %.16llx\n", value);
            break;
        case 3:
            printf("CR3: %.16llx\n", value);
            break;
        case 4:
            printf("CR4: %.16llx (%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s)\n",
                value,
                (value & (1<<20)) ? "SMEP" : "smep",
                (value & (1<<18)) ? "OXSAVE" : "oxsave",
                (value & (1<<17)) ? "PCIDE" : "pcide",
                (value & (1<<14)) ? "SMXE" : "smxe",
                (value & (1<<13)) ? "VMXE" : "vmxe",
                (value & (1<<10)) ? "OSXMM" : "osxmm",
                (value & (1<<9 )) ? "OSFXSR" : "osfxsr",
                (value & (1<<8 )) ? "PCE" : "pce",
                (value & (1<<7 )) ? "PGE" : "pge",
                (value & (1<<6 )) ? "MCE" : "mce",
                (value & (1<<5 )) ? "PAE" : "pae",
                (value & (1<<4 )) ? "PSE" : "pse",
                (value & (1<<3 )) ? "DE"  : "de",
                (value & (1<<2 )) ? "TSD" : "tsd",
                (value & (1<<1 )) ? "PVI" : "pvi",
                (value & (1<<0 )) ? "VME" : "vme");
            break;
        default:
            printf("Unknown CR number: %d value=%llx\n", num,value);
            break;
    }
}

void handle_regs(struct ptregs *p)
{

    printf("CPU ACIP ID: %x\n", p->acip_id);
    
    DumpCR(0, p->cr0);
    DumpCR(2, p->cr2);
    DumpCR(3, p->cr3);
    DumpCR(4, p->cr4);
    
    
    /* The LMA bit (IA32_EFER.LMA[bit 10]) determines whether the processor is operating in IA-32e mode. When running in IA-32e mode, 64-bit or compatibility sub-mode operation is determined by CS.L bit of the code segment.
     */
    
    printf("EFER: %llx %llx (%s %s %s %s)\n", 
          p->efer_a, p->efer_d,
          (p->efer_a & (1<<11)) ? "NXE" : "nxe",
          (p->efer_a & (1<<10)) ? "LMA" : "lma",
          (p->efer_a & (1<<8 )) ? "LME" : "lme",
          (p->efer_a & (1<<0)) ? "SCE" : "sce"
          );
    
    //check for IA32-e on
    if(!(p->efer_a & (1<<10))){
        printf("Not in IA32.e paging mode\n");
        return;
    }

    //begin pagetable walk
    printf("PCID: %llx physaddr= %.16llx\n", (p->cr3&0xfff), (p->cr3>>12)<<12);
}

// virtual
// physical
//  size

int
offsets[][3] = {
{ 0x1000 , 0xb000 , 0x9e000 },
{ 0x100000 , 0xa9000 , 2000 },
{ 0x103000 , 0xab000 , 0x3fddd000 },
{ 0x3ff00000 , 0x3fe88000 , 0x100000 },
{0, 0, 0}
};

//
// Read total_size bytes from physical address @ start into virt buffer @ dest
// (Implementation specific)
//
int
physread(uint32_t phys_start, void* dest, uint32_t total_size)
{
    int ret;
    
    //
    // Go through each offset and figure out if this is mapped
    //
    int i;
    int *x;
    for (i = 0; ; i++)
    {
        x = &offsets[i];
        if (x[0] == 0)
        {
            return 1;
        }
        
        if ((phys_start >= x[0]) && (phys_start <= (x[0]+x[2])) )
        {
//            printf("match for %x %x %x %x\n", phys_start, x[0], x[1], x[2]);
            break;
        }

    }
    
    lseek(fd, phys_start - x[0] + x[1], SEEK_SET);
    ret = read(fd, dest, total_size);
    if (ret > 0)
        return 0;
    return 1;
}

void*
ReadPhysMem(void* connection_port, void *address, uint64_t size)
{
 
    
    if ((uint64_t)address == ((0x00ffffffffffffff)>>12)<<12)
    {
        printf("Invalid address: %p", address);
        return NULL;
    }
    
    char *buf = malloc(size);

    physread(address, buf, size);                                                                                                                                                                                                      
    
    return buf;
}

void
dumpPT(void* connection_port, int a, int b, int c, uint64_t pt_addr)
{
  uint64_t *addr = ReadPhysMem(connection_port, (void*)pt_addr, 512*8);
  if(!addr) return;
  int i, skip=0;
  uint64_t _a, _b, _c, _d;
  _a = a;
  _b = b;
  _c = c;
  for(i = 0; i < 512; i++){
    _d = i;
    uint64_t entry = addr[i];
    if(!(entry&1)){
      skip++;
      continue;
    } 
    uint64_t address = ((entry & 0x00ffffffffffffff)>>12)<<12;
    if(entry & (1)) {
      printf("    PTE: %.3d %.3d %.3d %.3d virt=%.16llx %s %s %s %s %s %s %s %s %s %s, dest = %.16llx\n",
          a,b,c,i,
          ((_a<<39)+(_b<<30)+(_c<<21)+(_d<<12)),
          entry&(1<<0) ? "P" : "p",
          entry&(1<<1) ? "W" : "R",
          entry&(1<<2) ? "U" : "S",
          entry&(1<<3) ? "PWT" : "pwt",
          entry&(1<<4) ? "PCD" : "pcd",
          entry&(1<<5) ? "A" : "a",
          entry&(1<<6) ? "D" : "d",
          entry&(1<<7) ? "PS" : "ps",
          entry&(1<<8) ? "G" : "g",
          entry&(1LL<<63) ? "XD" : "xd",
          address);
    }
  }
  free(addr);
}

void
dumpPD(void* connection_port, int a, int b, uint64_t pd_addr){
  int i;

  uint64_t *addr = ReadPhysMem(connection_port, (void*)pd_addr, 512*8);
  if(!addr) return;
    
  for(i = 0; i < 512; i++){
    uint64_t entry = addr[i];
    if(!(entry&1)){
      continue;
    }
    uint64_t address = ((entry & 0x00ffffffffffffff)>>12)<<12;
    
    //2MB GIG mapping
    if(entry & (1<<7)) {
      printf("   2MB PDE flags: %.3d %.3d %.3d virt=%.16llx %s %s %s %s %s %s %s %s %s %s, dest = %.16llx\n",
          a,b,i,
          ((unsigned long long)a<<39LL)+((unsigned long long)b<<30LL)+((unsigned long long)i<<21LL),
          entry&(1<<0) ? "P" : "p",
          entry&(1<<1) ? "W" : "R",
          entry&(1<<2) ? "U" : "S",
          entry&(1<<3) ? "PWT" : "pwt",
          entry&(1<<4) ? "PCD" : "pcd",
          entry&(1<<5) ? "A" : "a",
          entry&(1<<6) ? "D" : "d",
          entry&(1<<7) ? "PS" : "ps",
          entry&(1<<8) ? "G" : "g",
          entry&(1LL<<63) ? "XD" : "xd",              
          address); 
      } else {
        //page directory reference
        printf("   PTE PDE flags: %.3d %.3d %.3d %s %s %s %s %s %s %s %s, dest = %.16llx\n",
              a,b,i,
              entry&(1<<0) ? "P" : "p",
              entry&(1<<1) ? "W" : "R",
              entry&(1<<2) ? "U" : "S",
              entry&(1<<3) ? "PWT" : "pwt",
              entry&(1<<4) ? "PCD" : "pcd",
              entry&(1<<5) ? "A" : "a",
              entry&(1<<7) ? "PS" : "ps",
              entry&(1LL<<63) ? "XD" : "xd",              
              address);
        if(entry & 1)
        {
          dumpPT(connection_port,a,b,i,address);
        }
    }
  }
  
  free(addr);
}

void
dumpPDPT(void* connection_port, int a, uint64_t pml4e_address)
{
  int i;
  int skip = 0;
  
  uint64_t *addr = ReadPhysMem(connection_port, (void*)pml4e_address, 512*8);
  if(!addr) return;
  for(i = 0; i < 512; i++){
    uint64_t entry=addr[i];
    if(!(entry&1)) {
      skip++;
      continue;
    }
    uint64_t address = ((entry & 0x00ffffffffffffff)>>12)<<12;
    if(entry & (1<<7)){
        //1 GIG mapping
        printf("  1G PDPTE flags: %.3d %.3d | %s %s %s %s %s %s %s %s %s, dest = %.16llx\n",
              a,i,
              entry&(1<<0) ? "P" : "p",
              entry&(1<<1) ? "W" : "R",
              entry&(1<<2) ? "U" : "S",
              entry&(1<<3) ? "PWT" : "pwt",
              entry&(1<<4) ? "PCD" : "pcd",
              entry&(1<<5) ? "A" : "a",
              entry&(1<<6) ? "D" : "d",
              entry&(1<<7) ? "PS" : "ps",
              entry&(1LL<<63) ? "XD" : "xd",              
              address);
    } else {
        //page directory reference
        printf("  PDE PDPTE %.3d %.3d flags: %s %s %s %s %s %s %s %s, dest = %.16llx\n",
              a,i,
              entry&(1<<0) ? "P" : "p",
              entry&(1<<1) ? "W" : "R",
              entry&(1<<2) ? "U" : "S",
              entry&(1<<3) ? "PWT" : "pwt",
              entry&(1<<4) ? "PCD" : "pcd",
              entry&(1<<5) ? "A" : "a",
              entry&(1<<7) ? "PS" : "ps",
              entry&(1LL<<63) ? "XD" : "xd",              
              address);
        if(entry & 1)
            dumpPD(connection_port, a, i, address);
    }
  }
  free(addr);
  printf("\n");
}


void
walkPT(void* connection_port, void *pml4_addr)
{
    uint64_t* dest;
    
    dest = ReadPhysMem(connection_port, pml4_addr, 512*8);
    if(!dest) return;
    printf("--- PML4 Table ---\n");
    for(int i = 0; i < 512; i++)
    {
        uint64_t entry = dest[i];
        uint64_t address = ((entry & 0x00ffffffffffffff)>>12)<<12;
        if(!entry) continue;
        printf("PML4E %.3d flags: %s %s %s %s %s %s %s %s, dest = %.16llx\n",
                i,
                entry&(1<<0) ? "P" : "p",
                entry&(1<<1) ? "W" : "R",
                entry&(1<<2) ? "U" : "S",
                entry&(1<<3) ? "PWT" : "pwt",
                entry&(1<<4) ? "PCD" : "pcd",
                entry&(1<<5) ? "A" : "a",
                entry&(1<<7) ? "PS" : "ps",
                entry&(1LL<<63) ? "XD" : "xd",
                address);
        if(entry & 1)
        {
            dumpPDPT(connection_port, i, address);
        }
    }
    free(dest);
}


int main(int argc, char *argv[])
{
    void * pml4;
    
    fd = open("MEMORY.DMP", O_RDONLY);
    if (fd == -1)
    {
        printf("failed to open mem\n");
        return 1;
    }
    
    pml4 = (void *) strtoul(argv[1], NULL, 0);
    
    walkPT(NULL, pml4);

    return 0;
}
