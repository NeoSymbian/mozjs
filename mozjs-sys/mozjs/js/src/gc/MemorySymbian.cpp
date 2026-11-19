/* SPDX-License-Identifier: Apache-2.0
 * NeoSymbian: the GC's pages from one disconnected EKA2 chunk (MemorySymbian.h; docs browser-js.md). The chunk
 * reserves the largest of 1 GiB, 512, 256 and 128 MiB the process can get (its local data area is about 2 GiB); a
 * bitmap of 4 KiB pages records which belong to a mapping. Mapping commits, unmapping decommits: committed memory
 * is real RAM, decommitted pages fault and cost nothing. */

#include "gc/MemorySymbian.h"

// default visibility: Mozilla builds with hidden visibility, and a hidden reference cannot bind to euser's exports
#pragma GCC visibility push(default)
#include <e32debug.h>
#include <e32std.h>
#pragma GCC visibility pop
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

namespace js::gc::symbian {

namespace {

const size_t kPage = 4096;

pthread_mutex_t gLock = PTHREAD_MUTEX_INITIALIZER;
bool gReady;
RChunk gChunk;
uintptr_t gBase;
size_t gPages;
uint32_t* gUsed;  // one bit per page: part of a mapping
size_t gHint;     // where the next search starts (next fit)

bool Used(size_t aPage) { return gUsed[aPage >> 5] & (1u << (aPage & 31)); }

void Mark(size_t aFirst, size_t aCount, bool aUsed) {
  for (size_t p = aFirst; p < aFirst + aCount; ++p) {
    if (aUsed) {
      gUsed[p >> 5] |= 1u << (p & 31);
    } else {
      gUsed[p >> 5] &= ~(1u << (p & 31));
    }
  }
}

bool Reserve() {
  if (gReady) {
    return true;
  }
  static const size_t kSizes[] = {1024u << 20, 512u << 20, 256u << 20,
                                  128u << 20};
  for (size_t size : kSizes) {
    TInt r = gChunk.CreateDisconnectedLocal(0, 0, TInt(size));
    if (r != KErrNone) {
      RDebug::Printf("js gc: no %d MiB chunk (%d)", TInt(size >> 20), r);
    }
    if (r == KErrNone) {
      gPages = size / kPage;
      gUsed = static_cast<uint32_t*>(calloc(gPages / 32, sizeof(uint32_t)));
      if (!gUsed) {
        gChunk.Close();
        return false;
      }
      gBase = reinterpret_cast<uintptr_t>(gChunk.Base());
      gReady = true;
      RDebug::Printf("js gc: %d MiB reserved at %08x", TInt(size >> 20), TUint(gBase));
      return true;
    }
  }
  return false;
}

// a run of `aCount` free pages whose address is `aAlign`-aligned, from `aFrom` on; gPages if none
size_t Find(size_t aCount, size_t aAlign, size_t aFrom) {
  size_t step = aAlign / kPage;
  size_t misalign = (gBase / kPage) % step;
  size_t p = aFrom + (step - (aFrom + misalign) % step) % step;
  for (; p + aCount <= gPages; p += step) {
    size_t q = p;
    while (q < p + aCount && !Used(q)) {
      ++q;
    }
    if (q == p + aCount) {
      return p;
    }
  }
  return gPages;
}

}  // namespace

void* MapAligned(size_t aLength, size_t aAlignment) {
  if (aAlignment < kPage) {
    aAlignment = kPage;
  }
  pthread_mutex_lock(&gLock);
  void* result = nullptr;
  if (Reserve()) {
    size_t count = aLength / kPage;
    size_t page = Find(count, aAlignment, gHint);
    if (page == gPages) {
      page = Find(count, aAlignment, 0);
    }
    TInt r = page < gPages ? gChunk.Commit(TInt(page * kPage), TInt(aLength))
                           : KErrNoMemory;
    if (r == KErrNone) {
      Mark(page, count, true);
      gHint = page + count;
      result = reinterpret_cast<void*>(gBase + page * kPage);
    } else {
      RDebug::Printf("js gc: map %d KiB (align %d KiB) failed (%d)",
                     TInt(aLength >> 10), TInt(aAlignment >> 10), r);
    }
  }
  pthread_mutex_unlock(&gLock);
  return result;
}

void Unmap(void* aRegion, size_t aLength) {
  size_t page = (reinterpret_cast<uintptr_t>(aRegion) - gBase) / kPage;
  pthread_mutex_lock(&gLock);
  gChunk.Decommit(TInt(page * kPage), TInt(aLength));  // pages already decommitted are fine
  Mark(page, aLength / kPage, false);
  if (page < gHint) {
    gHint = page;
  }
  pthread_mutex_unlock(&gLock);
}

bool Decommit(void* aRegion, size_t aLength) {
  TInt offset = TInt(reinterpret_cast<uintptr_t>(aRegion) - gBase);
  pthread_mutex_lock(&gLock);
  TInt r = gChunk.Decommit(offset, TInt(aLength));
  pthread_mutex_unlock(&gLock);
  return r == KErrNone;
}

bool Commit(void* aRegion, size_t aLength) {
  TInt offset = TInt(reinterpret_cast<uintptr_t>(aRegion) - gBase);
  pthread_mutex_lock(&gLock);
  TInt r = gChunk.Commit(offset, TInt(aLength));
  pthread_mutex_unlock(&gLock);
  return r == KErrNone;
}

}  // namespace js::gc::symbian
