#ifndef PLUGGINGCONTROLLER_HH
#define PLUGGINGCONTROLLER_HH

#include "RecordedCommand.hh"
#include "InfoTopic.hh"
#include "EmuTime.hh"
#include <memory>
#include <string_view>
#include <vector>

namespace openmsx {

class MSXMotherBoard;
class Connector;
class Pluggable;
class CliComm;

/**
 * Central administration of Connectors and Pluggables.
 */
class PluggingController
{
public:
	explicit PluggingController(MSXMotherBoard& motherBoard);
	~PluggingController();

	/** Connectors must be (un)registered
	  */
	void registerConnector(Connector& connector);
	void unregisterConnector(Connector& connector);

	/** Return the Connector with given name or
	  * nullptr if there is none with this name.
	  */
	[[nodiscard]] Connector* findConnector(std::string_view name) const;

	/** Add a Pluggable to the registry.
	 */
	void registerPluggable(std::unique_ptr<Pluggable> pluggable);

	/** Return the Pluggable with given name or
	  * nullptr if there is none with this name.
	  */
	[[nodiscard]] Pluggable* findPluggable(std::string_view name) const;

	/** Access to the MSX specific CliComm, so that Connectors can get it.
	 */
	[[nodiscard]] CliComm& getCliComm();

	/** Convenience method: get current time.
	 */
	[[nodiscard]] EmuTime::param getCurrentTime() const;

private:
	[[nodiscard]] Connector& getConnector(std::string_view name) const;
	[[nodiscard]] Pluggable& getPluggable(std::string_view name) const;

private:
	MSXMotherBoard& motherBoard;
	std::vector<Connector*> connectors; // no order
	std::vector<std::unique_ptr<Pluggable>> pluggables;

	struct PlugCmd final : RecordedCommand {
		PlugCmd(CommandController& commandController,
			StateChangeDistributor& stateChangeDistributor,
			Scheduler& scheduler);
		void execute(span<const TclObject> tokens, TclObject& result,
			     EmuTime::param time) override;
		[[nodiscard]] std::string help(span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
		[[nodiscard]] bool needRecord(span<const TclObject> tokens) const override;
	} plugCmd;

	struct UnplugCmd final : RecordedCommand {
		UnplugCmd(CommandController& commandController,
			  StateChangeDistributor& stateChangeDistributor,
			  Scheduler& scheduler);
		void execute(span<const TclObject> tokens, TclObject& result,
			     EmuTime::param time) override;
		[[nodiscard]] std::string help(span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} unplugCmd;

	struct PluggableInfo final : InfoTopic {
		explicit PluggableInfo(InfoCommand& machineInfoCommand);
		void execute(span<const TclObject> tokens,
			     TclObject& result) const override;
		[[nodiscard]] std::string help(span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} pluggableInfo;

	struct ConnectorInfo final : InfoTopic {
		explicit ConnectorInfo(InfoCommand& machineInfoCommand);
		void execute(span<const TclObject> tokens,
			     TclObject& result) const override;
		[[nodiscard]] std::string help(span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} connectorInfo;

	struct ConnectionClassInfo final : InfoTopic {
		explicit ConnectionClassInfo(InfoCommand& machineInfoCommand);
		void execute(span<const TclObject> tokens,
			     TclObject& result) const override;
		[[nodiscard]] std::string help(span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} connectionClassInfo;
};

} // namespace openmsx

#endif
