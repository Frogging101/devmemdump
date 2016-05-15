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

#define MAX_STR 32
#define MAX_BUF 512
#define INCLUDE_SIZE 10

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define PAGEMAP_BLOCK 8

typedef struct OptionsType {
    int inc_kernelStuff;
    int inc_buffers;
    char targetProc[MAX_STR];
    int targetPID;
    int targetLibs;
} Options;

typedef struct MemBlockType {
    char name[MAX_STR];
    uint64_t start;
    uint64_t end;
} MemBlock;
typedef struct MemBlockType Map;

typedef struct ArrayType {
    void *arr;
    int size;
    int capacity;
} ArrayType;

typedef struct ProcessType {
    int pid;
    char *basename;
    char *cmdLine;
    ArrayType maps;
} Process;

void initOptions(Options *opts);
void parseArgs(Options *opts, int argc, char **argv);
void getProcesses(ArrayType *processes);
void getProcessMaps(ArrayType *maps, int pid);

void getProcessByPID(ArrayType *processes, int pid, Process **proc);

uint64_t virtToPhys(uint64_t virtAddr, int pid);
uint64_t randRange(uint64_t min, uint64_t max);

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

    const char *blkPat = "\\s*([0-9a-f]+)-([0-9a-f]+) : (.*)$";
    regex_t blkProg;
    int regResult = regcomp(&blkProg, blkPat, REG_EXTENDED|REG_NEWLINE);
    if(regResult != 0) {
        printf("regcomp() returned %d\n", regResult);
        exit(1);
    }

    FILE *fd_iomem = fopen("/proc/iomem", "r");
    if(!fd_iomem) {
        printf("Failed to open /proc/iomem\n");
        exit(1);
    }

    int done = 0;
    ArrayType blocks;
    blocks.size = 0;
    blocks.capacity = 16;
    blocks.arr = calloc(16, sizeof(MemBlock));
    MemBlock *blocksArr = ((MemBlock *) blocks.arr);
    while(!done) {
        char *line = (char *) calloc(MAX_BUF, sizeof(char));
        while(1) {
            if(fgets(line, MAX_BUF, fd_iomem) == NULL) {
                done = 1;
                break;
            }
            if(line[strlen(line)-1] == '\n')
                break;
            line = (char *) realloc(line, (strlen(line)+MAX_BUF)*sizeof(char));
        }
        if(done)
            break;
        regmatch_t pmatch[4];
        regResult = regexec(&blkProg, line, 4, pmatch, 0);
        if(regResult != 0) {
            printf("Failed to match:\n%s\n", line);
            free(line);
            free(blocks.arr);
            exit(1);
        }

        MemBlock addBlock;

        memcpy(addBlock.name, line+pmatch[3].rm_so,
               pmatch[3].rm_eo-pmatch[3].rm_so);
        addBlock.name[pmatch[3].rm_eo-pmatch[3].rm_so] = '\0';

        int included = 0;
        for(int i=0; i<INCLUDE_SIZE; i++) {
            if(include[i] == NULL)
                break;
            if(strcmp(addBlock.name, include[i]) == 0) {
                included = 1;
                break;
            }
        }
        if(!included) {
            //free(line);
            continue;
        }

        char startStr[MAX_STR];
        char endStr[MAX_STR];
        memcpy(startStr, line+pmatch[1].rm_so,
               pmatch[1].rm_eo-pmatch[1].rm_so);
        memcpy(endStr, line+pmatch[2].rm_so,
               pmatch[2].rm_eo-pmatch[2].rm_so);
        startStr[pmatch[1].rm_eo-pmatch[1].rm_so] = '\0';
        endStr[pmatch[2].rm_eo-pmatch[2].rm_so] = '\0';
        sscanf(startStr, "%" PRIx64, &addBlock.start);
        sscanf(endStr, "%" PRIx64, &addBlock.end);

        if(blocks.size+1 > blocks.capacity) {
            blocks.arr = realloc(blocks.arr, (blocks.capacity+8)*sizeof(MemBlock));
            blocks.capacity += 8;
        }
        blocksArr[blocks.size] = addBlock;

        blocks.size++;
    }
    fclose(fd_iomem);
    ArrayType processes;
    getProcesses(&processes);
    uint64_t targetWidth = randRange(4000,8000); //TODO: Argument
    uint64_t targetAddr = 0;
    if(opts.targetPID || strcmp(opts.targetProc, "") != 0) {
        Process *target = NULL;
        if(opts.targetPID) {
            getProcessByPID(&processes, opts.targetPID, &target);
        } else {
            for(int i=0; i<processes.size; i++) {
                Process *curProc = &((Process *)processes.arr)[i];
                if(strcmp(curProc->cmdLine, opts.targetProc) == 0 ||
                   strcmp(curProc->basename, opts.targetProc) == 0) {
                    //target = curProc.pid;
                    target = curProc;
                }
            }
        }
        ArrayType possibleMaps = target->maps;
        Map *possibleMapsArr = (Map *) possibleMaps.arr;
        if(!opts.targetLibs) {
            possibleMaps.arr = malloc(target->maps.size*sizeof(Map));
            possibleMaps.capacity = target->maps.size;
            possibleMaps.size = 0;
            Map *targetMapsArr = (Map *) target->maps.arr;
            for(int i=0; i<target->maps.size; i++) {
                if(targetMapsArr[i].name[0] != '/') {
                    possibleMapsArr[possibleMaps.size] = targetMapsArr[i];
                    possibleMaps.size++;
                }
            }
        }
        Map targetMap = possibleMapsArr[randRange(0,possibleMaps.size)];
        uint64_t targetOffset = 0*randRange(0, targetMap.end-targetMap.start);
        targetAddr = virtToPhys(targetMap.start, target->pid)+targetOffset;
        printf("Targeting %x in %s[%d]: %s (%x-%x)\n", targetMap.start+targetOffset,
               target->basename, target->pid, targetMap.name, targetMap.start,
               targetMap.end);
        printf("Physical address: %x\n", targetAddr-targetOffset);
    } else {
        MemBlock targetBlock = blocksArr[randRange(0,blocks.size)];
        targetAddr = randRange(targetBlock.start, targetBlock.end);
        printf("Targeting %x in block %s (%x-%x)\n", targetAddr,
               targetBlock.name, targetBlock.start, targetBlock.end);
    }

    char *ddtmpl = "dd if=/dev/urandom of=/dev/mem bs=512 seek=%llu count=%llu"
                   " oflag=seek_bytes iflag=count_bytes\n";
    printf(ddtmpl, targetAddr, targetWidth);
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

