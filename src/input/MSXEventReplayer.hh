// $Id$

#ifndef MSXEVENTREPLAYER_HH
#define MSXEVENTREPLAYER_HH

#include "Schedulable.hh"
#include <string>
#include <fstream>

namespace openmsx {

class MSXEventDistributor;

class MSXEventReplayer : private Schedulable
{
public:
	MSXEventReplayer(Scheduler& scheduler,
	                 MSXEventDistributor& eventDistributor_,
	                 const std::string& fileName);

private:
	// Schedulable
	virtual const std::string& schedName() const;
	virtual void executeUntil(EmuTime::param time, int userData);

	void processLogEntry();

	MSXEventDistributor& eventDistributor;
	std::ifstream logFileStream;
	std::string eventString;
};

} // namespace openmsx

#endif // MSXEVENTREPLAYER_HH

