#ifndef OPENAURORA_IR_BLAST_H
#define OPENAURORA_IR_BLAST_H
#include <stddef.h>
#include <stdint.h>

/* Fixed record written by oa-ir-blast to its stdout pipe. The helper owns the
 * blocking vendor BLAST_RX call; mod_ir only parses this record on the loop.
 *
 * The ARM EABI pads this struct to 8 bytes (2 tail bytes after flag). Both
 * sides transfer sizeof(record), so the padding is part of the wire format;
 * pin the size and offsets so a layout change cannot silently desync the
 * stream. The helper zeroes the record before filling it. */
struct oa_ir_blast_record {
    uint32_t code;
    uint8_t aux;
    uint8_t flag;
};

_Static_assert(sizeof(struct oa_ir_blast_record) == 8,
               "oa_ir_blast_record must be 8 bytes (ARM tail padding)");
_Static_assert(offsetof(struct oa_ir_blast_record, code) == 0 &&
               offsetof(struct oa_ir_blast_record, aux) == 4 &&
               offsetof(struct oa_ir_blast_record, flag) == 5,
               "oa_ir_blast_record layout changed");

#endif
