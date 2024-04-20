#ifndef MSXCOMMANDCONTROLLER_HH
#define MSXCOMMANDCONTROLLER_HH

#include "CommandController.hh"
#include "Command.hh"
#include "InfoCommand.hh"
#include "MSXEventListener.hh"
#include "MSXCliComm.hh"
#include "TemporaryString.hh"
#include "hash_set.hh"
#include "xxhash.hh"
#include <memory>

namespace openmsx {

class GlobalCommandController;
class MSXEventDistributor;
class MSXMotherBoard;
class Reactor;
class Setting;

class MSXCommandController final
	: public CommandController, private MSXEventListener
{
public:
	MSXCommandController(GlobalCommandController& globalCommandController,
	                     Reactor& reactor, MSXMotherBoard& motherboard,
	                     MSXEventDistributor& msxEventDistributor,
	                     const std::string& machineID);
	MSXCommandController(const MSXCommandController&) = delete;
	MSXCommandController(MSXCommandController&&) = delete;
	MSXCommandController& operator=(const MSXCommandController&) = delete;
	MSXCommandController& operator=(MSXCommandController&&) = delete;
	~MSXCommandController();

	[[nodiscard]] GlobalCommandController& getGlobalCommandController() {
		return globalCommandController;
	}
	[[nodiscard]] InfoCommand& getMachineInfoCommand() {
		return *machineInfoCommand;
	}
	[[nodiscard]] MSXMotherBoard& getMSXMotherBoard() const {
		return motherboard;
	}
	[[nodiscard]] const std::string& getPrefix() const {
		return machineID;
	}

	[[nodiscard]] Command* findCommand(std::string_view name) const;
	[[nodiscard]] Setting* findSetting(std::string_view name) const;

	/** Returns true iff the machine this controller belongs to is currently
	  * active.
	  */
	[[nodiscard]] bool isActive() const;

	/** Transfer setting values from one machine to another,
	  * used for during 'reverse'. */
	void transferSettings(const MSXCommandController& from);

	[[nodiscard]] bool hasCommand(std::string_view command) const;

	// CommandController
	void   registerCompleter(CommandCompleter& completer,
	                         std::string_view str) override;
	void unregisterCompleter(CommandCompleter& completer,
	                         std::string_view str) override;
	void   registerCommand(Command& command,
	                       zstring_view str) override;
	void unregisterCommand(Command& command,
	                       std::string_view str) override;
	TclObject executeCommand(zstring_view command,
	                         CliConnection* connection = nullptr) override;
	void registerSetting(Setting& setting) override;
	void unregisterSetting(Setting& setting) override;
	[[nodiscard]] MSXCliComm& getCliComm() override;
	[[nodiscard]] Interpreter& getInterpreter() override;

private:
	[[nodiscard]] TemporaryString getFullName(std::string_view name);

	// MSXEventListener
	void signalMSXEvent(const Event& event,
	                    EmuTime::param time) noexcept override;

	GlobalCommandController& globalCommandController;
	Reactor& reactor;
	MSXMotherBoard& motherboard;
	MSXEventDistributor& msxEventDistributor;
	std::string machineID;
	std::optional<InfoCommand> machineInfoCommand;

	struct NameFromCommand {
		[[nodiscard]] const std::string& operator()(const Command* c) const {
			return c->getName();
		}
	};
	hash_set<Command*, NameFromCommand, XXHasher> commandMap;

	std::vector<Setting*> settings; // unordered
};

} // namespace openmsx

#endif
