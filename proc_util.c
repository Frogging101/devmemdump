#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <dirent.h>
#include <string.h>

#include "fuckdevmem.h"

void getMemBlocks(char include[10][MAX_STR], ArrayType *blocks) {
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
    blocks->size = 0;
    blocks->capacity = 24;
    blocks->arr = calloc(24, sizeof(MemBlock));
    MemBlock *blocksArr = ((MemBlock *) blocks->arr);
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
            fclose(fd_iomem);
            free(line);
            free(blocks->arr);
            exit(1);
        }

        MemBlock addBlock;

        memcpy(addBlock.name, line+pmatch[3].rm_so,
               pmatch[3].rm_eo-pmatch[3].rm_so);
        addBlock.name[pmatch[3].rm_eo-pmatch[3].rm_so] = '\0';

        int included = 0;
        for(int i=0; i<10; i++) {
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

        if(blocks->size+1 > blocks->capacity) {
            blocks->arr = realloc(blocks->arr, (blocks->capacity+8)*sizeof(MemBlock));
            blocks->capacity += 8;
        }
        blocksArr[blocks->size] = addBlock;

        blocks->size++;
    }
    fclose(fd_iomem);
}

Process *getProcessByPID(ArrayType *processes, int pid, Process *proc) {
    for(int i=0; i<processes->size; i++) {
        Process *curProc = &((Process *) processes->arr)[i];
        if(curProc->pid == pid) {
            *proc = *curProc;
            return proc;
        }
    }
    return NULL;
}

Process *getProcessByName(ArrayType *processes, char *name, Process *proc) {
    for(int i=0; i<processes->size; i++) {
        Process *curProc = &((Process *)processes->arr)[i];
        if(strcmp(curProc->cmdLine, name) == 0 ||
           strcmp(curProc->basename, name) == 0) {
            *proc = *curProc;
            return proc;
        }
    }
    return NULL;
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

        getProcessMaps(newProc.pid, &newProc.maps);

        Process *processArr = (Process *) processes->arr;
        processArr[processes->size] = newProc;
        processes->size++;
    }
}

void getProcessMaps(int pid, ArrayType *maps) {
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
