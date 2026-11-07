/* Compile-time/ELF probe: the ARM object must expose an 8-byte record. */
#include "openaurora/ir_blast.h"

char ir_record_size_probe[sizeof(struct oa_ir_blast_record)];
