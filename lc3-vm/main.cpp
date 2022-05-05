//
//  main.cpp
//  lc3-vm
//
//  Created by Du, Chengbin on 2022/5/5.
//

#define MEMORY_MAX (1 << 16)
#include <iostream>

using namespace std;

uint16_t memory[MEMORY_MAX];

enum {
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC,
    R_COND,
    R_COUNT,
};
uint16_t reg[R_COUNT];

// Opcodes
enum {
    OP_BR = 0, // branch
    OP_ADD,
    OP_LD, // load
    OP_ST, // store
    OP_JSR, // jump register
    OP_AND, // bitwise and
    OP_LDR, // load register
    OP_STR, // store register
    OP_RTI, // unused
    OP_NOT, // bitwise not
    OP_LDI, // load indirect
    OP_STI, // store indirect
    OP_JMP, // jump
    OP_RES, // reserved (unused)
    OP_LEA, // load effective address
    OP_TRAP // execute trap
};

// Condition Flags
enum {
    FL_POS = 1 << 0, // P
    FL_ZRO = 1 << 1, // Z
    FL_NEG = 1 << 2, // N
};

void init(void);
void run(void);

bool read_image(const char * path);

int main(int argc, const char * argv[]) {
    if (argc < 2) {
        cout << "lc3 [image-file1] ...\n";
        return 2;
    }
    for (int j = 1; j < argc; ++j) {
        if (!read_image(argv[j])) {
            cout << "failed to load image " << argv[j] << "\n";
            return 1;
        }
    }
    
    init();
    run();
    
    // insert code here...
    std::cout << "Hello, World!\n";
    return 0;
}

void init(void) {
    /* since exactly one condition flag should be set at any given time, set the Z flag */
    reg[R_COND] = FL_ZRO;
    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;
}

uint16_t mem_read(uint16_t i) {
    return memory[i];
}

uint16_t sign_extend(uint16_t x, int bit_count) {
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}

void update_flags(uint16_t r) {
    if (r==0) {
        reg[R_COND] = FL_ZRO;
    } else if((r >> 15) & 1) {
        reg[R_COND] = FL_NEG;
    } else {
        reg[R_COND] = FL_POS;
    }
}

void op_add(uint16_t instr) {
    // destination register (DR)
    uint16_t r0 = instr >> 9 & 0b111;
    // first register (SR1)
    uint16_t r1 = instr >> 6 & 0b111;
    // whether we are in immediate mode
    uint16_t imm_flag = instr >> 5 & 0b1;
    if (imm_flag) {
        uint16_t imm5 = sign_extend(instr & 0b11111, 5);
        reg[r0] = reg[r1] + imm5;
    } else {
        uint16_t r2 = instr & 0b111;
        reg[r0] = reg[r1] + reg[r2];
    }
    update_flags(r0);
}

void op_ldi(uint16_t instr) {
    // destination register (DR)
    uint16_t r0 = instr >> 9 & 0b111;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    reg[r0] = mem_read( mem_read(reg[R_PC] + pc_offset) );
    update_flags(r0);
}

void op_ldr(uint16_t instr) {
    // destination register (DR)
    uint16_t r0 = instr >> 9 & 0b111;
    // Base register (BaseR)
    uint16_t r1 = instr >> 6 & 0b111;
    uint16_t offset = sign_extend(instr & 0x3F, 6);
    reg[r0] = mem_read(reg[r1] + offset);
    update_flags(r0);
}

void op_and(uint16_t instr) {
    // destination register (DR)
    uint16_t r0 = instr >> 9 & 0b111;
    // SR1
    uint16_t r1 = instr >> 6 & 0b111;
    uint16_t imm_flag = instr >> 5 & 0b1;
    if (imm_flag) {
        reg[r0] = reg[r1] & sign_extend(instr & 0x1F, 5);
    } else {
        uint16_t r2 = instr & 0b111;
        reg[r0] = reg[r1] & reg[r2];
    }
    update_flags(r0);
}

