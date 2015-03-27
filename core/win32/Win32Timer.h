#ifndef CORE_TIMER_H
#	error "Do not include directly, only via core/Timer.h"
#endif

namespace rde
{

// Win32 timer.
class Timer
{
public:
	Timer();

	void Start();
	void Stop();

	int GetTimeInMs() const;
	// Time in internal units.
	int64 GetTime() const;
	int ToMillis(int64 ticks) const;
	static int64 Now();

	bool IsRunning() const { return m_running; }

private:
	__int64 m_startTime;
	__int64 m_stopTime;
	__int64 m_frequency;
	bool	m_running;
};

} // namespaces
