#include <stdint.h>
#include <string.h>

#include "instruction.h"
#include "emulator.h"
#include "emulator_function.h"

//各フラグ用のマクロ
#define DEFINE_JX(flag, is_flag) \
static void j ## flag(Emulator* emu){\
    int diff = is_flag(emu) ? get_sign_code8(emu,1):0;\
    emu->eip += (diff + 2);\
}\
static void jn ## flag(Emulator* emu){\
    int diff = is_flag(emu) ? 0 : get_sign_code8(emu,1);\
    emu->eip += (diff + 2);\
}

/* x86命令の配列、opecode番目の関数がx86の
   opcodeに対応した命令となっている */
instruction_func_t* instructions[256];


static void mov_r32_imm32(Emulator* emu)
{
    uint8_t reg = get_code8(emu,0) - 0xB8;
    uint32_t value = get_code32(emu,1);
    set_register32(emu, reg, value);
    emu->eip +=5;
}
//r32から32ビット値を読み込みrm32に書き込み
static void mov_r32_rm32(Emulator* emu)
{
    emu->eip += 1;
    ModRM modrm;
    parse_modrm(emu, &modrm);
    uint32_t rm32 = get_rm32(emu, &modrm);
    set_r32(emu, &modrm, rm32);
}

static void add_rm32_r32(Emulator* emu)
{
    emu->eip += 1;
    ModRM modrm;
    parse_modrm(emu, &modrm);
    uint32_t r32 = get_r32(emu, &modrm);
    uint32_t rm32 = get_rm32(emu, &modrm);
    set_rm32(emu, &modrm, rm32 + r32);
}
//rm32から32ビット値を読み取り、r32に書き込む
static void mov_rm32_r32(Emulator* emu)
{
    emu->eip += 1;
    ModRM modrm;
    parse_modrm(emu, &modrm);
    uint32_t r32 = get_r32(emu, &modrm);
    set_rm32(emu, &modrm, r32);
}

static void sub_rm32_imm8(Emulator* emu, ModRM* modrm)
{
    uint32_t rm32 = get_rm32(emu, modrm);
    uint32_t imm8 = (int32_t)get_sign_code8(emu, 0);
    emu->eip += 1;
    set_rm32(emu, modrm, rm32 - imm8);
}

static void code_83(Emulator* emu)
{
    emu->eip += 1;
    ModRM modrm;
    parse_modrm(emu, &modrm);

    switch (modrm.opecode) {
    case 5:
        sub_rm32_imm8(emu, &modrm);
        break;
    default:
        printf("not implemented: 83 /%d\n", modrm.opecode);
        exit(1);
    }
}

//32ビットの即値をMod/R/Mで指定されたレジスタまたはメモリ領域に書き込む
static void mov_rm32_imm32(Emulator* emu)
{
    emu->eip += 1;
    ModRM modrm;
    parse_modrm(emu, &modrm);
    uint32_t value = get_code32(emu, 0);
    emu->eip += 4;
    set_rm32(emu, &modrm, value);
}

static void inc_rm32(Emulator* emu, ModRM* modrm)
{
    uint32_t value = get_rm32(emu, modrm);
    set_rm32(emu, modrm, value + 1);
}

static void code_ff(Emulator* emu)
{
    emu->eip += 1;
    ModRM modrm;
    parse_modrm(emu, &modrm);

    switch (modrm.opecode) {
    case 0:
        inc_rm32(emu, &modrm);
        break;
    default:
        printf("not implemented: FF /%d\n", modrm.opecode);
        exit(1);
    }
}

static void short_jump(Emulator* emu)
{
    int8_t diff = get_sign_code8(emu, 1);
    emu->eip += (diff + 2);
}

static void near_jump(Emulator* emu)
{
    int32_t diff = get_sign_code32(emu, 1);
    emu->eip += (diff + 5);
}

void init_instructions(void)
{
    int i;
    memset(instructions, 0, sizeof(instructions));
    instructions[0x01] = add_rm32_r32;
    instructions[0x83] = code_83;
    instructions[0x89] = mov_rm32_r32;
    instructions[0x8B] = mov_r32_rm32;
    for (i = 0; i < 8; i++) {
        instructions[0xB8 + i] = mov_r32_imm32;
    }
    instructions[0xC7] = mov_rm32_imm32;
    instructions[0xE9] = near_jump;
    instructions[0xEB] = short_jump;
    instructions[0xFF] = code_ff;
}

