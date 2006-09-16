// $Id$

#include "MSXEventRecorderReplayerCLI.hh"
#include "MSXEventRecorder.hh" 
#include "MSXEventReplayer.hh"
#include "MSXMotherBoard.hh"
#include <string>
#include <list>

using std::string;
using std::list;

namespace openmsx {

MSXEventRecorderReplayerCLI::MSXEventRecorderReplayerCLI(CommandLineParser& commandLineParser)
	: recordOption(commandLineParser)
	, replayOption(commandLineParser)
{
	commandLineParser.registerOption("-record", &recordOption);
	commandLineParser.registerOption("-replay", &replayOption);
}

MSXEventRecorderReplayerCLI::~MSXEventRecorderReplayerCLI()
{
}

// Record Option

MSXEventRecorderReplayerCLI::RecordOption::RecordOption(CommandLineParser& parser_)
	: parser(parser_)
{
}

bool MSXEventRecorderReplayerCLI::RecordOption::parseOption(const string &option,
		list<string> &cmdLine)
{
	const string logFileName = getArgument(option, cmdLine);
	eventRecorder.reset(new MSXEventRecorder(parser.getMotherBoard()->getMSXEventDistributor(), logFileName));
	return true;
}

const string& MSXEventRecorderReplayerCLI::RecordOption::optionHelp() const
{
	static const string text("Record all input events to a file");
	return text;
}

// Replay Option

MSXEventRecorderReplayerCLI::ReplayOption::ReplayOption(CommandLineParser& parser_)
	: parser(parser_)
{
}

bool MSXEventRecorderReplayerCLI::ReplayOption::parseOption(const string &option,
		list<string> &cmdLine)
{
	const string logFileName = getArgument(option, cmdLine);
	eventReplayer.reset(new MSXEventReplayer(parser.getMotherBoard()->getScheduler(), parser.getMotherBoard()->getMSXEventDistributor(), logFileName));
	return true;
}

const string& MSXEventRecorderReplayerCLI::ReplayOption::optionHelp() const
{
	static const string text("Replay all input events from a file");
	return text;
}

} // namespace
