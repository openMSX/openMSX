#include "RecordedCommand.hh"
#include "StateChangeDistributor.hh"
#include "TclObject.hh"
#include "Scheduler.hh"
#include "StateChange.hh"
#include "ScopedAssign.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "stl.hh"
#include "view.hh"
#include "xrange.hh"

using std::vector;
using std::string;

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

void RecordedCommand::execute(span<const TclObject> tokens, TclObject& result)
{
	auto time = scheduler.getCurrentTime();
	if (needRecord(tokens)) {
		ScopedAssign sa(currentResultObject, &result);
		stateChangeDistributor.distributeNew<MSXCommandEvent>(time, tokens);
	} else {
		execute(tokens, result, time);
	}
}

bool RecordedCommand::needRecord(span<const TclObject> /*tokens*/) const
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
	const auto* commandEvent = dynamic_cast<const MSXCommandEvent*>(&event);
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

void RecordedCommand::stopReplay(EmuTime::param /*time*/) noexcept
{
	// nothing
}


// class MSXCommandEvent

MSXCommandEvent::MSXCommandEvent(EmuTime::param time_, span<const TclObject> tokens_)
	: StateChange(time_)
	, tokens(tokens_.size())
{
	for (auto i : xrange(tokens.size())) {
		tokens[i] = tokens_[i];
	}
}

template<typename Archive>
void MSXCommandEvent::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<StateChange>(*this);

	// serialize vector<TclObject> as vector<string>
	vector<string> str;
	if constexpr (!Archive::IS_LOADER) {
		str = to_vector(view::transform(
			tokens, [](auto& t) { return string(t.getString()); }));
	}
	ar.serialize("tokens", str);
	if constexpr (Archive::IS_LOADER) {
		assert(tokens.empty());
		size_t n = str.size();
		tokens = dynarray<TclObject>(n);
		for (auto i : xrange(n)) {
			tokens[i] = TclObject(str[i]);
		}
	}
}
REGISTER_POLYMORPHIC_CLASS(StateChange, MSXCommandEvent, "MSXCommandEvent");

} // namespace openmsx