void op_br(uint16_t instr) {
    uint16_t conf_flag = instr >> 9 & 0b111;
    if (conf_flag & reg[R_COND]) {
        reg[R_PC] += sign_extend(instr & 0x1FF, 9);
    }
}

void op_ld(uint16_t instr) {
    // destination register (DR)
    uint16_t r0 = instr >> 9 & 0b111;
    uint16_t offset = sign_extend(instr & 0x1FF, 9);
    reg[r0] = mem_read(reg[R_PC] + offset);
    update_flags(reg[r0]);
}

void op_st(uint16_t instr) {
    // destination register (DR)
    uint16_t r0 = instr >> 9 & 0b111;
    reg[r0] = 1; // @todo
    update_flags(r0);
}

void op_jsr(uint16_t instr) {
    // destination register (DR)
    uint16_t r0 = instr >> 9 & 0b111;
    reg[r0] = 1; // @todo
    update_flags(r0);
}

void op_str(uint16_t instr) {
    // destination register (DR)
    uint16_t r0 = instr >> 9 & 0b111;
    reg[r0] = 1; // @todo
    update_flags(r0);
}

void op_not(uint16_t instr) {
    // destination register (DR)
    uint16_t r0 = instr >> 9 & 0b111;
    reg[r0] = 1; // @todo
    update_flags(r0);
}

void op_sti(uint16_t instr) {
    // destination register (DR)
    uint16_t r0 = instr >> 9 & 0b111;
    reg[r0] = 1; // @todo
    update_flags(r0);
}

void op_jmp(uint16_t instr) {
    // destination register (DR)
    uint16_t r0 = instr >> 9 & 0b111;
    reg[r0] = 1; // @todo
    update_flags(r0);
}

void op_lea(uint16_t instr) {
    // destination register (DR)
    uint16_t r0 = instr >> 9 & 0b111;
    reg[r0] = 1; // @todo
    update_flags(r0);
}

void op_trap(uint16_t instr) {
    // destination register (DR)
    uint16_t r0 = instr >> 9 & 0b111;
    reg[r0] = 1; // @todo
    update_flags(r0);
}

void run(void) {
    bool running = true;
    while (running) {
        // Fetch instruction
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12;
        switch (op) {
            case OP_BR:
                op_br(instr);
                break;
            case OP_ADD:
                op_add(instr);
                break;
            case OP_LD:
                op_ld(instr);
                break;
            case OP_ST:
                op_st(instr);
                break;
            case OP_JSR:
                op_jsr(instr);
                break;
            case OP_AND:
                op_and(instr);
                break;
            case OP_LDR:
                op_ldr(instr);
                break;
            case OP_STR:
                op_str(instr);
                break;
            case OP_RTI:
                cout << "error: unimplemented opcode\n";
                exit(-2);
                break;
            case OP_NOT:
                op_not(instr);
                break;
            case OP_LDI:
                op_ldi(instr);
                break;
            case OP_STI:
                op_sti(instr);
                break;
            case OP_JMP:
                op_jmp(instr);
                break;
            case OP_RES:
                cout << "error: use reserved opcode\n";
                exit(-2);
                break;
            case OP_LEA:
                op_lea(instr);
                break;
            case OP_TRAP:
                op_trap(instr);
                break;
        }
    }
}

uint16_t swap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}

void read_image_file(FILE* file)
{
    /* the origin tells us where in memory to place the image */
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin);

    /* we know the maximum file size so we only need one fread */
    uint16_t max_read = MEMORY_MAX - origin;
    uint16_t* p = memory + origin;
    size_t count = fread(p, sizeof(uint16_t), max_read, file);

    /* swap to little endian */
    while (count-- > 0) {
        *p = swap16(*p);
        ++p;
    }
}

bool read_image(const char * path) {
    cout << "load image: " << path << "\n";
    FILE* file = fopen(path, "rb");
    if (!file) { return false; };
    read_image_file(file);
    fclose(file);
    return true;
}
