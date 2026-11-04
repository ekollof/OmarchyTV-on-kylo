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
/* Borrow the first matching type2..4 payload, matching ROMProfile's byte-copy
 * overload. Length and absolute file offset are bounded; no NUL is invented.
 * Missing/wrong type returns NULL/0/present0. Nonzero header+0x20 selects an
 * external data base, unsupported here (ENOTSUP for a matched payload).
 * All outputs stay unchanged on error. Lifetime is the source buffer's. */
int oa_profile_data(const void *data,size_t size,const char *section,const char *key,
                    const void **out,size_t *length,int *present);
/* Case-sensitive signed-byte firmware hash, NUL excluded. */
uint32_t oa_profile_hash(const char *name);
#endif
