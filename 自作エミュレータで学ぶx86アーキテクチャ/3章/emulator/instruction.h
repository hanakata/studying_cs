#ifndef INSTRUCTION_H_
#define INSTRUCTION_H_

#include "emulator.h"

/* 命令セットの初期化関数 */
//関数プロトタイプというもの。
void init_instructions(void);

typedef void instruction_func_t(Emulator*);
/* x86命令の配列、opecode番目の関数がx86の
   opcodeに対応した命令となっている */
   //ここにはinstructions配列の実態はなけどどこかにあるという定義
extern instruction_func_t* instructions[256];

#endif