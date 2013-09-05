#include <linux/types.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <asm/ioctl.h>
#include <asm/memory.h>
#include <sys/mman.h>
#include <errno.h>


#define DEVMEM_IOCTL_MAGIC 'V'

#define DEVMEM_IOCTL_TTBR0      _IO(DEVMEM_IOCTL_MAGIC, 1)
#define DEVMEM_IOCTL_TTBR1      _IO(DEVMEM_IOCTL_MAGIC, 2)
#define DEVMEM_IOCTL_TTBCR      _IO(DEVMEM_IOCTL_MAGIC, 3)
#define DEVMEM_IOCTL_TTBR0_ALL  _IO(DEVMEM_IOCTL_MAGIC, 4)
#define DEVMEM_IOCTL_SCTLR      _IO(DEVMEM_IOCTL_MAGIC, 5)



static int memfd = -1;
static unsigned long ttbcr, ttbr0, ttbr1;

uint32_t sctlr;


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


static void * dump_all_ttbr0(int memfd) {
    unsigned long size = 0x10000L;
    int k, processes;
    char * base, * p;

    do {
        base = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);

        if(base == MAP_FAILED) {
            perror("Could not allocate buffer for all ttbr0");
            return NULL;
        }

        processes = ioctl(memfd, DEVMEM_IOCTL_TTBR0_ALL, base);

        if(processes >= 0) {
            break;
        }

        munmap(base, size);
        size <<= 1;

        printf("%lx was too small, trying %lx\n", size >> 1, size);
    } while(size < 0x80000L);

    if(processes < 0) {
        perror("Could not dump all ttbr0");
        return NULL;
    }

    p = base;
    for(k = 0; k < processes; ++k, p += sizeof(unsigned long) + 16) {
        if(!(* (unsigned long *) p)) {
            continue;
        }

        fprintf(stdout, "<<%-16s: %08lx>>\n", p + sizeof(unsigned long),
                * (unsigned long *) p);
       
        armPtPrint(ttbcr, * (unsigned long *) p, ttbr1);
    }

    munmap(base, size);
    return NULL;
}

int main(int argc, char * argv[]) {
    memfd = open("/dev/mem", O_RDWR);
    if(memfd < 0) {
        perror("Could not open physical memory (not root?)");
        return 1;
    }

    ttbcr = ioctl(memfd, DEVMEM_IOCTL_TTBCR);
    ttbr0 = ioctl(memfd, DEVMEM_IOCTL_TTBR0);
    ttbr1 = ioctl(memfd, DEVMEM_IOCTL_TTBR1);
    sctlr = ioctl(memfd, DEVMEM_IOCTL_SCTLR);

    printf("ttbcr: %x\nttbr0: %x\nttbr1: %x\nsctlr: %x\n",
            ttbcr, ttbr0, ttbr1, sctlr);

    dump_all_ttbr0(memfd);

    close(memfd);
    return 0;
}
