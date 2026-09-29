#pragma once

// MSVC CRT names and extensions used by the PC port, provided for non-MSVC compilers (macOS).
// clang-cl defines _MSC_VER, so the Windows build never sees any of this.
#ifndef _MSC_VER
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#include <malloc/malloc.h>
#endif

// Same guard as TextureUpload's Pcsx2Defs.h, which carries an identical shim.
#ifndef MSVC_COMPAT_ALIGNED_MALLOC
#define MSVC_COMPAT_ALIGNED_MALLOC
inline void* _aligned_malloc(size_t size, size_t alignment)
{
	void* p = nullptr;
	return posix_memalign(&p, alignment < sizeof(void*) ? sizeof(void*) : alignment, size) == 0 ? p : nullptr;
}

inline void _aligned_free(void* p)
{
	free(p);
}
#endif

#ifdef __APPLE__
inline void* _aligned_realloc(void* p, size_t size, size_t alignment)
{
	if (!p) {
		return _aligned_malloc(size, alignment);
	}

	if (size == 0) {
		_aligned_free(p);
		return nullptr;
	}

	void* pNew = _aligned_malloc(size, alignment);
	if (pNew) {
		const size_t oldSize = malloc_size(p);
		memcpy(pNew, p, oldSize < size ? oldSize : size);
		_aligned_free(p);
	}
	return pNew;
}
#endif

inline int sprintf_s(char* buffer, size_t size, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	const int result = vsnprintf(buffer, size, format, args);
	va_end(args);
	return result;
}

template<size_t N>
int sprintf_s(char (&buffer)[N], const char* format, ...)
{
	va_list args;
	va_start(args, format);
	const int result = vsnprintf(buffer, N, format, args);
	va_end(args);
	return result;
}

inline int strcpy_s(char* dst, size_t size, const char* src)
{
	snprintf(dst, size, "%s", src);
	return 0;
}

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
