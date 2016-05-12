from __future__ import print_function
import sys
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

print("Block: "+blocks[chosenblock].name, file=sys.stderr)

command = ddtmpl.format(random.randint(chosenstart,chosenend),random.randint(bytesLower,bytesUpper))

print(command)
