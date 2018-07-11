#ifndef MSXCOMMANDCONTROLLER_HH
#define MSXCOMMANDCONTROLLER_HH

#include "CommandController.hh"
#include "Command.hh"
#include "MSXEventListener.hh"
#include "Setting.hh"
#include "hash_set.hh"
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
{
public:
	MSXCommandController(const MSXCommandController&) = delete;
	MSXCommandController& operator=(const MSXCommandController&) = delete;

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

	Command* findCommand(string_view name) const;

	/** Returns true iff the machine this controller belongs to is currently
	  * active.
	  */
	bool isActive() const;

	/** Transfer setting values from one machine to another,
	  * used for during 'reverse'. */
	void transferSettings(const MSXCommandController& from);

	// CommandController
	void   registerCompleter(CommandCompleter& completer,
	                         string_view str) override;
	void unregisterCompleter(CommandCompleter& completer,
	                         string_view str) override;
	void   registerCommand(Command& command,
	                       const std::string& str) override;
	void unregisterCommand(Command& command,
	                       string_view str) override;
	bool hasCommand(string_view command) const override;
	TclObject executeCommand(const std::string& command,
	                         CliConnection* connection = nullptr) override;
	void registerSetting(Setting& setting) override;
	void unregisterSetting(Setting& setting) override;
	CliComm& getCliComm() override;
	Interpreter& getInterpreter() override;

private:
	std::string getFullName(string_view name);

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

	std::vector<Setting*> settings; // unordered
};

} // namespace openmsx

#endif
