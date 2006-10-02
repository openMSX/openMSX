// $Id$

#ifndef MSXEVENTRECORDER_HH
#define MSXEVENTRECORDER_HH

#include "MSXEventListener.hh"
#include "noncopyable.hh"
#include <fstream>
#include <string>

namespace openmsx {

class MSXEventDistributor;

class MSXEventRecorder : private MSXEventListener, private noncopyable
{
public:
	MSXEventRecorder(MSXEventDistributor& eventDistributor, 
	                 const std::string& fileName);
	virtual ~MSXEventRecorder();

private:
	// EventListener
	virtual void signalEvent(shared_ptr<const Event> event,
                                 const EmuTime& time);

	MSXEventDistributor& eventDistributor;
	std::ofstream logFileStream;
};

} // namespace openmsx

#endif
