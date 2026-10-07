# RV32I Instruction-Set Simulator (C++ Reference Model)

A small **C++ simulator for the RV32I RISC-V instruction set**. It runs the same compiled programs as my Verilog RISC-V core and writes a per-instruction trace. Comparing that trace with the RTL simulation shows exactly where the hardware first goes wrong.

## Why this project

When you design a CPU, you need a "correct answer" to compare against. A software model is much easier to write and debug than hardware, so it acts as the **golden reference**. Chip teams use C or C++ models like this to check RTL before silicon exists.

## How it works (simple version)

The simulator is a loop that copies what a real CPU does, one instruction at a time:

```
   +--> FETCH    read the 32-bit instruction at the program counter (PC)
   |    DECODE   split it into opcode, registers, and immediate value
   |    EXECUTE  do the operation (add, compare, load, store, branch ...)
   |    WRITE    update the register (if needed) and the PC, log a trace line
   +----------- repeat until the program writes the mailbox
```

- **Memory** is a 16 KB byte array starting at address `0x0` (same as the hardware project).
- **Registers** are an array of 32 numbers; `x0` always stays zero.
- **Special addresses** work like the RTL testbench: a store to `0x10000004` prints a character, and a store to `0x10000000` ends the run (`1` = PASS, `2` = FAIL).
- **Custom MAC** (opcode `0x0B`) is implemented as `rd = rs1 * rs2`. Change this line in `rv32i_iss.cpp` if your accelerator does something different.

### Supported instructions

All RV32I base instructions: `LUI AUIPC JAL JALR`, branches (`BEQ BNE BLT BGE BLTU BGEU`), loads (`LB LH LW LBU LHU`), stores (`SB SH SW`), ALU immediate and register operations, shifts, `FENCE` (as a no-op), plus the custom MAC.

## Build and run

```bash
make test
```

This builds `rv32i_iss` and runs `examples/prog.bin`. Or manually:

```bash
g++ -std=c++17 -O2 -Wall -Wextra rv32i_iss.cpp -o rv32i_iss
./rv32i_iss examples/prog.bin trace.txt
```

Exit code: `0` = PASS, `1` = FAIL, `2` = timeout or error.

Expected output:

```
RV32I bare-metal tests
array_sum: PASS
fib10: PASS
mem_rw: PASS
custom_mac: PASS
*** ISS: ALL TESTS PASSED ***
```

`examples/prog.bin` is built from [`rv32i-baremetal-tests`](https://github.com/khatriayanna/rv32i-baremetal-tests).

## Comparing the model with the RTL

1. Add `rtl_trace_snippet.sv` to your testbench (rename the signals). It writes `rtl_trace.txt` in the same format as the ISS.
2. Run the same `prog.hex` / `prog.bin` on both.
3. Compare:

```bash
python3 compare_traces.py trace.txt rtl_trace.txt
```

The script prints the **first instruction where the two disagree**, for example:

```
MISMATCH at instruction #42
  ISS (expected): 00000064 00a50533 x10=0000000a
  RTL (actual)  : 00000064 00a50533 x10=00000009
```

That one line tells you the instruction, the register, and the wrong value, which is where debugging starts.

### Trace format

```
<pc> <instruction> [xN=<value written to register N>]
```

All values are 8-digit lowercase hex. Instructions that do not write a register (stores, branches) have no `xN=` part.

## Files

| File                      | Purpose                                          |
|---------------------------|--------------------------------------------------|
| `rv32i_iss.cpp`           | the simulator (CPU class + `main`)               |
| `Makefile`                | `make` builds, `make test` runs the example      |
| `compare_traces.py`       | finds the first mismatch between two traces      |
| `rtl_trace_snippet.sv`    | testbench code that writes the RTL trace         |
| `examples/prog.bin`       | example program to run                           |

## What I learned

- How fetch, decode, and execute work at the instruction level, including how immediates are rebuilt from scattered instruction bits.
- Why a reference model makes hardware debugging faster.
- How to design a trace format that two different tools (C++ and RTL) can compare.

## Limitations and future work

- No interrupts, CSRs, compressed instructions, or M/A/F extensions.
- Misaligned accesses are not modelled.
- Single flat memory; no caches, MMU, or DMA.
- Next steps: add timer interrupts and a small DMA model, matching the hardware project.
