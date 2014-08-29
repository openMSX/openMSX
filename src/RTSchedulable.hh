#ifndef RTSCHEDULABLE_HH
#define RTSCHEDULABLE_HH

#include "noncopyable.hh"
#include <cstdint>

namespace openmsx {

class RTScheduler;

class RTSchedulable : private noncopyable
{
public:
	virtual void executeRT() = 0;

protected:
	explicit RTSchedulable(RTScheduler& scheduler);
	~RTSchedulable();

	void scheduleRT(uint64_t delta);
	bool cancelRT();
	bool isPendingRT() const;

private:
	RTScheduler& scheduler;
};

} // namespace openmsx

#endif
