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

blocks = devmem_util.getBlocks(include)
commands = []
ddtmpl = "dd if=/dev/urandom of=/dev/mem bs=512 seek={} count={} oflag=seek_bytes iflag=count_bytes"

chosenblock = random.randint(0,len(blocks)-1)
chosenstart = max(0,blocks[chosenblock].start)
chosenend = blocks[chosenblock].end

chosenOffset = random.randint(chosenstart,chosenend)
chosenCount = random.randint(bytesLower,bytesUpper)

processes = devmem_util.getProcs()
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
