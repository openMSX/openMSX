// $Id$

#ifndef __CLICOMMUNICATOR_HH__
#define __CLICOMMUNICATOR_HH__

#include <deque>
#include <string>
#include "Thread.hh"
#include "Schedulable.hh"
#include "Semaphore.hh"

using std::deque;
using std::string;

namespace openmsx {

class CliCommunicator : public Runnable, private Schedulable
{
public:
	CliCommunicator();
	virtual ~CliCommunicator();

	virtual void run();

private:
	virtual void executeUntilEmuTime(const EmuTime& time, int userData);
	virtual const string& schedName() const;

	deque<string> cmds;
	Semaphore lock;
};

} // namespace openmsx

#endif
