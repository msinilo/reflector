#include "core/ThreadEvent.h"
#include "core/Console.h"
#include "core/RdeAssert.h"
#include "core/ThreadProfiler.h"
#include "core/win32/Windows.h"

namespace rde
{
ThreadEvent::ThreadEvent(ResetType resetType, bool initialState)
{
	const BOOL manualReset = (resetType == MANUAL_RESET);
	m_hEvent = ::CreateEvent(0, manualReset, initialState, 0);
	RDE_ASSERT(m_hEvent != 0);
	if (m_hEvent == 0)
		Console::Warningf("Couldn't create event\n");
}
ThreadEvent::~ThreadEvent()
{
	if (m_hEvent && m_hEvent != INVALID_HANDLE_VALUE)
	{
		const bool ok = (::CloseHandle(m_hEvent) != 0);
		RDE_ASSERT(ok);
	}
	m_hEvent = 0;
}
void ThreadEvent::Signal()
{
	RDE_ASSERT(m_hEvent != 0);
	const bool ok = ::SetEvent(m_hEvent) != 0;
	RDE_ASSERT(ok);
}
void ThreadEvent::Reset()
{
	RDE_ASSERT(m_hEvent != 0);
	const bool ok = ::ResetEvent(m_hEvent) != 0;
	RDE_ASSERT(ok);
}
void ThreadEvent::WaitInfinite() const
{
	RDE_ASSERT(m_hEvent != 0);
	ThreadProfiler::AddEvent(ThreadProfiler::Event::EVENT_WAIT_START, this);
	const DWORD result = ::WaitForSingleObject(m_hEvent, INFINITE);
	ThreadProfiler::AddEvent(ThreadProfiler::Event::EVENT_WAIT_END, this);
	RDE_ASSERT(result == WAIT_OBJECT_0);
}
bool ThreadEvent::WaitTimeout(long milliseconds) const
{
	RDE_ASSERT(m_hEvent != 0);
	ThreadProfiler::AddEvent(ThreadProfiler::Event::EVENT_WAIT_START, this);
	const DWORD result = WaitForSingleObject(m_hEvent, milliseconds);
	ThreadProfiler::AddEvent(ThreadProfiler::Event::EVENT_WAIT_END, this);
	RDE_ASSERT(result == WAIT_TIMEOUT || result == WAIT_OBJECT_0);
	return result == WAIT_OBJECT_0;
}
void* ThreadEvent::GetSystemRepresentation() const
{
	return m_hEvent;
}

} // rde
