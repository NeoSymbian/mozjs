/* SPDX-License-Identifier: Apache-2.0
 * NeoSymbian: the GC's pages from one disconnected EKA2 chunk (MemorySymbian.cpp). P.I.P.S. has no anonymous mmap
 * and its mprotect does nothing; a disconnected RChunk reserves address space and commits/decommits any page in it,
 * which is what Memory.cpp needs. No EKA2 types here, so SpiderMonkey's sources never see the e32 headers. */

#ifndef gc_MemorySymbian_h
#define gc_MemorySymbian_h

#include <stddef.h>

namespace js::gc::symbian {

// Committed (zeroed, read/write) pages: `length` bytes at an `alignment`-aligned address (both multiples of 4 KiB),
// or null when the reservation is full or memory is out.
void* MapAligned(size_t length, size_t alignment);
// Decommit and give back `length` bytes at `region` (any page range of a MapAligned result).
void Unmap(void* region, size_t length);
// Decommit pages, which then fault until committed again; their address space stays ours.
bool Decommit(void* region, size_t length);
// Commit decommitted pages again (zeroed).
bool Commit(void* region, size_t length);

}  // namespace js::gc::symbian

#endif  // gc_MemorySymbian_h
