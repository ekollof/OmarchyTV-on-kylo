#ifndef OPENAURORA_IR_BLAST_H
#define OPENAURORA_IR_BLAST_H
#include <stdint.h>

/* Fixed record written by oa-ir-blast to its stdout pipe. The helper owns the
 * blocking vendor BLAST_RX call; mod_ir only parses this record on the loop. */
struct oa_ir_blast_record {
    uint32_t code;
    uint8_t aux;
    uint8_t flag;
};

#endif
