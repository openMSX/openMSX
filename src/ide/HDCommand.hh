// $Id$

#ifndef HDCOMMAND_HH
#define HDCOMMAND_HH

#include "RecordedCommand.hh"
#include <string>
#include <vector>

namespace openmsx {

class MSXCommandController;
class MSXEventDistributor;
class Scheduler;
class TclObject;
class HD;
class MSXCliComm;

class HDCommand : public RecordedCommand
{
public:
	HDCommand(MSXCommandController& msxCommandController,
	          MSXEventDistributor& msxEventDistributor,
	          Scheduler& scheduler, MSXCliComm& cliComm, HD& hd);
	virtual void execute(const std::vector<TclObject*>& tokens,
	                     TclObject& result, const EmuTime& time);
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

private:
	HD& hd;
	MSXCliComm& cliComm;
};

} // namespace openmsx

#endif
