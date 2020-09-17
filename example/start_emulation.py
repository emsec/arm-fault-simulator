START_SYMBOL = "interesting_to_fault"
HALT_SYMBOLS = ["super_secret_function"]
IGNORE_SYMBOLS = []


################################################################
############## DO NOT CHANGE ANYTHING BELOW ####################
################################################################

from subprocess import Popen, PIPE
import os
import sys

def is_int(s):
    try:
        int(s, 16)
    except:
        return False
    return True

def run(cmd):
    p = Popen(cmd.split(), stdin=PIPE, stdout=PIPE, stderr=PIPE)
    output, err = p.communicate()
    output = output.decode('UTF-8')
    err = err.decode('UTF-8')
    if p.returncode != 0:
        print(output)
        print(err)
        sys.exit(1)
    return output, err

def num(s):
    return hex(int(s, 16))

# extract/verify supported architecture

armv6 = None
out,err = run("arm-none-eabi-readelf -A tmp_data/binary.elf")

if "Tag_CPU_name:" in out:
    out = out[out.find("Tag_CPU_name:"):]
    out = out[out.find(":")+1 : out.find("\n")].strip()
    if out == '"6-M"':
        armv6 = True
    elif out == '"7-M"':
        armv6 = False
    else:
        print("ERROR: {} is not an ARMv6-M or ARMv7-M binary.".format(out))
        sys.exit(1)
else:
    print("ERROR: 'Tag_CPU_name' is missing in the output of readelf.")
    sys.exit(1)



# extract addresses of symbols

with open("tmp_data/binary.map", "rt") as file:
    lines = file.readlines()

flash = (0,0)
end_of_flash = None
ram = (0,0)
start_address = None
halt_addresses = list()
ignore_ranges = list()

for line in lines:
    parts = line.split()
    if len(parts) < 2: continue
    if parts[0] == "flash":
        flash = (num(parts[1]), num(parts[2])) # origin, length
        end_of_flash = int(parts[1], 16) + int(parts[2],16)
    elif parts[0] == "ram":
        ram = (num(parts[1]), num(parts[2])) # origin, length


# extract all symbols

begin_ignore = None
sections = list()
out,err = run("arm-none-eabi-objdump tmp_data/binary.elf -t")
lines = sorted([l for l in out.split("\n")])
for l in lines:
    parts = l.split()

    if len(parts) < 3: continue

    if begin_ignore != None and parts[2] == "F" and not (parts[-1] in IGNORE_SYMBOLS):
        if is_int(parts[0]):
            ignore_ranges.append((begin_ignore, hex(min(int(parts[0],16), end_of_flash))))
            begin_ignore = None

    if parts[-1] == START_SYMBOL:
        start_address = num(parts[0])
    elif parts[-1] in HALT_SYMBOLS:
        halt_addresses.append((parts[-1], num(parts[0])))
    elif (parts[-1] in IGNORE_SYMBOLS or parts[2] == "O") and begin_ignore == None:
        begin_ignore = num(parts[0])

if begin_ignore != None:
    ignore_ranges.append((begin_ignore, hex(end_of_flash)))

if flash == (0,0) or ram == (0,0):
    print("flash or ram section could not be found in map file")
    sys.exit(1)

if start_address == None:
    print("{} symbol could not be found in map file".format(START_SYMBOL))
    sys.exit(1)

if not halt_addresses:
    print("no end symbol could not be found in map file")
    sys.exit(1)

# extract all sections to binary files

sections = list()
out,err = run("arm-none-eabi-objdump tmp_data/binary.elf -h")
lines = [l for l in out.split("\n")]
while len(lines) > 0:
    l = lines.pop(0)
    parts = l.split()
    if len(parts) == 7 and parts[1].startswith("."):
        attr = lines.pop(0)
        if "ALLOC" not in attr: continue
        if int(parts[2], 16) != 0: # size != 0
            sections.append((parts[1], num(parts[3]))) # name, offset

for name, _ in sections:
    run("arm-none-eabi-objcopy -O binary --only-section="+name+" tmp_data/binary.elf tmp_data/code_section"+name)

# generate command line args

args = "--start {}".format(start_address)
for x in halt_addresses:
    args += " --halt {} {}".format(x[0], x[1])
for start, end in ignore_ranges:
    args += " --ignore {} {}".format(start, end)

args += " --flash {} {} --ram {} {}".format(flash[0], flash[1], ram[0], ram[1])

if armv6: args += " --armv6m"
else: args += " --armv7m"

for name, offset in sections:
    args += " --section tmp_data/code_section{} {}".format(name, offset)

sys.stderr.write(args+"\n")

os.system("../build/armory_example " + args)
