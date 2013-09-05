#include <linux/types.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <asm/ioctl.h>
#include <asm/memory.h>
#include <sys/mman.h>
#include <errno.h>


static int memfd = -1;
unsigned long ttbcr, ttbr0, ttbr1, sctlr;


void * allocate(uint32_t size) {
    void * buffer = mmap(NULL, (size + 4095) & ~4095, PROT_READ|PROT_WRITE,
            MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);

    return buffer == MAP_FAILED ? NULL : buffer;
}

void deallocate(void * p, uint32_t size) {
    return (void) munmap(p, size);
}

int physread(uint32_t phys_start, void* dest, uint32_t total_size) {
    int res;

    if(!dest) {
        fputs("NULL estination buffer\n", stderr);
        return -ENOMEM;
    }

    if(memfd < 0) {
        fprintf(stderr, "Failed on physical memory fd: %i\n", memfd);
        return -EBADF;
    }

    errno = 0;
    if(lseek(memfd, phys_start, SEEK_SET), errno) {
    fprintf(stderr, "Failed to seek physical memory at %lx: %s\n",
            phys_start, strerror(errno));
        return -EIO;
    }

    if((res = read(memfd, (char *) dest, total_size)) == total_size) {
        return 0;
    }

    fprintf(stderr, "Failed to read physical memory at %lx[%i]: %s\n",
            phys_start, res, strerror(errno));
    return -EIO;
}


extern int elev8(void);

void kcode_extra(void) {
    unsigned long * d = (unsigned long *) 0xc0000000L;
    asm ( "mrc p15, 0, %0, c2, c0, 2" : "=r" (d[0]) );
    asm ( "mrc p15, 0, %0, c2, c0, 1" : "=r" (d[1]) );
    asm ( "mrc p15, 0, %0, c2, c0, 0" : "=r" (d[2]) );
    asm ( "mrc p15, 0, %0, c1, c0, 0" : "=r" (d[3]) );

}


int main(int argc, char * argv[]) {
    if(elev8() < 0) {
        puts("Could not elevate. Insert Esser joke here.");
        return 1;
    }

    memfd = open("/dev/mem", O_RDWR);
    if(memfd < 0) {
        perror("Could not open physical memory (took the stairs?)");
        return 1;
    }

    if(physread(PHYS_START, &ttbcr, 4) < 0
            || physread(PHYS_START + 4, &ttbr1, 4) < 0
            || physread(PHYS_START + 8, &ttbr0, 4) < 0
            || physread(PHYS_START + 12, &sctlr, 4) < 0) {
        return 1;
    }

    printf("ttbcr: %x\nttbr0: %x\nttbr1: %x\nsctlr: %x\n",
            ttbcr, ttbr0, ttbr1, sctlr);
    
    armPtPrint(ttbcr, ttbr0, ttbr1);

    close(memfd);

    memfd = open("/proc/self/maps", O_RDONLY);
    for(;;) {
        char buf[1024];
        int res = read(memfd, buf, sizeof(buf));

        if(res <= 0) {
            break;
        }

        write(1, buf, res);
    }
    close(memfd);

    return 0;
}
