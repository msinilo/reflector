#include "core/Thread.h"
#include "core/Console.h"
#include "core/RdeAssert.h"
#include "core/ThreadEvent.h"
#include "core/ThreadProfiler.h"
#include "core/win32/Windows.h"
#include <new>
#include <process.h>		// beginthreadex

namespace rde
{
struct Thread::Impl
{
	Impl(): hThread(0) {}
	void Run()
	{
		evtThreadStarted.Signal();
		mdelegate();
	}
	HANDLE				hThread;
	Thread::Delegate	mdelegate;
	rde::ThreadEvent	evtThreadStarted;
};
} // rde

namespace
{
__declspec(thread) const char*		s_currentThreadName(0);

// See http://www.codeproject.com/KB/threads/Name_threads_in_debugger.aspx
// and/or http://msdn2.microsoft.com/en-us/library/xcb2z8hs.aspx
typedef struct tagTHREADNAME_INFO
{
  DWORD dwType; // must be 0x1000
  LPCSTR szName; // pointer to name (in user addr space)
  DWORD dwThreadID; // thread ID (-1=caller thread)
  DWORD dwFlags; // reserved for future use, must be zero
} THREADNAME_INFO;
void SetThreadDebugName(DWORD threadId, const char* name)
{
	s_currentThreadName = name;

	// No need to set thread name, if there's no debugger attached.
	if (!IsDebuggerPresent())
		return;

	THREADNAME_INFO info;
	{
		info.dwType = 0x1000;
		info.szName = name;
		info.dwThreadID = threadId;
		info.dwFlags = 0;
	}
	__try
	{
		RaiseException(0x406D1388, 0, sizeof(info)/sizeof(DWORD), (DWORD*)&info);
	}
	__except (EXCEPTION_CONTINUE_EXECUTION)
	{
	}				
} 

unsigned __stdcall Thread_Run(LPVOID arg)
{
	rde::Thread::Impl* impl = static_cast<rde::Thread::Impl*>(arg);
	impl->Run();
	rde::ThreadProfiler::SubmitEvents();
	s_currentThreadName = 0;
	return 0;
}
static int ToWin32ThreadPriority(rde::Thread::Priority pri)
{
	if (pri == rde::Thread::PRIORITY_NORMAL)
		return THREAD_PRIORITY_NORMAL;
	if (pri == rde::Thread::PRIORITY_LOW)
		return THREAD_PRIORITY_BELOW_NORMAL;
	return THREAD_PRIORITY_ABOVE_NORMAL;
}
} // namespace

namespace rde
{
Thread::Thread()
{
	RDE_COMPILE_CHECK(sizeof(Impl) <= sizeof(m_implMem));
	m_impl = new(m_implMem) Impl();
}
Thread::~Thread()
{
	if (m_impl->hThread != 0)
		Stop();
}

bool Thread::Start(const Delegate& delegate, unsigned int stackSize /*= 4096*/,
		Priority priority /*= PRIORITY_NORMAL*/)
{
	RDE_ASSERT(!IsRunning());
	m_impl->mdelegate = delegate;
	m_impl->hThread = reinterpret_cast<HANDLE>(
		::_beginthreadex(0, stackSize, &Thread_Run, m_impl, 0, 0));
	RDE_ASSERT(m_impl->hThread);
	if (m_impl->hThread == 0)
		Console::Warningf("Couldn't start thread");
	else
	{
		::SetThreadPriority(m_impl->hThread, ::ToWin32ThreadPriority(priority));
		ThreadProfiler::AddEvent(ThreadProfiler::Event::THREAD_STARTED, m_impl->hThread);
		m_impl->evtThreadStarted.WaitInfinite();
	}
	return m_impl->hThread != 0;
}

void Thread::Stop()
{
	RDE_ASSERT(IsRunning());
//	m_impl->evtThreadStarted.WaitInfinite();
	const DWORD rv = ::WaitForSingleObject(m_impl->hThread, INFINITE);
	RDE_ASSERT(rv == WAIT_OBJECT_0);
	if (rv == WAIT_OBJECT_0)
	{
		const BOOL ok = ::CloseHandle(m_impl->hThread);
		RDE_ASSERT(ok);
		if (!ok)
			Console::Warningf("Couldn't close thread (%p)\n", m_impl->hThread);

		m_impl->hThread = 0;
	}
}

void Thread::Wait()
{
	::WaitForSingleObject(m_impl->hThread, INFINITE);
}

bool Thread::IsRunning() const
{
	return m_impl->hThread != 0;
}

void Thread::SetAffinityMask(uint32 affinityMask)
{
	::SetThreadAffinityMask(m_impl->hThread, affinityMask);
}

void Thread::SetName(const char* name)
{
	::SetThreadDebugName(Thread::GetCurrentThreadId(), name);
}
const char* Thread::GetCurrentThreadName()
{
	return ::s_currentThreadName;
}
int Thread::GetCurrentThreadId()
{
	return ::GetCurrentThreadId();
}
void Thread::Sleep(long millis)
{
	ThreadProfiler::AddEvent(ThreadProfiler::Event::SLEEP);
	::Sleep(millis);
}
void Thread::YieldCurrentThread()
{
	::SwitchToThread();
}

uint32 Thread::GetProcessAffinityMask()
{
	DWORD_PTR processAffinityMask(0);
	DWORD_PTR systemAffinityMask(0);
	::GetProcessAffinityMask(::GetCurrentProcess(), &processAffinityMask, &systemAffinityMask);
	RDE_ASSERT(processAffinityMask != 0 && "Unable to get affinity mask for the process");
	return uint32(processAffinityMask);
}

} // rde
