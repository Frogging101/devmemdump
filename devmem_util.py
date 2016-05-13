import re
import os

class IOMemBlock:
    def __init__(self):
        self.name = None
        self.start = None
        self.end = None

    def __str__(self):
        return self.name+':\t'+hex(self.start)+'-'+hex(self.end)

class Process:
    def __init__(self, pid):
        self.pid = pid
        self.name = None
        self.maps = []

def getBlocks(include):
    blocks = []

    iomemf = open('/proc/iomem','r')
    iomem = iomemf.readlines()
    iomemf.close()

    blkPat = "^\s*([0-9a-f]+)-([0-9a-f]+) : (.*)$"
    blkProg = re.compile(blkPat,re.M)
    blocks = []
    for line in iomem:
        blk = IOMemBlock()
        m = blkProg.match(line)
        blk.name = m.group(3)
        if blk.name not in include:
            continue
        blk.start = int(m.group(1),16)
        blk.end = int(m.group(2),16);
        blocks.append(blk)
    return blocks

def getProcs():
    processes = []
    pids = [pid for pid in os.listdir('/proc') if pid.isdigit()]

    mapsPat = r"([A-Fa-f0-9]+)-([A-Fa-f0-9]+)\s+[^\s]+\s+[^\s]+\s+[^\s]+\s+[^\s]+\s*([^\s]+)?"
    mapsProg = re.compile(mapsPat)

    for pid in pids:
        try:
            newProcess = Process(pid)
            with open(os.path.join('/proc', pid, 'comm'), 'r') as commFile:
                newProcess.name = commFile.readline().rstrip()

            with open(os.path.join('/proc', pid, 'maps'), 'r') as mapsFile:
                lines = mapsFile.readlines()
                for line in lines:
                    m = mapsProg.match(line)
                    mapName = ''
                    if m.group(3):
                        mapName = m.group(3)
                    mapStart = m.group(1)
                    mapEnd = m.group(2)
                    mapTuple = (int(mapStart,16), int(mapEnd, 16), mapName)
                    newProcess.maps.append(mapTuple)
            processes.append(newProcess)
        except IOError:
            continue
    return processes
