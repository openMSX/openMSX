#include "MSXEventReplayer.hh"
#include "InputEventFactory.hh"
#include "MSXEventDistributor.hh"
#include "Event.hh"
#include "InputEvents.hh"
#include "StringOp.hh"
#include "CommandException.hh"

using std::ifstream;
using std::string;

namespace openmsx {

MSXEventReplayer::MSXEventReplayer(Scheduler& scheduler,
			MSXEventDistributor& eventDistributor_,
			const string fileName)
	: Schedulable(scheduler)
	, eventDistributor(eventDistributor_)
{
	logFileStream.open(fileName.c_str());

	processLogEntry();
}

const std::string& MSXEventReplayer::schedName() const
{
        static const string schedName = "MSXEventReplayer";
        return schedName;
}

void MSXEventReplayer::processLogEntry()
{
	string temp;
	getline(logFileStream, temp);
	if (logFileStream.good()) {
		string emutimeStr;
		StringOp::splitOnFirst(temp, " ", emutimeStr, eventString);
	 	EmuTime nextEventTime = EmuTime::makeEmuTime(
				StringOp::stringToUint64(emutimeStr));
		setSyncPoint(nextEventTime);
	} else {
		// file ended or something goes wrong
		logFileStream.close();
	}
}

void MSXEventReplayer::executeUntil(
		const EmuTime& time, int /*userData*/)
{
	try {
	        InputEventFactory::EventPtr eventPtr = 
			InputEventFactory::createInputEvent(eventString);
		eventDistributor.distributeEvent(eventPtr, time);
	} catch (CommandException &e) {
		std::cerr << "Ignoring unknown event " << eventString << ", error was: " << e.getMessage() << std::endl;
	}
	
	processLogEntry();
}

}
