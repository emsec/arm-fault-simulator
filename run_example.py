#!/usr/bin/python3

import os
import sys
import shutil

# build ARMORY
if not os.path.exists("build/"):
    if os.system("meson -Db_lto=true --buildtype=release build") != 0: sys.exit(1)
if os.system("meson compile -C build/") != 0: sys.exit(1)

# dir for tmp data
os.makedirs("example/tmp_data/", exist_ok=True)

# build the ARM binary
if os.system("arm-none-eabi-gcc -Wall -Wpedantic -std=c11 -ffreestanding -nostdlib -mthumb -march=armv7-m -O3 -Wl,-Texample/linker.ld -Wl,-Map,example/tmp_data/binary.map example/example.c -o example/tmp_data/binary.elf") != 0: sys.exit(1)

# create disassembly for looking up armory results later
if os.system("arm-none-eabi-objdump example/tmp_data/binary.elf -d > disassembled.txt") != 0: sys.exit(1)

# start fault simulation
if os.system("cd example && python3 start_emulation.py") != 0: sys.exit(1)

# cleanup tmp data
shutil.rmtree("example/tmp_data/")

