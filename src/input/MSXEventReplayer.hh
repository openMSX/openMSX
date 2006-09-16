// $Id$

#ifndef MSXEVENTREPLAYER_HH
#define MSXEVENTREPLAYER_HH

#include "Schedulable.hh"
#include "EmuTime.hh"
#include <string>
#include <iostream>
#include <fstream>

namespace openmsx {

class MSXEventDistributor;

class MSXEventReplayer : private Schedulable
{
public:
	MSXEventReplayer(Scheduler& scheduler,
		MSXEventDistributor& eventDistributor_, 
		const std::string fileName);
private:
	// Schedulable
        virtual const std::string& schedName() const;
	virtual void executeUntil(const EmuTime& time, int userData);

	void processLogEntry();

	std::ifstream logFileStream;
	std::string eventString;

	MSXEventDistributor& eventDistributor;
};

} // namespace openmsx

#endif // MSXEVENTREPLAYER_HH

