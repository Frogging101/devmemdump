from __future__ import print_function
import sys
import os
import re
import argparse
import os.path
import random

include = ['System RAM',
           'System ROM']
argparser = argparse.ArgumentParser()
argparser.add_argument("-b","--buffers", help="Include buffers", action="store_true")
argparser.add_argument("-k","--kernel-stuff", help="Include kernel stuff", action="store_true")
argparser.add_argument("-f","--file", help="Dump to file", dest='outfile')
argparser.add_argument("bytes", type=str, nargs='?',
                       help="How many bytes to fuck up. Can be a range as in \"4000-8000\" (the default)",
                       default="null")

args = argparser.parse_args()

if args.buffers:
    include.append("RAM buffer")
if args.kernel_stuff:
    include.append("Kernel code")
    include.append("Kernel data")
    include.append("Kernel bss")

class IOMemBlock:
    def __init__(self):
        self.name = None
        self.start = None
        self.end = None

    def __str__(self):
        return self.name+':\t'+hex(self.start)+'-'+hex(self.end)

iomemf = open('/proc/iomem','r')
iomem = iomemf.readlines()
iomemf.close()

bytesLower = 4000
bytesUpper = 8000

rangePat = r"(\d+)-(\d+)"
m = re.match(rangePat, args.bytes)
if m:
    bytesLower = int(m.group(1))
    bytesUpper = int(m.group(2))
else:
    try:
        bytesLower = int(args.bytes)
        bytesUpper = bytesLower
    except ValueError:
        pass

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

commands = []
ddtmpl = "dd if=/dev/urandom of=/dev/mem bs=512 seek={} count={} oflag=seek_bytes iflag=count_bytes"

chosenblock = random.randint(0,len(blocks)-1)
chosenstart = max(0,blocks[chosenblock].start)
chosenend = blocks[chosenblock].end

chosenOffset = random.randint(chosenstart,chosenend)
chosenCount = random.randint(bytesLower,bytesUpper)

class Process:
    def __init__(self, pid):
        self.pid = pid
        self.name = None
        self.maps = []

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

hits = []
for proc in processes:
    for map_ in proc.maps:
        if chosenOffset <= map_[1] and\
           map_[0] <= chosenOffset+chosenCount:
               hits.append((proc, map_))
print("Block: "+blocks[chosenblock].name+" ("+hex(chosenOffset)+'-'+hex(chosenOffset+chosenCount)+')', file=sys.stderr)
print("Hitting:", file=sys.stderr)
for hit in hits:
    print(hit[0].name+'['+hit[0].pid+']: '+hit[1][2]+" ("+hex(hit[1][0])+'-'+hex(hit[1][1])+')', file=sys.stderr)

command = ddtmpl.format(chosenOffset,chosenCount)

print(command)
