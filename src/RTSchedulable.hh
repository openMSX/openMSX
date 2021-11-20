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

	void scheduleRT(uint64_t delta);
	bool cancelRT();
	[[nodiscard]] bool isPendingRT() const;

protected:
	explicit RTSchedulable(RTScheduler& scheduler);
	~RTSchedulable();

private:
	RTScheduler& scheduler;
};

} // namespace openmsx

#endif
