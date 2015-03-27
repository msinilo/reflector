#include "core/CPU.h"
#include "core/win32/Windows.h"

namespace rde
{
double GetCPUTicksPerSecond()
{
	// Possible, but very unlikely data race here if called from multiple threads
	// (access to clockSpeed).
	static double clockSpeed(0.0);
	if (clockSpeed > 0.0)
		return clockSpeed;

	__int64 countsPerSecond(0);
	QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&countsPerSecond));
	LARGE_INTEGER timeStart;
	QueryPerformanceCounter(&timeStart);

	LARGE_INTEGER timeEnd;
	timeEnd.QuadPart = 0;
	static const LONGLONG kWaitCounts = 500000;
	unsigned __int64 ticksStart = __rdtsc();
	while (timeEnd.QuadPart < timeStart.QuadPart + kWaitCounts)
	{
		QueryPerformanceCounter(&timeEnd);
	}
	unsigned __int64 ticksTaken = __rdtsc() - ticksStart;
	const double secondsPassed = double(countsPerSecond) / kWaitCounts;
	clockSpeed = double(ticksTaken) * secondsPassed;
		
	return clockSpeed;
}
} // rde
