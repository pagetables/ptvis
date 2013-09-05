#include <stdio.h>
#include <stdlib.h>
#include <IOKit/IOKitLib.h>
#include <string.h>
#define kMyDriversIOKitClassName 	"ptdump_Driver"

struct ptregs
{
    uint64_t cr0,cr1,cr2,cr3,cr4;
    uint64_t efer_a, efer_d;
    uint32_t acip_id;
};

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

void*
ReadPhysMem(io_connect_t connection_port, void *address, uint64_t size)
{
    uint64_t input[16];
    uint32_t inputCnt=0,inputStructCnt=0;
    uint8_t *inputStruct=NULL;
    void *outputStruct = NULL;
    size_t outputStructCnt = 0;
    uint64_t *output = NULL; 
    uint32_t outputCnt=0;
    uint32_t selector=0;
    
    if ((uint64_t)address == ((0x00ffffffffffffff)>>12)<<12)
    {
        printf("Invalid address: %p", address);
        return NULL;
    }

    selector = 1;
    inputCnt = 2;
    input[0] = (uint64_t)address;
    input[1] = size;
    output =  (uint64_t *)calloc(16, 8);
    outputCnt = 2;
    
  int kr = 0;
  kr = IOConnectCallMethod(connection_port, selector, input, inputCnt, inputStruct, inputStructCnt, output, &outputCnt, outputStruct, &outputStructCnt);
  
  char *buf = malloc(size);
  memcpy(buf, (void*)output[0], size);
  return buf;
}

void
dumpPT(io_connect_t connection_port, int a, int b, int c, uint64_t pt_addr)
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
dumpPD(io_connect_t connection_port, int a, int b, uint64_t pd_addr){
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
dumpPDPT(io_connect_t connection_port, int a, uint64_t pml4e_address)
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
walkPT(io_connect_t connection_port, void *pml4_addr)
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

int connect(io_connect_t *ConnectionPort)
{
  CFDictionaryRef	classToMatch;
  mach_port_t		masterPort;
  kern_return_t	kernResult; 
  io_service_t	serviceObject;
  io_iterator_t 	iterator;

  kernResult = IOMasterPort(MACH_PORT_NULL, &masterPort);    
  
  if (kernResult != KERN_SUCCESS)
  {
      printf( "Error: IOMasterPort returned %d\n", kernResult);
      return -1;
  }
  
  classToMatch = IOServiceMatching(kMyDriversIOKitClassName);
  
  if (classToMatch == NULL)
  {
      printf( "  Error: IOServiceMatching returned a NULL dictionary.\n");
      return -1;
  }
  
  kernResult = IOServiceGetMatchingServices(masterPort, classToMatch, &iterator);
  
  if (kernResult != KERN_SUCCESS)
  {
      printf( "  Error: IOServiceGetMatchingServices returned %d\n\n", kernResult);
      return -1;
  }
  
	while ((serviceObject = IOIteratorNext(iterator)))
	{
		io_name_t srvName;
		IORegistryEntryGetName(serviceObject, srvName);
        break;
    }

  IOObjectRelease(iterator);
  
  if (!serviceObject)
  {
      printf( "Error: Couldn't find any matches.\n");
      return -1;
  }
  
  
  kernResult = IOServiceOpen(serviceObject, mach_task_self(), 0, ConnectionPort);
  
  IOObjectRelease(serviceObject);
  
  if (kernResult != KERN_SUCCESS)
  {
      printf( "Error: IOServiceOpen returned %d\n", kernResult);
      return -1;
  }

  return 0;	
}

int main(int argc, char *argv[]){
  io_connect_t connection_port;
  if(0 != connect(&connection_port)) {
    printf("failed to connect\n");
    exit(0);
  }
  printf("Connected\n");

  uint64_t input[16];
  uint32_t inputCnt=0,inputStructCnt=0;
  uint8_t *inputStruct=NULL;
  void *outputStruct = NULL;
  size_t outputStructCnt = 0;
  uint64_t *output = NULL; 
  uint32_t outputCnt=0;
  uint32_t selector=0;
  
  if(!argv[1]){
    printf("Missing arguments\n");
    return 1;
  } else if(!strcmp(argv[1], "dumpregisters")) {
    selector = 0;
    outputStruct =  (uint64_t *)calloc(4096, 1);
    outputStructCnt = 4096;
  } else if(!strcmp(argv[1], "physmem")) {
    selector = 1;
    inputCnt = 2;
    input[0] = strtoull(argv[2],NULL,0);
    input[1] = strtoull(argv[3],NULL,0);
    output =  (uint64_t *)calloc(16, 8);
    outputCnt = 2;
  } else if(!strcmp(argv[1], "virtmem")) {
    selector = 2;
    inputCnt = 2;
    input[0] = strtoull(argv[2],NULL,0);
    input[1] = strtoull(argv[3],NULL,0);
    output =  (uint64_t *)calloc(16, 8);
    outputCnt = 2;
  } else if(!strcmp(argv[1], "walkpt"))
  {
    uint64_t pml4 = 0;
    if (!argv[2])
    {
        selector = 0;
        outputStruct =  (uint64_t *)calloc(4096, 1);
        outputStructCnt = 4096;
      int kr = 0;
      kr = IOConnectCallMethod(connection_port, selector, input, inputCnt, inputStruct, inputStructCnt, output, &outputCnt, outputStruct, &outputStructCnt);
    
        pml4 = (((struct ptregs*)outputStruct)->cr3>>12)<<12;
    }
    else {
        pml4 = strtoull(argv[2], NULL, 0);
    }
    walkPT(connection_port, (void*)pml4);
    return 0;
  } else if(!strcmp(argv[1],"walkthreads"))
  {
    selector = 3;
    outputStruct =  (uint64_t *)calloc(4096, 1);
    outputStructCnt = 4096;
  }
  else {
    printf("unknown command\n");
    return 1;
  }  

  int kr = 0;
  kr = IOConnectCallMethod(connection_port, selector, input, inputCnt, inputStruct, inputStructCnt, output, &outputCnt, outputStruct, &outputStructCnt);

  if (output != NULL)
  {
    printf("%x, got output address= [%llx %llx] %d\n", kr,  output[0], output[1], outputCnt);
  }
  
  if (output && output[0])
  {
    printf("Got %lld bytes\n", output[1]);
      for (int i = 0; i < output[1]; i++)
      {
        printf("%.2x", ((char *)(output[0]))[i] &0xff);
      }
      printf("\nDone\n");
  }
  
  if (outputStructCnt)
  {
    if (selector == 0)
    {
        handle_regs((struct ptregs*)outputStruct);
    }
    else if (selector == 3)
    {
        uint64_t *out = outputStruct;
        for(int i = 1; i < out[0]; i++)
        {
            printf("pml4 physbase 0x%.16llx\n",out[i]);
        }
    }
  }

 // dumpPML4((uint64_t *)output[0]);

  printf("Closed\n");

  return 0;
}