/*
int randRange(unsigned int max) {
    randRange(0, max);
}*/

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

void getProcesses(ArrayType *processes) {
    processes->arr = calloc(64, sizeof(Process));
    processes->capacity = 64;
    processes->size = 0;

    DIR *procDir = opendir("/proc");
    struct dirent *dent = NULL;
    while((dent = readdir(procDir)) != NULL) {
        int pid = atoi(dent->d_name);
        if(pid == 0)
            continue;

        Process newProc;
        newProc.pid = pid;

        //char pidPath[16];
        char cmdLinePath[24];
        sprintf(cmdLinePath, "/proc/%d/cmdline", pid);
        FILE *cmdLineFd = fopen(cmdLinePath, "r"); //TODO: I forget
        char cmdLine[4096]; //TODO: PATH_MAX

        if(fgets(cmdLine, 4096, cmdLineFd) == NULL)
            continue;
        fclose(cmdLineFd);
        
        char *basename = strrchr(cmdLine, '/');
        if(basename != NULL)
            basename++;
        else
            basename = cmdLine;
        newProc.basename = (char *) calloc(strlen(basename)+1, sizeof(char));
        newProc.cmdLine = (char *) calloc(strlen(cmdLine)+1, sizeof(char));
        strcpy(newProc.basename, basename);
        strcpy(newProc.cmdLine, cmdLine);
        if(processes->size+1 > processes->capacity) {
            processes->arr = realloc(processes->arr,
                                     (processes->capacity+64)*sizeof(Process));
            processes->capacity += 64;
        }

        getProcessMaps(&newProc.maps, newProc.pid);

        Process *processArr = (Process *) processes->arr;
        processArr[processes->size] = newProc;
        processes->size++;
    }
}

