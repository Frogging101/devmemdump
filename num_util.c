#define _BSD_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

#include "fuckdevmem.h"

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define PAGEMAP_BLOCK 8

uint64_t randRange(uint64_t min, uint64_t max) {
    uint64_t numRandom = RAND_MAX;
    numRandom += 1;
    uint64_t rangeSize = max-min+1;
    uint64_t numBuckets = rangeSize;
    uint64_t bucketSize = numRandom/numBuckets;
    uint64_t error = numRandom % numBuckets;

    long int x = random();
    while(x >= numRandom-error)
        x = random();

    /*printf("Random number from %llu to %llu: %li\n", min, max,
           (x/bucketSize)+min);
    printf("x: %li, buckets: %llu, bucket size: %llu, RAND_MAX: %llu\n",
           x, numBuckets, bucketSize, RAND_MAX);*/
    return (x/bucketSize)+min;
}

uint64_t virtToPhys(uint64_t virtAddr, int pid) {
    char pmPath[24];
    uint64_t pmIndex = virtAddr/PAGE_SIZE*PAGEMAP_BLOCK;
    printf("pmIndex (dec): %llu\n", pmIndex);
    sprintf(pmPath, "/proc/%d/pagemap", pid);
    FILE *pmfd = fopen(pmPath, "rb");
    
    uint64_t pmInfoBytes = 0;
    fseek(pmfd, pmIndex, SEEK_SET);
    fread(&pmInfoBytes, 1, 8, pmfd);
    fclose(pmfd);

    unsigned char pmStorageInfo = pmInfoBytes;

    uint64_t pmPFNMask = 0xffffffffffffffff >> 9;
    uint64_t pmPFN = pmInfoBytes & pmPFNMask;
    printf("PFN: %" PRIu64 "\n", pmPFN);

    uint64_t phyAddr = pmPFN << PAGE_SHIFT;
    return phyAddr;
}

