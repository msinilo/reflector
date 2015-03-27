#ifndef CORE_ATOMIC_H
#	error "Do not include this file directly, use core/Atomic.h"
#endif

#include "core/win32/Windows.h"
#include <intrin.h>

#pragma intrinsic (_InterlockedExchange)
// Mhm, doesn't seem to be available as intrinsic.
// @TODO: Examine generated code (with function) and decide if to replace with
// handcoded assembly.
//#pragma intrinsic (_InterlockedExchange64)
#pragma intrinsic (_InterlockedCompareExchange)
#pragma intrinsic (_InterlockedCompareExchange64)
#pragma intrinsic (_InterlockedExchangeAdd)
#pragma intrinsic (_InterlockedIncrement, _InterlockedIncrement16)
#pragma intrinsic (_InterlockedDecrement, _InterlockedDecrement16)
#pragma intrinsic (_ReadWriteBarrier)
#pragma intrinsic (_ReadBarrier)
#pragma intrinsic (_WriteBarrier)

namespace rde
{
typedef volatile long	Atomic32;
typedef volatile int64	Atomic64;

#define FN_COMPARE_AND_SWAP(T, ACCREG, XCHGREG) \
	inline T CompareAndSwap(T* dst, T comparand, T exchange) \
	{ \
		T result; \
		{ \
			_asm mov edx, dst \
			_asm mov XCHGREG, exchange \
			_asm mov ACCREG, comparand \
			_asm lock cmpxchg [edx], XCHGREG \
			_asm mov result, ACCREG \
		} \
		CompilerReadWriteBarrier(); \
		return result; \
	}

#define FN_FETCH_AND_ADD_BODY(T, ACCREG, ADDEND) \
	T result; \
	{ \
		_asm mov edx, dst \
		_asm mov ACCREG, ADDEND \
		_asm lock xadd [edx], ACCREG \
		_asm add ACCREG, ADDEND \
		_asm mov result, ACCREG \
	} \
	CompilerReadWriteBarrier(); \
	return result;

#define FN_FETCH_AND_STORE(T, ACCREG)	\
	inline T FetchAndStore(T* dst, T v) \
	{ \
		T result; \
	    { \
			_asm mov edx, dst \
			_asm mov ACCREG, v \
			_asm lock xchg [edx], ACCREG \
			_asm mov result, ACCREG \
		} \
		return result; \
	}

#define FN_INCREMENT(T, ACCREG) \
	inline T Increment(T* dst) { FN_FETCH_AND_ADD_BODY(T, ACCREG, 1); }
#define FN_DECREMENT(T, ACCREG) \
	inline T Decrement(T* dst) { FN_FETCH_AND_ADD_BODY(T, ACCREG, -1); }
#define FN_FETCH_AND_ADD(T, ACCREG)	\
	inline T FetchAndAdd(T* dst, T a) { FN_FETCH_AND_ADD_BODY(T, ACCREG, a); }

// Read/writes will be completed at this point (compiler barrier only!).
inline void CompilerReadWriteBarrier()
{
	_ReadWriteBarrier();
}
// Full memory barrier
inline void MemoryBarrier()
{
	::MemoryBarrier();
}
inline void CompilerReadBarrier()
{
	_ReadBarrier();
}
inline void CompilerWriteBarrier()
{
	_WriteBarrier();
}

template<typename T>
inline T Load_Relaxed(const T& v)
{
	RDE_COMPILE_CHECK(sizeof(T) <= 8);
	T ret = v;
	return ret;
}
template<typename T> 
inline void Store_Relaxed(T& dst, T v)
{
	RDE_COMPILE_CHECK(sizeof(T) <= 8);
	dst = v;
}

template<typename T> 
inline T Load_Acquire(const T& v)
{
	RDE_COMPILE_CHECK(sizeof(T) <= 8);
	T ret = v;
	CompilerReadBarrier();
	return ret;
}
template<typename T> 
inline void Store_Release(T& dst, T v)
{
	RDE_COMPILE_CHECK(sizeof(T) <= 8);
	CompilerWriteBarrier();
	dst = v;
}

template<typename T> 
inline T Load_SeqCst(const T& v)
{
	RDE_COMPILE_CHECK(sizeof(T) <= 4);
	CompilerReadWriteBarrier();
	T ret = v;
	CompilerReadWriteBarrier();
	return ret;
}
template<typename T> 
inline void Store_SeqCst(T& dst, T v)
{
	RDE_COMPILE_CHECK(sizeof(T) <= 4);
	CompilerReadWriteBarrier();
	dst = v;
	CompilerReadWriteBarrier();
}

namespace Interlocked
{
// @note:	32-bit versions implemented via intrinsic, where possible, because
//			it tends to generate better code than inline assembly (as weird as it sounds).

// if (*ptr == comparand)
//	*ptr = exchange
// Return initial value of *ptr.
inline Atomic32 CompareAndSwap(Atomic32* ptr, Atomic32 comparand,
	Atomic32 exchange)
{
	// Arguments order swapped on purpose (Win32 uses xchg/comp, we use comp/xchg).
	return _InterlockedCompareExchange(ptr, exchange, comparand);
}
inline Atomic64 CompareAndSwap(Atomic64* ptr, Atomic64 comparand,
	Atomic64 exchange)
{
	return _InterlockedCompareExchange64(ptr, exchange, comparand);
}
FN_COMPARE_AND_SWAP(char, al, cl);
FN_COMPARE_AND_SWAP(short, ax, cx);

inline Atomic32 FetchAndAdd(Atomic32* ptr, Atomic32 addend)
{
	return _InterlockedExchangeAdd(ptr, addend);
}
FN_FETCH_AND_ADD(char, al);
FN_FETCH_AND_ADD(short, ax);

// *ptr = value, returns previous *ptr.
inline Atomic32 FetchAndStore(Atomic32* ptr, Atomic32 value)
{
	return _InterlockedExchange(ptr, value);
}
inline Atomic64 FetchAndStore(Atomic64* ptr, Atomic64 value)
{
	return InterlockedExchange64(ptr, value);
}
FN_FETCH_AND_STORE(char, al);
FN_FETCH_AND_STORE(short, ax);

// Returns new value of *ptr.
inline Atomic32 Increment(Atomic32* ptr)
{
	return _InterlockedIncrement(ptr);
}
FN_INCREMENT(char, al);
FN_INCREMENT(short, ax);

// Returns new value of *ptr.
inline Atomic32 Decrement(Atomic32* ptr)
{
	return _InterlockedDecrement(ptr);
}
FN_DECREMENT(char, al);
FN_DECREMENT(short, ax);
} // Interlocked
} // rde
