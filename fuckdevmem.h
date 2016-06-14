#define MAX_STR 32
#define MAX_BUF 512

typedef struct OptionsType {
    int inc_kernelStuff;
    int inc_buffers;
    char targetProc[MAX_STR];
    char specRegions[128];
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

void getMemBlocks(char include[10][MAX_STR], ArrayType *blocks);

void getProcesses(ArrayType *processes);
void getProcessMaps(int pid, ArrayType *maps);
Process *getProcessByPID(ArrayType *processes, int pid, Process *proc);
Process *getProcessByName(ArrayType *processes, char *name, Process *proc);

uint64_t randRange(uint64_t min, uint64_t max);
uint64_t virtToPhys(uint64_t virtAddr, int pid);
