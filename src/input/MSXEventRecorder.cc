// $Id$

#include "MSXEventRecorder.hh"
#include "MSXEventDistributor.hh"
#include "openmsx.hh"
#include "Event.hh"
#include "EmuTime.hh"
#include "FileException.hh"
#include <iostream>

using std::vector;
using std::string;

namespace openmsx {

MSXEventRecorder::MSXEventRecorder(MSXEventDistributor& eventDistributor_,
		const string fileName)
	: eventDistributor(eventDistributor_)
{
	logFileStream.open(fileName.c_str());

	if (!logFileStream.good()) throw FileException("Can't open file " + fileName + " to log to");

	eventDistributor.registerEventListener(*this);
}

MSXEventRecorder::~MSXEventRecorder()
{
	eventDistributor.unregisterEventListener(*this);
}

void MSXEventRecorder::signalEvent(shared_ptr<const Event> event,
					const EmuTime& time)
{
	logFileStream << time << ' ' << event->toString() << std::endl;	
}


} // namespace openmsx
