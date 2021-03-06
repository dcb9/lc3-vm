//
//  main.cpp
//  lc3-vm
//
//  Created by Du, Chengbin on 2022/5/5.
//

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
/* unix */
#include <unistd.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

using namespace std;

enum
{
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

//
// Opcodes
enum
{
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
enum
{
    FL_POS = 1 << 0, // P
    FL_ZRO = 1 << 1, // Z
    FL_NEG = 1 << 2, // N
};



enum
{
    MR_KBSR = 0xFE00, /* keyboard status */
    MR_KBDR = 0xFE02  /* keyboard data */
};

enum
{
    TRAP_GETC = 0x20,  /* get character from keyboard, not echoed onto the terminal */
    TRAP_OUT = 0x21,   /* output a character */
    TRAP_PUTS = 0x22,  /* output a word string */
    TRAP_IN = 0x23,    /* get character from keyboard, echoed onto the terminal */
    TRAP_PUTSP = 0x24, /* output a byte string */
    TRAP_HALT = 0x25   /* halt the program */
};

#define MEMORY_MAX (1 << 16)
uint16_t memory[MEMORY_MAX]; /* 65536 locations */
uint16_t reg[R_COUNT];

uint16_t sign_extend(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}
uint16_t swap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}
void update_flags(uint16_t r)
{
    if (reg[r]==0) {
        reg[R_COND] = FL_ZRO;
    } else if(reg[r] >> 15) {
        reg[R_COND] = FL_NEG;
    } else {
        reg[R_COND] = FL_POS;
    }
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
bool read_image(const char * image_path)
{
    cout << "load image: " << image_path << "\n";
    FILE* file = fopen(image_path, "rb");
    if (!file) { return false; };
    read_image_file(file);
    fclose(file);
    return 1;
}
uint16_t check_key()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}
void mem_write(uint16_t address, uint16_t val)
{
    memory[address] = val;
}

uint16_t mem_read(uint16_t address) {
    if (address == MR_KBSR)
    {
        if (check_key())
        {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        }
        else
        {
            memory[MR_KBSR] = 0;
        }
    }
    return memory[address];
}

struct termios original_tio;

void disable_input_buffering()
{
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\nExit.\n");
    exit(-2);
}

void run(void);


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
    signal(SIGINT, handle_interrupt);
    disable_input_buffering();

    /* since exactly one condition flag should be set at any given time, set the Z flag */
    reg[R_COND] = FL_ZRO;

    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

    run();

    return 0;
}

void init(void) {
    signal(SIGINT, handle_interrupt);
    disable_input_buffering();

    /* since exactly one condition flag should be set at any given time, set the Z flag */
    reg[R_COND] = FL_ZRO;
    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;
}


void op_add(uint16_t instr) {
    // destination register (DR)
    uint16_t r0 = (instr >> 9) & 0b111;
    // first register (SR1)
    uint16_t r1 = (instr >> 6) & 0b111;
    // whether we are in immediate mode
    uint16_t imm_flag = (instr >> 5) & 0b1;
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
    uint16_t r0 = (instr >> 9) & 0b111;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    reg[r0] = mem_read( mem_read(reg[R_PC] + pc_offset) );
    update_flags(r0);
}

void op_ldr(uint16_t instr) {
    // destination register (DR)
    uint16_t r0 = (instr >> 9) & 0b111;
    // Base register (BaseR)
    uint16_t r1 = (instr >> 6) & 0b111;
    uint16_t offset = sign_extend(instr & 0x3F, 6);
    reg[r0] = mem_read(reg[r1] + offset);
    update_flags(r0);
}

void op_and(uint16_t instr) {
    // destination register (DR)
    uint16_t r0 = (instr >> 9) & 0x7;
    // SR1
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t imm_flag = (instr >> 5) & 0x1;
    if (imm_flag) {
        reg[r0] = reg[r1] & sign_extend(instr & 0x1F, 5);
    } else {
        uint16_t r2 = instr & 0x7;
        reg[r0] = reg[r1] & reg[r2];
    }
    update_flags(r0);
}

