/* SPDX-License-Identifier: Apache-2.0
 * NeoSymbian: EKA2 for SpiderMonkey where P.I.P.S. has nothing (SymbianOS.h): a thread's stack from the kernel, the
 * kernel's random numbers, and threads with a cleanup stack. */

#include "mozilla/SymbianOS.h"

// default visibility: Mozilla builds with hidden visibility, and a hidden
// reference cannot bind to euser's exports
#pragma GCC visibility push(default)
#include <e32base.h>
#include <e32debug.h>
#include <e32math.h>
#pragma GCC visibility pop
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void* MozSymbianStackBase(void) {
  TThreadStackInfo info;
  if (RThread().StackInfo(info) != KErrNone) {
    abort();
  }
  return reinterpret_cast<void*>(info.iBase);  // iBase: the top; iLimit: the bottom
}

void MozSymbianRandom(void* aBuffer, size_t aLength) {
  unsigned char* out = static_cast<unsigned char*>(aBuffer);
  while (aLength) {
    TUint32 r = Math::Random();  // the secure generator (WB1: IRQ-timing entropy, SoC seed)
    size_t n = aLength < sizeof r ? aLength : sizeof r;
    memcpy(out, &r, n);
    out += n;
    aLength -= n;
  }
}

void MozSymbianLog(const char* aFormat, ...) {
  char line[200];  // the debug port faults on more than 256 bytes at once
  va_list ap;
  va_start(ap, aFormat);
  vsnprintf(line, sizeof line, aFormat, ap);
  va_end(ap);
  RDebug::Printf("%s", line);
}

size_t MozSymbianMallocUsableSize(const void* aPtr) {
  return aPtr ? size_t(User::AllocLen(aPtr)) : 0;
}

namespace {

struct Start {
  void* (*mMain)(void*);
  void* mArg;
};

void* Trampoline(void* aStart) {
  Start start = *static_cast<Start*>(aStart);
  free(aStart);
  CTrapCleanup* cleanup = CTrapCleanup::New();
  if (!cleanup) {
    abort();
  }
  void* result = start.mMain(start.mArg);
  delete cleanup;
  return result;
}

}  // namespace

int MozSymbianPthreadCreate(pthread_t* aThread, const pthread_attr_t* aAttr,
                            void* (*aMain)(void*), void* aArg) {
  Start* start = static_cast<Start*>(malloc(sizeof(Start)));
  if (!start) {
    return 12;  // ENOMEM
  }
  start->mMain = aMain;
  start->mArg = aArg;
  // P.I.P.S. declares the attributes non-const
  int r = pthread_create(aThread, const_cast<pthread_attr_t*>(aAttr),
                         Trampoline, start);
  if (r) {
    free(start);
  }
  return r;
}
