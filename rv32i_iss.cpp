// rv32i_iss.cpp
// A small RV32I instruction-set simulator in C++.
// Use it as a "golden reference model": run the same program as your RTL core,
// then compare the execution traces to find hardware bugs.
//
// Build:  make
// Run:    ./rv32i_iss prog.bin [trace.txt]
// Exit code: 0 = program reported PASS, 1 = FAIL, 2 = timeout / error

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

class Rv32iCpu {
public:
    // Memory map: must match link.ld and your testbench
    static constexpr uint32_t MEM_SIZE = 16 * 1024;
    static constexpr uint32_t MAILBOX  = 0x10000000u;  // 1 = PASS, 2 = FAIL
    static constexpr uint32_t UART_TX  = 0x10000004u;

    explicit Rv32iCpu(const std::vector<uint8_t> &program, const char *trace_path)
        : mem_(MEM_SIZE, 0) {
        for (size_t i = 0; i < program.size() && i < MEM_SIZE; i++) mem_[i] = program[i];
        if (trace_path) trace_ = std::fopen(trace_path, "w");
    }
    ~Rv32iCpu() { if (trace_) std::fclose(trace_); }

    // Run until the program writes the mailbox, or max_steps is reached.
    int run(uint64_t max_steps) {
        for (uint64_t n = 0; n < max_steps && !done_; n++) step();
        if (!done_) { std::fprintf(stderr, "ERROR: timeout (no mailbox write)\n"); return 2; }
        return mailbox_value_ == 1 ? 0 : 1;
    }

private:
    std::vector<uint8_t> mem_;
    uint32_t x_[32] = {0};   // register file
    uint32_t pc_ = 0;
    bool done_ = false;
    uint32_t mailbox_value_ = 0;
    FILE *trace_ = nullptr;

    // ---- memory access (RAM + memory-mapped I/O) ----
    uint32_t load(uint32_t addr, int bytes, bool is_signed) {
        if (addr + bytes > MEM_SIZE) { fail("load out of range", addr); return 0; }
        uint32_t v = 0;
        for (int i = 0; i < bytes; i++) v |= (uint32_t)mem_[addr + i] << (8 * i);
        if (is_signed && bytes < 4) {
            uint32_t sign = 1u << (8 * bytes - 1);
            if (v & sign) v |= ~((sign << 1) - 1);
        }
        return v;
    }
    void store(uint32_t addr, int bytes, uint32_t val) {
        if (addr == UART_TX) { std::putchar((int)(val & 0xff)); return; }
        if (addr == MAILBOX) { mailbox_value_ = val; done_ = true; return; }
        if (addr + bytes > MEM_SIZE) { fail("store out of range", addr); return; }
        for (int i = 0; i < bytes; i++) mem_[addr + i] = (val >> (8 * i)) & 0xff;
    }
    void fail(const char *msg, uint32_t addr) {
        std::fprintf(stderr, "ERROR: %s at 0x%08x (pc=0x%08x)\n", msg, addr, pc_);
        mailbox_value_ = 2; done_ = true;
    }

