#ifndef CORE_THREAD_EVENT_H
#	error "Do not include directly, only via core/ThreadEvent.h"
#endif

namespace rde
{
// Manual reset event.
class ThreadEvent
{
public:
	enum ResetType
	{
		AUTO_RESET,
		MANUAL_RESET
	};

	explicit ThreadEvent(ResetType resetType = AUTO_RESET, bool initialState = false);
	~ThreadEvent();

	void Signal();
	void Reset();
	void WaitInfinite() const;
	bool WaitTimeout(long milliseconds) const;

	void* GetSystemRepresentation() const;
private:
	void*	m_hEvent;
};
}
