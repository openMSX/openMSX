// $Id$

#ifndef MSXEVENTRECORDERREPLAYERCLI_HH
#define MSXEVENTRECORDERREPLAYERCLI_HH

#include <memory>

namespace openmsx {

class CommandLineParser;
class RecordOption;
class ReplayOption;

class MSXEventRecorderReplayerCLI
{
public:
	explicit MSXEventRecorderReplayerCLI(CommandLineParser& cmdLineParser);
	~MSXEventRecorderReplayerCLI();

private:
	std::auto_ptr<RecordOption> recordOption;
	std::auto_ptr<ReplayOption> replayOption;
};

} // namespace openmsx

#endif
