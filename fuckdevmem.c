#define _BSD_SOURCE
#define _POSIX_C_SOURCE 199309L
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <regex.h>

#include <sys/types.h>
#include <dirent.h>

#include <time.h>

#include "fuckdevmem.h"

void initOptions(Options *opts);
void parseArgs(Options *opts, int argc, char **argv);

int main(int argc, char **argv) {
    struct timespec monoTime;
    clock_gettime(CLOCK_MONOTONIC, &monoTime);
    unsigned int seedTime = monoTime.tv_sec*1000+(monoTime.tv_nsec/1000000);
    printf("Seeding RNG with %u\n", seedTime);
    srandom(seedTime);

    char include[10][MAX_STR];
    memset(include, 0, 10*MAX_STR*sizeof(char));
    strcpy(include[0], "System RAM");
    strcpy(include[1], "System ROM");

    Options opts;
    initOptions(&opts);
    parseArgs(&opts, argc, argv);

    if(opts.inc_kernelStuff) {
        strcpy(include[2], "Kernel code");
        strcpy(include[3], "Kernel data");
        strcpy(include[4], "Kernel bss");
    }
    if(opts.inc_buffers)
        strcpy(include[5], "RAM buffer");

    ArrayType blocks;
    getMemBlocks(&include[0], &blocks);
    MemBlock *blocksArr = (MemBlock *) blocks.arr;
    ArrayType processes;
    getProcesses(&processes);
    uint64_t targetWidth = randRange(4000,8000); //TODO: Argument
    uint64_t targetAddr = 0;
    if(opts.targetPID || strcmp(opts.targetProc, "") != 0) {
        Process target;
        target.pid = 0;
        if(opts.targetPID) {
            getProcessByPID(&processes, opts.targetPID, &target);
        } else {
            getProcessByName(&processes, opts.targetProc, &target);
        }
        if(target.pid == 0) {
            printf("Specified process could not be found.\n");
            exit(1);
        }
        ArrayType possibleMaps = target.maps;
        Map *possibleMapsArr = (Map *) possibleMaps.arr;
        if(!opts.targetLibs) {
            possibleMaps.arr = malloc(target.maps.size*sizeof(Map));
            possibleMaps.capacity = target.maps.size;
            possibleMaps.size = 0;
            Map *targetMapsArr = (Map *) target.maps.arr;
            for(int i=0; i<target.maps.size; i++) {
                if(targetMapsArr[i].name[0] != '/') {
                    possibleMapsArr[possibleMaps.size] = targetMapsArr[i];
                    possibleMaps.size++;
                }
            }
        }
        Map targetMap = possibleMapsArr[randRange(0,possibleMaps.size-1)];
        uint64_t targetOffset = randRange(0, targetMap.end-targetMap.start);
        targetAddr = virtToPhys(targetMap.start, target.pid)+targetOffset;
        printf("Targeting %x in %s[%d]: %s (%x-%x)\n", targetMap.start+targetOffset,
               target.basename, target.pid, targetMap.name, targetMap.start,
               targetMap.end);
        printf("Physical address: %x\n", targetAddr-targetOffset);
    } else {
        MemBlock targetBlock = blocksArr[randRange(0,blocks.size-1)];
        targetAddr = randRange(targetBlock.start, targetBlock.end);
        printf("Targeting %x in block %s (%x-%x)\n", targetAddr,
               targetBlock.name, targetBlock.start, targetBlock.end);
    }

    char *ddOutTemplate = "dd if=/dev/urandom of=/dev/mem bs=512 seek=%llu count=%llu"
                   " oflag=seek_bytes iflag=count_bytes\n";
    char *ddInTemplate = "dd if=/dev/mem bs=512 skip=%llu count=%llu"
                         " iflag=skip_bytes,count_bytes | xxd | less\n";
    printf(ddOutTemplate, targetAddr, targetWidth);
    printf(ddInTemplate, targetAddr, targetWidth);
}

void initOptions(Options *opts) {
    opts->inc_kernelStuff = 0;
    opts->inc_buffers = 0;
    opts->targetProc[0] = '\0';
    opts->targetPID = 0;
    opts->targetLibs = 0;
}

void parseArgs(Options *opts, int argc, char **argv) {
    struct option opt_kernelStuff;
    opt_kernelStuff.name = "kernel-stuff";
    opt_kernelStuff.has_arg = 0;
    opt_kernelStuff.flag = NULL; //&opts->inc_kernelStuff;
    opt_kernelStuff.val = 'K'; //1;

    struct option opt_buffers;
    opt_buffers.name = "buffers";
    opt_buffers.has_arg = 0;
    opt_buffers.flag = NULL; //&opts->inc_buffers;
    opt_buffers.val = 'b'; //1;

    struct option opt_targetLibs;
    opt_targetLibs.name = "process-libs";
    opt_targetLibs.has_arg=0;
    opt_targetLibs.flag = NULL;
    opt_targetLibs.val = 'L';

    const struct option longopts[5] = {opt_kernelStuff, opt_buffers, 
                                       opt_targetLibs};
    const char *optstring = "bKLk:p:";

    int doneParsing = 0;
    while(!doneParsing) {
        int opt = getopt_long(argc, argv, optstring, longopts, NULL);
        switch(opt) {
            case 'b':
                opts->inc_buffers = 1;
                break;
            case 'K':
                opts->inc_kernelStuff = 1;
                break;
            case 'L':
                opts->targetLibs = 1;
                break;
            case 'k':
                opts->targetPID = atoi(optarg);
                break;
            case 'p':
                strcpy(opts->targetProc, optarg);
                break;
            case -1:
                doneParsing = 1;
                break;
        }
    }
}

