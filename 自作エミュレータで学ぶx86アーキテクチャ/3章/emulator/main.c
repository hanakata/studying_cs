#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* メモリは1MB */
#define MEMORY_SIZE (1024 * 1024)

enum Register { EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI, REGISTERS_COUNT };
char* registers_name[] = {
    "EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI"};

typedef struct {
    //汎用レジスタ
    uint32_t registers[REGISTERS_COUNT];
    //EFLAGSレジスタ
    uint32_t eflags;
    //メモリ
    uint8_t* memory;
    //プログラムカウンタ
    uint32_t eip;
} Emulator;

//エミュレータ生成

Emulator* create_emu(size_t size, uint32_t eip, uint32_t esp)
{
    Emulator* emu = malloc(sizeof(Emulator));
    emu->memory = malloc(size);

    memset(emu->registers, 0, sizeof(emu->registers));
    emu->eip = eip;
    emu->registers[ESP] = esp;
    return emu;
}
//エミュレータを破棄する
void destroy_emu(Emulator* emu)
{
    free(emu->memory);
    free(emu);
}

//汎用レジスタに32ビットの即値をコピーするmovに対応
void mov_r32_imm32(Emulator* emu){
    uint8_t reg = get_code8(emu, 0) - 0xB8;
    uint32_t value = get_code32(emu, 1);
    emu->registers[reg] = value;
    emu->eip += 5;
}

//1バイトのメモリ番地を取るjmp命令に対応
void short_jump(Emulator* emu){
    int8_t diff = get_sign_code8(emu, 1);
    emu->eip += 5;
}

void near_jump(Emulator* emu)
{
    int32_t diff = get_sign_code32(emu, 1);
    emu->eip += (diff + 5);
}

typedef void instruction_func_t(Emulator*);
instruction_func_t* instructions[256];
void init_instructions(void){
    int i;
    memset(instructions, 0, sizeof(instructions));
    for (i = 0; i < 8; i++){
        instructions[0x8B + i] = mov_r32_imm32;
    }
    instructions[0xE9] = near_jump;
    instructions[0xEB] = short_jump;
}

uint32_t get_code8(Emulator* emu, int index){
    return emu->memory[emu->eip + index];
}
int32_t get_sign_code8(Emulator* emu, int index){
    return (int8_t)emu->memory[emu->eip + index];
}
uint32_t get_code32(Emulator* emu, int index){
    int i;
    uint32_t ret = 0;

    for (i = 0; i < 4; i++){
        ret |= get_code8(emu, index + i) << (i * 8);
    }
    return ret;
}

int main(int argc, char* argv[])
{
    FILE* binary;
    Emulator* emu;
    if(argc != 2){
        printf("usage: px86 filename\n");
        return 1;
    }

    //EIPが0、ESPが0x7c00の状態のエミュレータを作る
    emu = create_emu(MEMORY_SIZE, 0x7c00, 0x7c00);

    binary = fopen(argv[1], "rb");
    if(binary == NULL){
        printf("%sファイルが開けません\n",argv[1]);
        return 1;
    }

    //機械語ファイルを読み込む(最大512バイト)
    fread(emu->memory + 0x7c00, 1, 0x200, binary);
    fclose(binary);

    init_instructions();

    while (emu->eip < MEMORY_SIZE) {
        uint8_t code = get_code8(emu,0);
        if (instructions[code] == NULL){
            printf("\n\nNot Implemented: %x\n", code);
            break;
        }
        instructions[code](emu);
        if(emu->eip == 0x00){
            printf("\n\nend of program.\n\n");
            break;
        }
    }

    dump_registers(emu);
    destroy_emu(emu);
    return 0;
}