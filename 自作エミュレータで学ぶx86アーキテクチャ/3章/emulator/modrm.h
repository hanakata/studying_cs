#include <stdint.h>

typedef struct {
    uint8_t mod;
    //共用体。それぞれ同じ値が入る。
    union 
    {
        uint8_t opecode;
        uint8_t reg_index;
    };
    uint8_t rm;
    uint8_t sib;
    union{
        int8_t disp8;
        uint32_t disp32;
    };
        
} ModRM;