void getProcessMaps(ArrayType *maps, int pid) {
    maps->arr = calloc(32, sizeof(Map));
    maps->size = 0;
    maps->capacity = 32;

    char mapsPath[24];
    sprintf(mapsPath, "/proc/%d/maps", pid);
    FILE *fd_maps = fopen(mapsPath, "r");
    int done = 0;

    while(!done) {
        char *line = (char *) calloc(MAX_BUF, sizeof(char));
        while(1) {
            if(fgets(line, MAX_BUF, fd_maps) == NULL) {
                done = 1;
                break;
            }
            if(line[strlen(line)-1] != '\n') {
                line = realloc(line, (strlen(line)+MAX_BUF)*sizeof(char));
            } else {
                break;
            }
        }
        if(done)
            break;

        //char *pat_maps = "([A-Fa-f0-9]+)-([A-Fa-f0-9]+)"
        //                 "\\s+[^\\s]+\\s+[^\\s]+\\s+[^\\s]+\\s+[^\\s]+\\s*([^\\s]+)?";
        char *pat_maps = "([A-Fa-f0-9]+)-([A-Fa-f0-9]+)"
                         "[[:space:]]+[^[:space:]]+"
                         "[[:space:]]+[^[:space:]]+"
                         "[[:space:]]+[^[:space:]]+"
                         "[[:space:]]+[^[:space:]]+"
                         "[[:space:]]*([^[:space:]]+)?";
        regex_t prog_maps;
        int regResult = regcomp(&prog_maps, pat_maps, REG_EXTENDED);
        if(regResult != 0) {
            printf("regcomp() returned %d\n", regResult);
            //regfree(prog_maps);
            free(line);
            exit(1);
        }
        regmatch_t pmatch[4];

        regResult = regexec(&prog_maps, line, 4, pmatch, 0);
        if(regResult != 0) {
            printf("Failed to match:\n%s\n", line);
            regfree(&prog_maps);
            free(line);
            exit(1);
        }

        Map newMap;
        newMap.name[0] = '\0';
        if(pmatch[3].rm_so != -1) {
            memcpy(newMap.name, line+pmatch[3].rm_so,
                   pmatch[3].rm_eo-pmatch[3].rm_so);
            newMap.name[pmatch[3].rm_eo-pmatch[3].rm_so] = '\0';
        }
        char startStr[MAX_STR];
        char endStr[MAX_STR];
        memcpy(startStr, line+pmatch[1].rm_so, pmatch[1].rm_eo-pmatch[1].rm_so);
        startStr[pmatch[1].rm_eo-pmatch[1].rm_so] = '\0';
        memcpy(endStr, line+pmatch[2].rm_so, pmatch[2].rm_eo-pmatch[2].rm_so);
        endStr[pmatch[2].rm_eo-pmatch[2].rm_so] = '\0';
        sscanf(startStr, "%" PRIx64, &newMap.start);
        sscanf(endStr, "%" PRIx64, &newMap.end);

        if(maps->size+1 > maps->capacity) {
            maps->arr = realloc(maps->arr, (maps->capacity+32)*sizeof(Map));
            maps->capacity += 32;
        }
        Map *mapsArr = (Map *) maps->arr;
        mapsArr[maps->size] = newMap;
        maps->size++;
    }
}

void getProcessByPID(ArrayType *processes, int pid, Process **proc) {
    for(int i=0; i<processes->size; i++) {
        Process *processArr = (Process *) processes->arr;
        if(processArr[i].pid == pid) {
            *proc = &processArr[i];
            return;
        }
    }
    *proc = NULL;
}
