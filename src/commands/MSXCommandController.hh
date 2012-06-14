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

	Command* findCommand(const std::string& name) const;

	/** Returns true iff the machine this controller belongs to is currently
	  * active.
	  */
	bool isActive() const;

	// CommandController
	virtual void   registerCompleter(CommandCompleter& completer,
	                                 const std::string& str);
	virtual void unregisterCompleter(CommandCompleter& completer,
	                                 const std::string& str);
	virtual void   registerCommand(Command& command,
	                               const std::string& str);
	virtual void unregisterCommand(Command& command,
	                               const std::string& str);
	virtual bool hasCommand(const std::string& command) const;
	virtual std::string executeCommand(const std::string& command,
	                                   CliConnection* connection = 0);
	virtual void splitList(const std::string& list,
	                       std::vector<std::string>& result);
	virtual void registerSetting(Setting& setting);
	virtual void unregisterSetting(Setting& setting);
	virtual Setting* findSetting(const std::string& name);
	virtual void changeSetting(Setting& setting, const std::string& value);
	virtual CliComm& getCliComm();

private:
	std::string getFullName(const std::string& name);

	// MSXEventListener
	virtual void signalEvent(const shared_ptr<const Event>& event,
	                         EmuTime::param time);

	GlobalCommandController& globalCommandController;
	Reactor& reactor;
	MSXMotherBoard& motherboard;
	MSXEventDistributor& msxEventDistributor;
	const std::string& machineID;
	std::auto_ptr<InfoCommand> machineInfoCommand;

	typedef StringMap<Command*> CommandMap;
	CommandMap commandMap;
	typedef StringMap<Setting*> SettingMap;
	SettingMap settingMap;
};

} // namespace openmsx

#endif
