#include "core/Timer.h"
//#include "core/Console.h"
#include "core/win32/Windows.h"
#include <cassert>

namespace
{
RDE_FORCEINLINE void Internal_GetTime(__int64& time)
{
	const BOOL success = 
		::QueryPerformanceCounter(reinterpret_cast< LARGE_INTEGER* >(&time));
	assert(success);
	(void) success;
}
} // <anonymous>

namespace rde
{
Timer::Timer()
:	m_startTime(0),
	m_stopTime(0),
	m_running(false)
{
	const BOOL success = ::QueryPerformanceFrequency(reinterpret_cast< LARGE_INTEGER* >(&m_frequency));
	assert(success);
	(void)success;
//	if (!success)
//		Console::Warningf("Error while calling QPF\n");
}

void Timer::Start()
{
	if (!m_running)
	{
		::Internal_GetTime(m_startTime);
		m_running = true;
	}
}

void Timer::Stop()
{
	if (m_running)
	{
		::Internal_GetTime(m_stopTime);
		m_running = false;
	}
}

int Timer::GetTimeInMs() const
{
	LARGE_INTEGER curTime;
	if (m_running)
	{
		const BOOL success = ::QueryPerformanceCounter(&curTime);
		assert(success);
		(void) success;
	}
	else
	{
		curTime.QuadPart = m_stopTime;
	}
	LARGE_INTEGER elapsedTime;
	elapsedTime.QuadPart = curTime.QuadPart - m_startTime;

	const double seconds = double(elapsedTime.QuadPart) / double(m_frequency);

	return int(seconds * 1000.0);
}

int64 Timer::GetTime() const
{
	const int64 curTime = m_running ? Now() : m_stopTime;
	return curTime - m_startTime;
}
int Timer::ToMillis(int64 ticks) const
{
	const double seconds = double(ticks) / m_frequency;
	return int(seconds * 1000.0);
}

int64 Timer::Now()
{
	int64 curTime;
	Internal_GetTime(curTime);
	return curTime;
}
} // rde