void op_br(uint16_t instr) {
    uint16_t conf_flag = (instr >> 9) & 0x7;
    if (conf_flag & reg[R_COND]) {
        reg[R_PC] += sign_extend(instr & 0x1FF, 9);
    }
}

void op_ld(uint16_t instr) {
    // destination register (DR)
    uint16_t r = (instr >> 9) & 0x7;
    uint16_t offset = sign_extend(instr & 0x1FF, 9);
    reg[r] = mem_read(reg[R_PC] + offset);
    update_flags(r);
}

void op_st(uint16_t instr) {
    // destination register (DR)
    uint16_t r = (instr >> 9) & 0b111;
    uint16_t offset = sign_extend(instr & 0x1FF, 9);
    mem_write(reg[R_PC] + offset, reg[r]);
}

void op_jsr(uint16_t instr) {
    // destination register (DR)
    uint16_t pc_offset_flag = (instr >> 11) & 1;
    reg[R_R7] = reg[R_PC]; // save the return address
    if (pc_offset_flag) {
        // JSR: PC-relative
        uint16_t pc_offset = sign_extend(instr & 0x7FF, 11);
        reg[R_PC] += pc_offset;
    } else {
        // JSRR: Base Register addressing
        uint16_t r = (instr >> 6) & 0x7;
        reg[R_PC] = reg[r];
    }
}

void op_str(uint16_t instr) {
    // destination register (DR)
    uint16_t dr = (instr >> 9) & 0x7;
    uint16_t base_r = (instr >> 6) & 0x7;
    uint16_t offset = sign_extend(instr & 0x3F, 6);
    mem_write(reg[base_r] + offset, reg[dr]);
}

void op_not(uint16_t instr) {
    // destination register (DR)
    uint16_t dr = (instr >> 9) & 0x7;
    uint16_t sr = (instr >> 6) & 0x7;
    reg[dr] = ~reg[sr];
    update_flags(dr);
}

void op_sti(uint16_t instr) {
    // destination register (DR)
    uint16_t dr = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr &0x1FF, 9);
    mem_write( mem_read(reg[R_PC]+pc_offset), reg[dr]);
}

void op_jmp(uint16_t instr) {
    // destination register (DR)
    uint16_t r = (instr >> 6) & 0x7;
    reg[R_PC] = reg[r];
}

void op_lea(uint16_t instr) {
    // destination register (DR)
    uint16_t r0 = (instr >> 9) & 0b111;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    reg[r0] = reg[R_PC] + pc_offset;
    update_flags(r0);
}

void op_trap(uint16_t instr, bool * running) {
    reg[R_R7] = reg[R_PC];

    switch (instr & 0xFF) {
    case TRAP_GETC:
        /* read a single ASCII char */
        reg[R_R0] = (uint16_t)getchar();

        update_flags(R_R0);


        break;
    case TRAP_OUT:
        putc((char)reg[R_R0], stdout);
        fflush(stdout);
        return;
    case TRAP_PUTS:
    {
        /* one char per word */
        uint16_t *c = memory + reg[R_R0];
        while (*c)
        {
            putc((char)*c, stdout);
            ++c;
        }
        fflush(stdout);
    }
    return;
    case TRAP_IN:
    {
        printf("Enter a character: ");
        char c = getchar();
        putc(c, stdout);
        fflush(stdout);
        reg[R_R0] = (uint16_t)c;
        update_flags(R_R0);
    }
    return;
    case TRAP_PUTSP:
    {
        /* one char per byte (two bytes per word)
           here we need to swap back to
           big endian format */
        uint16_t *c = memory + reg[R_R0];
        while (*c)
        {
            char char1 = (*c) & 0xFF;
            putc(char1, stdout);
            char char2 = (*c) >> 8;
            if (char2)
                putc(char2, stdout);
            ++c;
        }
        fflush(stdout);
    }
    return;
    case TRAP_HALT:
        puts("HALT");
        fflush(stdout);
        *running = false;
        return;
    }
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
                op_trap(instr, &running);
                break;
        }
    }

    restore_input_buffering();
}
