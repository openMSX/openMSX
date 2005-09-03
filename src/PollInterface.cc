//$Id$

#include "PollInterface.hh"
#include "Scheduler.hh"

namespace openmsx {

PollInterface::PollInterface(Scheduler& scheduler_)
	: scheduler(scheduler_)
{
	scheduler.registerPoll(*this);
}

PollInterface::~PollInterface()
{
	scheduler.unregisterPoll(*this);
}

} // namespace openmsx
