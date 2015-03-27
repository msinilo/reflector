#include "core/Mutex.h"
#include "core/RdeAssert.h"
#include "core/ThreadProfiler.h"
#include "core/win32/Windows.h"
#include <new>

namespace rde
{
struct Mutex::Impl
{
	Impl(long spinCount)
	{
		if (spinCount > 0)
			::InitializeCriticalSectionAndSpinCount(&m_criticalSection, spinCount);
		else
			::InitializeCriticalSection(&m_criticalSection);
		//m_locked = false;
	}
	~Impl()
	{
		//RDE_ASSERT(!m_locked);
		::DeleteCriticalSection(&m_criticalSection);
	}

	CRITICAL_SECTION	m_criticalSection;
	//bool				m_locked;
};

Mutex::Mutex(long spinCount)
{
	RDE_COMPILE_CHECK(sizeof(Impl) <= sizeof(m_implMem));
	m_impl = new(m_implMem) Impl(spinCount);
}
Mutex::~Mutex()
{
	ThreadProfiler::AddEvent(ThreadProfiler::Event::MUTEX_DESTROYED, this);
	m_impl->~Impl();
	m_impl = 0;
}

void Mutex::Acquire() const
{
	//RDE_ASSERT(!m_impl->m_locked && "Recursive lock detected");
	//ThreadProfiler::AddEvent(ThreadProfiler::Event::MUTEX_WAIT_START, this);
	::EnterCriticalSection(&m_impl->m_criticalSection);
	//ThreadProfiler::AddEvent(ThreadProfiler::Event::MUTEX_WAIT_END, this);
	//m_impl->m_locked = true;
}
bool Mutex::TryAcquire() const
{
	//RDE_ASSERT(!m_impl->m_locked && "Recursive lock detected");
	return ::TryEnterCriticalSection(&m_impl->m_criticalSection) != FALSE;
//		m_impl->m_locked = true;
//	return m_impl->m_locked;
}
void Mutex::Release() const
{
	//RDE_ASSERT(m_impl->m_locked && "Trying to release mutex that's not acquired");
	::LeaveCriticalSection(&m_impl->m_criticalSection);
	//ThreadProfiler::AddEvent(ThreadProfiler::Event::MUTEX_RELEASED, this);
	//m_impl->m_locked = false;
}
bool Mutex::IsLocked() const
{
	return false;//m_impl->m_locked;
}
void* Mutex::GetSystemRepresentation() const
{
	return &m_impl->m_criticalSection;
}

} // rde
