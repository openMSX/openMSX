#ifndef HDCOMMAND_HH
#define HDCOMMAND_HH

#include "RecordedCommand.hh"
#include <string>
#include <vector>

namespace openmsx {

class CommandController;
class StateChangeDistributor;
class Scheduler;
class TclObject;
class HD;
class BooleanSetting;

class HDCommand : public RecordedCommand
{
public:
	HDCommand(CommandController& commandController,
	          StateChangeDistributor& stateChangeDistributor,
	          Scheduler& scheduler, HD& hd, BooleanSetting& powerSetting);
	virtual void execute(const std::vector<TclObject>& tokens,
	                     TclObject& result, EmuTime::param time);
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;
	virtual bool needRecord(const std::vector<std::string>& tokens) const;
private:
	HD& hd;
	const BooleanSetting& powerSetting;
};

} // namespace openmsx

#endif
