import re
import argparse
import os.path

include = ['System RAM',
        'System ROM',
        'Kernel code',
        'Kernel data',
        'Kernel bss']

argparser = argparse.ArgumentParser()
argparser.add_argument("-b","--buffers", help="Include buffers", action="store_true")
argparser.add_argument("-f","--file", help="Dump to file", dest='outfile')

args = argparser.parse_args()

if args.buffers:
    include.append("RAM buffer")

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

blocks = []
for line in iomem:
    blk = IOMemBlock()
    pattern = "^\s*([0-9a-f]+)-([0-9a-f]+) : (.*)$"
    prog = re.compile(pattern,re.M)
    m = prog.match(line)
    blk.name = m.group(3)
    if blk.name not in include:
        continue
    blk.start = int(m.group(1),16)
    blk.end = int(m.group(2),16);
    blocks.append(blk)

commands = []
ddtmpl = "dd if=/dev/mem bs=512 skip={} count={} iflag=skip_bytes,count_bytes"
if args.outfile:
    ddtmpl += ' >> '+args.outfile

for block in blocks:
    command = ddtmpl.format(max(0,block.start-1),block.end-block.start)
    commands.append(command+'\n')

if os.path.isfile("dddevmem.sh"):
    print "dddevmem.sh already exists. Exiting"
    exit(1)

f = open("dddevmem.sh",'w')
f.writelines(commands)
f.close()
