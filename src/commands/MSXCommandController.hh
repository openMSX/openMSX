// $Id$

#ifndef MSXCOMMANDCONTROLLER_HH
#define MSXCOMMANDCONTROLLER_HH

#include "CommandController.hh"
#include "MSXEventListener.hh"
#include "StringMap.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class GlobalCommandController;
class Reactor;
class MSXMotherBoard;
class MSXEventDistributor;
class InfoCommand;

class MSXCommandController : public CommandController, private MSXEventListener,
                             private noncopyable
{
public:
	MSXCommandController(GlobalCommandController& globalCommandController,
	                     Reactor& reactor, MSXMotherBoard& motherboard,
	                     MSXEventDistributor& msxEventDistributor,
	                     const std::string& machineID);
	~MSXCommandController();

	GlobalCommandController& getGlobalCommandController();
	InfoCommand& getMachineInfoCommand();
	MSXMotherBoard& getMSXMotherBoard() const;

	Command* findCommand(string_ref name) const;

	/** Returns true iff the machine this controller belongs to is currently
	  * active.
	  */
	bool isActive() const;

	// CommandController
	virtual void   registerCompleter(CommandCompleter& completer,
	                                 string_ref str);
	virtual void unregisterCompleter(CommandCompleter& completer,
	                                 string_ref str);
	virtual void   registerCommand(Command& command,
	                               const std::string& str);
	virtual void unregisterCommand(Command& command,
	                               string_ref str);
	virtual bool hasCommand(string_ref command) const;
	virtual std::string executeCommand(const std::string& command,
	                                   CliConnection* connection = nullptr);
	virtual std::vector<std::string> splitList(const std::string& list);
	virtual void registerSetting(Setting& setting);
	virtual void unregisterSetting(Setting& setting);
	virtual Setting* findSetting(string_ref name);
	virtual void changeSetting(Setting& setting, const std::string& value);
	virtual CliComm& getCliComm();

private:
	std::string getFullName(string_ref name);

	// MSXEventListener
	virtual void signalEvent(const std::shared_ptr<const Event>& event,
	                         EmuTime::param time);

	GlobalCommandController& globalCommandController;
	Reactor& reactor;
	MSXMotherBoard& motherboard;
	MSXEventDistributor& msxEventDistributor;
	const std::string& machineID;
	std::unique_ptr<InfoCommand> machineInfoCommand;

	StringMap<Command*> commandMap;
	StringMap<Setting*> settingMap;
};

} // namespace openmsx

#endif
