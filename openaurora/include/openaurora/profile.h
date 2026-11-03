#ifndef OPENAURORA_PROFILE_H
#define OPENAURORA_PROFILE_H
#include <stddef.h>
#include <stdint.h>
/* ROMProfile scalar reader. Borrowed EFSL bytes, no allocation/device access.
 * Slash-separated sections skip empty segments. First matching non-section
 * hash wins; only type5 is an inline u32. Missing section/value or another type
 * returns default_value with present=0. EINVAL/EBADMSG leave outputs unchanged.
 * Traversed records/descendant spans are bounded; unrelated blobs/name tables
 * and complete container integrity are not validated. Header+8 is not a file
 * size requirement: the saved swprofile contains the scalar record area only. */
int oa_profile_u32(const void *data,size_t size,const char *section,const char *key,
                   uint32_t default_value,uint32_t *out,int *present);
/* Case-sensitive signed-byte firmware hash, NUL excluded. */
uint32_t oa_profile_hash(const char *name);
#endif
