// $Id$

#ifndef __CLICOMMUNICATOR_HH__
#define __CLICOMMUNICATOR_HH__

#include "Thread.hh"


namespace openmsx {

class CliCommunicator : public Runnable
{
	public:
		virtual void run();
};

} // namespace openmsx

#endif
