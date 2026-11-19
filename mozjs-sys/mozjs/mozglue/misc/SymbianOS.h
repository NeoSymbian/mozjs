/* SPDX-License-Identifier: Apache-2.0
 * NeoSymbian: the few EKA2 facts SpiderMonkey needs that P.I.P.S. does not give (SymbianOS.cpp). No EKA2 types here,
 * so Mozilla's sources never see the e32 headers. */

#ifndef mozilla_SymbianOS_h
#define mozilla_SymbianOS_h

#include <pthread.h>
#include <stddef.h>

#include "mozilla/Types.h"

extern "C" {

// The current thread's stack: its highest address (the stack grows down).
MFBT_API void* MozSymbianStackBase(void);

// `aLength` random bytes from the kernel's secure generator (Math::Random).
MFBT_API void MozSymbianRandom(void* aBuffer, size_t aLength);

// The usable size of a malloc'd block: P.I.P.S.'s malloc is the EXE's heap
// (User::Alloc), so its cells' length (User::AllocLen).
MFBT_API size_t MozSymbianMallocUsableSize(const void* aPtr);

// pthread_create, with the new thread given a cleanup stack first: P.I.P.S.
// panics (E32USER-CBase 69) on a thread without one.
MFBT_API int MozSymbianPthreadCreate(pthread_t* aThread,
                                     const pthread_attr_t* aAttr,
                                     void* (*aMain)(void*), void* aArg);
}

#endif  // mozilla_SymbianOS_h
