#pragma once

// MSVC CRT names and extensions used by the PC port, provided for non-MSVC compilers (macOS).
// clang-cl defines _MSC_VER, so the Windows build never sees any of this.
#ifndef _MSC_VER
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

inline void* _aligned_malloc(size_t size, size_t alignment)
{
	void* p = nullptr;
	return posix_memalign(&p, alignment < sizeof(void*) ? sizeof(void*) : alignment, size) == 0 ? p : nullptr;
}

inline void _aligned_free(void* p)
{
	free(p);
}

// All call sites pass an explicit buffer size, which matches snprintf's signature.
#define sprintf_s snprintf

#define __assume(cond) do { if (!(cond)) __builtin_unreachable(); } while (0)

// Same definition as Pcsx2Defs.h, so whichever is included first wins without a redefinition.
#ifndef __forceinline
#ifdef NDEBUG
#define __forceinline __attribute__((always_inline, unused))
#else
#define __forceinline __attribute__((unused))
#endif
#endif

typedef uint64_t DWORD64;
#endif
