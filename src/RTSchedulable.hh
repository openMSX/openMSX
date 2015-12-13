#ifndef RTSCHEDULABLE_HH
#define RTSCHEDULABLE_HH

#include <cstdint>

namespace openmsx {

class RTScheduler;

class RTSchedulable
{
public:
	RTSchedulable(const RTSchedulable&) = delete;
	RTSchedulable& operator=(const RTSchedulable&) = delete;

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