    // ---- execute one instruction ----
    void step() {
        uint32_t inst = load(pc_, 4, false);
        uint32_t opcode = inst & 0x7f;
        uint32_t rd  = (inst >> 7) & 0x1f;
        uint32_t f3  = (inst >> 12) & 0x7;
        uint32_t rs1 = (inst >> 15) & 0x1f;
        uint32_t rs2 = (inst >> 20) & 0x1f;
        uint32_t f7  = inst >> 25;
        uint32_t a = x_[rs1], b = x_[rs2];

        // immediates
        int32_t imm_i = (int32_t)inst >> 20;
        int32_t imm_s = ((int32_t)(inst & 0xfe000000u) >> 20) | ((inst >> 7) & 0x1f);
        int32_t imm_b = ((int32_t)(inst & 0x80000000u) >> 19) | ((inst & 0x80) << 4) |
                        ((inst >> 20) & 0x7e0) | ((inst >> 7) & 0x1e);
        uint32_t imm_u = inst & 0xfffff000u;
        int32_t imm_j = ((int32_t)(inst & 0x80000000u) >> 11) | (inst & 0xff000) |
                        ((inst >> 9) & 0x800) | ((inst >> 20) & 0x7fe);

        uint32_t next_pc = pc_ + 4;
        bool writes_rd = false;
        uint32_t result = 0;

        switch (opcode) {
        case 0x37: result = imm_u;        writes_rd = true; break;            // LUI
        case 0x17: result = pc_ + imm_u;  writes_rd = true; break;            // AUIPC
        case 0x6f: result = pc_ + 4; writes_rd = true; next_pc = pc_ + imm_j; break;  // JAL
        case 0x67: result = pc_ + 4; writes_rd = true; next_pc = (a + imm_i) & ~1u; break; // JALR
        case 0x63: {                                                          // branches
            bool take = false;
            switch (f3) {
            case 0: take = (a == b); break;                                   // BEQ
            case 1: take = (a != b); break;                                   // BNE
            case 4: take = ((int32_t)a <  (int32_t)b); break;                 // BLT
            case 5: take = ((int32_t)a >= (int32_t)b); break;                 // BGE
            case 6: take = (a <  b); break;                                   // BLTU
            case 7: take = (a >= b); break;                                   // BGEU
            }
            if (take) next_pc = pc_ + imm_b;
            break;
        }
        case 0x03: {                                                          // loads
            uint32_t addr = a + imm_i;
            writes_rd = true;
            switch (f3) {
            case 0: result = load(addr, 1, true);  break;                     // LB
            case 1: result = load(addr, 2, true);  break;                     // LH
            case 2: result = load(addr, 4, false); break;                     // LW
            case 4: result = load(addr, 1, false); break;                     // LBU
            case 5: result = load(addr, 2, false); break;                     // LHU
            }
            break;
        }
        case 0x23: {                                                          // stores
            uint32_t addr = a + imm_s;
            if (f3 == 0) store(addr, 1, b);                                   // SB
            else if (f3 == 1) store(addr, 2, b);                              // SH
            else if (f3 == 2) store(addr, 4, b);                              // SW
            break;
        }
        case 0x13: {                                                          // ALU with immediate
            writes_rd = true;
            uint32_t sh = rs2;  // shift amount
            switch (f3) {
            case 0: result = a + imm_i; break;                                // ADDI
            case 2: result = ((int32_t)a < imm_i); break;                     // SLTI
            case 3: result = (a < (uint32_t)imm_i); break;                    // SLTIU
            case 4: result = a ^ imm_i; break;                                // XORI
            case 6: result = a | imm_i; break;                                // ORI
            case 7: result = a & imm_i; break;                                // ANDI
            case 1: result = a << sh; break;                                  // SLLI
            case 5: result = (f7 & 0x20) ? (uint32_t)((int32_t)a >> sh) : (a >> sh); break; // SRLI/SRAI
            }
            break;
        }
        case 0x33: {                                                          // ALU register-register
            writes_rd = true;
            switch (f3) {
            case 0: result = (f7 & 0x20) ? a - b : a + b; break;              // SUB / ADD
            case 1: result = a << (b & 31); break;                            // SLL
            case 2: result = ((int32_t)a < (int32_t)b); break;                // SLT
            case 3: result = (a < b); break;                                  // SLTU
            case 4: result = a ^ b; break;                                    // XOR
            case 5: result = (f7 & 0x20) ? (uint32_t)((int32_t)a >> (b & 31)) : (a >> (b & 31)); break;
            case 6: result = a | b; break;                                    // OR
            case 7: result = a & b; break;                                    // AND
            }
            break;
        }
        case 0x0b:                                                            // custom-0: MAC
            result = a * b;   // EDIT to match your accelerator's behaviour
            writes_rd = true;
            break;
        case 0x0f: break;                                                     // FENCE: no-op
        default:
            std::fprintf(stderr, "ERROR: unknown instruction 0x%08x at pc=0x%08x\n", inst, pc_);
            mailbox_value_ = 2; done_ = true;
            break;
        }

        if (writes_rd && rd != 0) x_[rd] = result;
        if (trace_) {
            std::fprintf(trace_, "%08x %08x", pc_, inst);
            if (writes_rd && rd != 0) std::fprintf(trace_, " x%u=%08x", rd, result);
            std::fprintf(trace_, "\n");
        }
        pc_ = next_pc;
    }
};

int main(int argc, char **argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: %s prog.bin [trace.txt]\n", argv[0]); return 2; }
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    std::vector<uint8_t> program((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    Rv32iCpu cpu(program, argc > 2 ? argv[2] : nullptr);
    int rc = cpu.run(1000000);
    std::printf("%s\n", rc == 0 ? "*** ISS: ALL TESTS PASSED ***" : "*** ISS: TESTS FAILED ***");
    return rc;
}
