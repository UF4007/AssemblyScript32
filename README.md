## AssemblyScript32

Simple 32-bit assembly-like script language.

Suitable when:
- resource-limited device, such as MCU
- state synchronization required scenarios
- stable, high-performance, and unlimited script language (hint on Westwood Ra2 terrible trigger system)

demo:
```C++
#include "AS32_demo.h"
asc_testmain();
```

description:
- each **function** has two registers, EIP (program counter) and ESP (universal register), and a fixed-size binary stack/program mixed assembly byte area.
- each **32bit instruction** comprised of three parts:
  - one **opcode** (2 bytes)
  - two **addressing mode** (1*2 bytes)
  - **operand 1** (4 bytes, if opcode requires this)
  - **operand 2** (4 bytes, if opcode requires this)
- there is only one operand type: int32
- each **elf** has four sections:
  - basic: name (char[8]), referenced count
  - dependency: dependency name table, import functions table
  - text section: an array store functions
  - data section: The global data section, can be addressed by instruction from the text section in the same elf file
- **CALL**: an assembly instruction to call another function, ESP from the caller function will be addressed as [EBP + operand] in the callee function, to deliver stipulation parameters.
  - local call: operand 1 addressing result >= 0, call function by text section array index of this elf.
  - import call: operand 1 addressing result < 0, call function by import function table index of this elf. operand 1 will be negatived then reduce 1.
- **CALLEXT**: **extlib** records the extern C++ function that can be called by instruction CALLEXT.
  - see **AS32_extlib.h** as demo

todo:
- a vue website editor for AssemblyScript32
- shorter **8-bit instructions** (It will make the C++ source more complicated and need more test)
- dynamic memory allocation(with individual instruction) and safety heap addressing
