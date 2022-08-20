#!/usr/bin/env python3

import sys
import subprocess

if __name__ == "__main__":
    if(len(sys.argv) < 3):
        print("Usage genkernelsyms.py: <dest> <obj1>[;<obj2>...]")

    dest = open(sys.argv[1], "w")

    symbols = {}

    symtab = []
    strtab = []
    strTabOffset = 0

    for arg in sys.argv[2].split(';'):
        p = subprocess.run(["nm", "-gj", "--defined-only", arg], stdout=subprocess.PIPE)

        dump = p.stdout.decode().splitlines()
        # Use a dictionary so we don't have to worry about duplicate symbols
        # (its the linker's job to sort those out)
        for ln in dump:
            symbols[ln] = 1

    for sym, n in symbols.items():
        symtab.append(f"extern {sym}\ndq {sym}\n")
        symtab.append("dq _kernel_symbol_strings + " + str(strTabOffset) + "\n")
        strtab.append(f'db "{ sym }", 0\n')

        strTabOffset += len(sym) + 1

    dest.write("global _kernel_symbols_start\n")
    dest.write("global _kernel_symbol_strings\n")
    dest.write("section .data\n")
    dest.write("_kernel_symbols_start:\n")
    dest.writelines(symtab)
    dest.write("_kernel_symbol_strings:\n")
    dest.writelines(strtab)