static void push_r32(Emulator* emu){
    //レジスタ番号取得(ベース値を引き算することで取得)
    uint8_t reg = get_code8(emu,0) - 0x50;
    //取得したレジスタ番号を基にレジスタから値を読み込み
    push32(emu, get_register32(emu, reg));
    //スタックトップにプッシュ
    emu->eip += 1;
}

static void pop_r32(Emulator* emu){
    uint8_t reg = get_code8(emu, 0) - 0x58;
    set_register32(emu,reg,pop32(emu));
    emu->eip += 1;
}

static void call_rel32(Emulator* emu){
    int32_t diff = get_sign_code32(emu, 1);
    push32(emu, emu->eip + 5);
    emu->eip += (diff + 5);
}

static void ret(Emulator* emu){
    emu->eip = pop32(emu);
}

static void leave(Emulator* emu){
    uint32_t ebp = get_register32(emu,EBP);
    set_register32(emu, ESP, ebp);
    set_register32(emu, EBP, pop32(emu));
    emu->eip += 1;
}

static void push_imm32(Emulator* emu){
    uint32_t value = get_code32(emu, 1);
    push32(emu, value);
    emu->eip += 5;
}

static void push_imm8(Emulator* emu){
    uint8_t value = get_code8(emu, 1);
    push32(emu, value);
    emu->eip += 2;
}

static void add_rm32_imm8(Emulator* emu, ModRM* modrm){
    uint32_t rm32 = get_rm32(emu,modrm);
    uint32_t imm8 = (int32_t)get_sign_code8(emu,0);
    emu->eip += 1;
    set_rm32(emu, modrm, rm32 + imm8);
}

static void cmp_r32_rm32(Emulator* emu){
    emu->eip += 1;
    ModRM modrm;
    parse_modrm(emu, &modrm);
    uint32_t r32 = get_r32(emu, &modrm);
    uint32_t rm32 = get_rm32(emu, &modrm);
    uint64_t result = (uint64_t)r32 - (uint64_t)rm32;
    update_eflags_sub(emu, r32,rm32,result);
}

static void cmp_rm32_imm8(Emulator* emu, ModRM* modrm){
    uint32_t rm32 = get_rm32(emu,modrm);
    uint32_t imm8 = (int32_t)get_sign_code8(emu,0);
    emu->eip += 1;
    uint64_t result = (uint64_t)rm32 - (uint64_t)imm8;
    update_eflags_sub(emu,rm32,imm8,result);
}

static void sub_rm32_imm8(Emulator* emu, ModRM modrm){
    uint32_t rm32 = get_rm32(emu,modrm);
    uint32_t imm8 = (uint32_t)get_sign_code8(emu,0);
    emu->eip += 1;
    uint64_t result = (uint64_t)rm32 - (uint64_t)imm8;
    set_rm32(emu,rm32,imm8,result);
}

static void js(Emulator* emu){
    int diff = is_sign(emu) ? get_sign_code8(emu,1):0;
    emu->eip += (diff + 2);
}

static void jns(Emulator* emu){
    int diff = is_sign(emu) ? 0 : get_sign_code8(emu,1);
    emu->eip += (diff + 2);
}

static void jl(Emulator* emu){
    int diff = (is_sign(emu) != is_overflow(emu)) ? get_sign_code8(emu, 1) : 0;
    emu->eip += (diff + 2);
}

static void jle(Emulator* emu){
    int diff = (is_zero(emu) || (is_sign(emu) != is_overflow(emu))) ? get_sign_code8(emu, 1) : 0;
    emu->eip += (diff + 2);
}

static void in_al_dx(Emulator* emu){
    uint16_t address = get_register32(emu, EDX) & 0xffff;
    uint8_t value = io_in8(address);
    set_register8(emu, AL, value);
    emu->eip += 1;
}

static void out_dx_al(Emulator* emu){
    uint16_t address = get_register32(emu, EDX) & 0xffff;
    uint8_t value = get register32(emu, AL);
    io_out8(address,value);
    emu->eip += 1;
}