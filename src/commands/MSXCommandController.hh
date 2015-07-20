#ifndef MSXCOMMANDCONTROLLER_HH
#define MSXCOMMANDCONTROLLER_HH

#include "CommandController.hh"
#include "Command.hh"
#include "MSXEventListener.hh"
#include "Setting.hh"
#include "hash_set.hh"
#include "noncopyable.hh"
#include "xxhash.hh"
#include <memory>

namespace openmsx {

class GlobalCommandController;
class Reactor;
class MSXMotherBoard;
class MSXEventDistributor;
class InfoCommand;

class MSXCommandController final
	: public CommandController, private MSXEventListener
	, private noncopyable
{
public:
	MSXCommandController(GlobalCommandController& globalCommandController,
	                     Reactor& reactor, MSXMotherBoard& motherboard,
	                     MSXEventDistributor& msxEventDistributor,
	                     const std::string& machineID);
	~MSXCommandController();

	GlobalCommandController& getGlobalCommandController() {
		return globalCommandController;
	}
	InfoCommand& getMachineInfoCommand() {
		return *machineInfoCommand;
	}
	MSXMotherBoard& getMSXMotherBoard() const {
		return motherboard;
	}
	const std::string& getPrefix() const {
		return machineID;
	}

	Command* findCommand(string_ref name) const;

	/** Returns true iff the machine this controller belongs to is currently
	  * active.
	  */
	bool isActive() const;

	/** Transfer setting values from one machine to another,
	  * used for during 'reverse'. */
	void transferSettings(const MSXCommandController& from);

	// CommandController
	void   registerCompleter(CommandCompleter& completer,
	                         string_ref str) override;
	void unregisterCompleter(CommandCompleter& completer,
	                         string_ref str) override;
	void   registerCommand(Command& command,
	                       const std::string& str) override;
	void unregisterCommand(Command& command,
	                       string_ref str) override;
	bool hasCommand(string_ref command) const override;
	TclObject executeCommand(const std::string& command,
	                         CliConnection* connection = nullptr) override;
	void registerSetting(Setting& setting) override;
	void unregisterSetting(Setting& setting) override;
	void changeSetting(Setting& setting, const TclObject& value) override;
	CliComm& getCliComm() override;
	Interpreter& getInterpreter() override;

private:
	std::string getFullName(string_ref name);

	// MSXEventListener
	void signalEvent(const std::shared_ptr<const Event>& event,
	                 EmuTime::param time) override;

	GlobalCommandController& globalCommandController;
	Reactor& reactor;
	MSXMotherBoard& motherboard;
	MSXEventDistributor& msxEventDistributor;
	std::string machineID;
	std::unique_ptr<InfoCommand> machineInfoCommand;

	struct NameFromCommand {
		const std::string& operator()(const Command* c) const {
			return c->getName();
		}
	};
	hash_set<Command*, NameFromCommand, XXHasher> commandMap;

	struct NameFromSetting {
		const std::string& operator()(const Setting* s) const {
			return s->getName();
		}
	};
	hash_set<Setting*, NameFromSetting, XXHasher> settingMap;
};

} // namespace openmsx

#endif
