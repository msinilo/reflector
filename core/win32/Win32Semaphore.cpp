#include "core/Semaphore.h"
#include "core/Console.h"
#include "core/RdeAssert.h"
#include "core/ThreadProfiler.h"
#include "core/win32/Windows.h"
#include <climits>	// INT_MAX
#include <new>

// We dont have explicit impl pointer, instead we cast before use.
// This way we save 4 bytes per instance, which in case of semaphores may add up.
#define SEM_AS_IMPL(mem)	((Impl*)(mem))

#define RDE_SEMAPHORE_DEBUG	0

namespace rde
{
struct Semaphore::Impl
{
	Impl(int initialValue)
	{
		hSemaphore = ::CreateSemaphore(0, initialValue, INT_MAX, 0);
		Invariant();
		if (hSemaphore == 0)
			Console::Warningf("Couldn't create semaphore\n");
	}
	~Impl()
	{
		Invariant();
		const bool ok = ::CloseHandle(hSemaphore) != 0;
		RDE_ASSERT(ok);
		hSemaphore = 0;
	}
	void Invariant()
	{
		RDE_ASSERT(hSemaphore != 0);
	}
	HANDLE	hSemaphore;
};

Semaphore::Semaphore(int initialValue /* = 0 */)
{
	RDE_COMPILE_CHECK(sizeof(Impl) <= sizeof(m_implMem));
	new (m_implMem) Impl(initialValue);
//	ThreadProfiler::AddObject(this);
}
Semaphore::~Semaphore()
{
	ThreadProfiler::AddEvent(ThreadProfiler::Event::SEMAPHORE_DESTROYED, this);
	SEM_AS_IMPL(m_implMem)->~Impl();
}
void Semaphore::WaitInfinite()
{
	ThreadProfiler::AddEvent(ThreadProfiler::Event::SEMAPHORE_WAIT_START, this);
	const int res = ::WaitForSingleObject(SEM_AS_IMPL(m_implMem)->hSemaphore, INFINITE);
	ThreadProfiler::AddEvent(ThreadProfiler::Event::SEMAPHORE_WAIT_END, this);
	RDE_ASSERT(res == WAIT_OBJECT_0);
#if RDE_SEMAPHORE_DEBUG
	if (res != WAIT_OBJECT_0)
		Console::Warningf("Error while waiting for semaphore\n");
#endif
}
bool Semaphore::WaitTimeout(long milliseconds)
{
	ThreadProfiler::AddEvent(ThreadProfiler::Event::SEMAPHORE_WAIT_START, this);
	const int res = ::WaitForSingleObject(SEM_AS_IMPL(m_implMem)->hSemaphore, milliseconds);
	ThreadProfiler::AddEvent(ThreadProfiler::Event::SEMAPHORE_WAIT_END, this);
	return res == WAIT_OBJECT_0;
}
void Semaphore::Signal(int num)
{
	ThreadProfiler::AddEvent(ThreadProfiler::Event::SEMAPHORE_SIGNALLED, this);
	const bool ok = ::ReleaseSemaphore(SEM_AS_IMPL(m_implMem)->hSemaphore, num, 0) != 0;
	RDE_ASSERT(ok);

#if RDE_SEMAPHORE_DEBUG
	if (!ok)
		Console::Warningf("Error while releasing semaphore\n");
#endif
}

} // rde
