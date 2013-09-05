#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

void armPtPrint(uint32_t ttbcr, uint32_t ttbr0, uint32_t ttbr1);

uint32_t sctlr = 0x70c5187d;

int fd;

/*
 TODO: need to figure out how to get these from the crash dump file,
        these were extracted from windbg for a specific MEMORY.DMP file
*/
int offsets[][3] = {
{ 0x82006000 , 0x21000 , 0x7a406000 },
{ 0xfd40c000 , 0x7a427000 , 0x12c000 },
{ 0xfd53a000 , 0x7a553000 , 0x3000 },
{ 0xfd53e000 , 0x7a556000 , 0x27000 },
{ 0xfd566000 , 0x7a57d000 , 0x69000 },
{ 0xfd5d2000 , 0x7a5e6000 , 0xb2000 },
{ 0xfd693000 , 0x7a698000 , 0xa9000 },
{ 0xfd7bc000 , 0x7a741000 , 0x12d000 },
{ 0xfd8f5000 , 0x7a86e000 , 0x1b000 },
{ 0xfd912000 , 0x7a889000 , 0x1000 },
{ 0xfd917000 , 0x7a88a000 , 0x1000 },
{ 0xfd919000 , 0x7a88b000 , 0x9cb000 },
{ 0xfe2e5000 , 0x7b256000 , 0xd84000 },
{ 0xff0e1000 , 0x7bfda000 , 0x71b000 },
{ 0, 0, 0 },
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

//
// mem allocation, again impl specific
//
void *allocate(uint32_t size)
{
    return malloc(size);
}
void deallocate(void * p, uint32_t size)
{
    free(p);
}


int main(int argc, char *argv[])
{
    fd = open("MEMORY.DMP", O_RDONLY);

    if (fd == -1)
    {
        printf("failed to open dump file\n");
        return 1;
    }
    int size = strtoul(argv[2],0,0);
    char *dest = allocate(size);

    physread(strtoul(argv[1],0,0), dest, size);
    
    int i;
    for(i = 0; i < 4096; i++)
    {
        printf("%.2x", dest[i] & 0xff);
    }

    deallocate(dest, size);
    
    close(fd);

    return 0;
}
