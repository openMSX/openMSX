// $Id$

#ifndef POLLINTERFACE_HH
#define POLLINTERFACE_HH

namespace openmsx {

class Scheduler;

class PollInterface
{
public:
	explicit PollInterface(Scheduler& scheduler);
	virtual ~PollInterface();

	virtual void poll() = 0;

private:
	Scheduler& scheduler;
};

} // namespace openmsx

#endif
