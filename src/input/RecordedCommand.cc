#include "RecordedCommand.hh"

#include "Scheduler.hh"
#include "StateChange.hh"
#include "StateChangeDistributor.hh"
#include "TclObject.hh"

#include "ScopedAssign.hh"
#include "dynarray.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "stl.hh"

#include <ranges>
#include <string>
#include <vector>

namespace openmsx {

RecordedCommand::RecordedCommand(CommandController& commandController_,
                                 StateChangeDistributor& stateChangeDistributor_,
                                 Scheduler& scheduler_,
                                 std::string_view name_)
	: Command(commandController_, name_)
	, stateChangeDistributor(stateChangeDistributor_)
	, scheduler(scheduler_)
	, currentResultObject(&dummyResultObject)
{
	stateChangeDistributor.registerListener(*this);
}

RecordedCommand::~RecordedCommand()
{
	stateChangeDistributor.unregisterListener(*this);
}

void RecordedCommand::execute(std::span<const TclObject> tokens, TclObject& result)
{
	auto time = scheduler.getCurrentTime();
	if (needRecord(tokens)) {
		ScopedAssign sa(currentResultObject, &result);
		stateChangeDistributor.distributeNew<MSXCommandEvent>(time, tokens);
	} else {
		execute(tokens, result, time);
	}
}

bool RecordedCommand::needRecord(std::span<const TclObject> /*tokens*/) const
{
	return true;
}

[[nodiscard]] static std::string_view getBaseName(std::string_view str)
{
	auto pos = str.rfind("::");
	return (pos == std::string_view::npos) ? str : str.substr(pos + 2);
}

void RecordedCommand::signalStateChange(const StateChange& event)
{
	const auto* commandEvent = std::get_if<MSXCommandEvent>(&event);
	if (!commandEvent) return;

	const auto& tokens = commandEvent->getTokens();
	if (getBaseName(tokens[0].getString()) != getName()) return;

	if (needRecord(tokens)) {
		execute(tokens, *currentResultObject, commandEvent->getTime());
	} else {
		// Normally this shouldn't happen. But it's possible in case
		// we're replaying a replay file that has manual edits in the
		// event log. It's crucial for security that we don't blindly
		// execute such commands. We already only execute
		// RecordedCommands, but we also need a strict check that
		// only commands that would be recorded are also replayed.
		// For example:
		//   debug set_bp 0x0038 true {<some-arbitrary-Tcl-command>}
		// The debug write/write_block commands should be recorded and
		// replayed, but via the set_bp it would be possible to
		// execute arbitrary Tcl code.
	}
}

void RecordedCommand::stopReplay(EmuTime /*time*/) noexcept
{
	// nothing
}

} // namespace openmsx
