from __future__ import print_function
import re
import sys
import argparse
import random

import devmem_util
from devmem_util import IOMemBlock

include = ['System RAM',
           'System ROM']
argparser = argparse.ArgumentParser()
argparser.add_argument("-b","--buffers", help="Include buffers", action="store_true")
argparser.add_argument("-K","--kernel-stuff", help="Include kernel stuff", action="store_true")
argparser.add_argument("-f","--file", help="Dump to file", dest='outfile')
argparser.add_argument("bytes", type=str, nargs='?',
                       help="How many bytes to fuck up. Can be a range as in \"4000-8000\" (the default)",
                       default="null")

argparser.add_argument("-p","--process", help="Target this process name", type=str)
argparser.add_argument("-k","--pid", help="Target this PID", type=int)
argparser.add_argument("--process-libs", help="Include shared libraries when targeting a process", action="store_true")

args = argparser.parse_args()

if args.buffers:
    include.append("RAM buffer")
if args.kernel_stuff:
    include.append("Kernel code")
    include.append("Kernel data")
    include.append("Kernel bss")

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

processes = devmem_util.getProcs()
commands = []
ddtmpl = "dd if=/dev/urandom of=/dev/mem bs=512 seek={} count={} oflag=seek_bytes iflag=count_bytes"

chosenStart = None
chosenEnd = None

if args.process or args.pid:
    target = None
    if args.process:
        targets = [proc for proc in processes if proc.name == args.process]
        if len(targets) < 1:
            print("No process with the name '"+args.process+"' could be found.")
            exit(1)
        target = random.choice(targets)
    elif args.pid:
        for proc in processes:
            if proc.pid == args.pid:
                target = proc
        if not target:
            print("PID "+str(args.pid)+" not found.")
            exit(1)
    print("Targetting "+target.name+'['+str(target.pid)+']')
    targetMaps = target.maps
    if not args.process_libs:
        targetMaps = filter(lambda x: not x[2].startswith('/'), target.maps)

    targetMap = random.choice(targetMaps)
    chosenStart = targetMap[0]
    chosenEnd = targetMap[1]
else:
    blocks = devmem_util.getBlocks(include)
    chosenBlock = random.randint(0,len(blocks)-1)
    chosenStart = max(0,blocks[chosenBlock].start)
    chosenEnd = blocks[chosenBlock].end
    print("Block: "+blocks[chosenBlock].name+" ("+hex(chosenStart)+'-'+hex(chosenEnd)+')')

chosenOffset = random.randint(chosenStart,chosenEnd)
chosenCount = random.randint(bytesLower,bytesUpper)

hits = []

for proc in processes:
    for map_ in proc.maps:
        if chosenOffset <= map_[1] and\
           map_[0] <= chosenOffset+chosenCount:
               hits.append((proc, map_))

print("Target: "+hex(chosenOffset)+'-'+hex(chosenOffset+chosenCount), file=sys.stderr)
print("Hitting:", file=sys.stderr)
for hit in hits:
    print(hit[0].name+'['+str(hit[0].pid)+']: '+hit[1][2]+" ("+hex(hit[1][0])+'-'+hex(hit[1][1])+')', file=sys.stderr)

command = ddtmpl.format(chosenOffset,chosenCount)

print(command)
