#ifndef TRACER_HH
#define TRACER_HH

#include "EmuTime.hh"
#include "StateChangeListener.hh"
#include "TclObject.hh"
#include "TraceValue.hh"

#include "Observer.hh"

#include <memory>

namespace openmsx {

class Command;
class Debugger;
class MSXMotherBoard;
class ProbeBase;
class StateChangeDistributor;

class Tracer final : public StateChangeListener
{
public:
	struct Event {
		EmuTime time;
		TraceValue value;
	};
	struct Trace final : Observer<ProbeBase> {
		// Format of values seen so far. Ordered from specific to general:
		enum class Format : uint8_t { MONOSTATE = 0, BOOL = 1, INTEGER = 2, DOUBLE = 3, STRING = 4 };

		explicit Trace(std::string name_) : name(std::move(name_)) {}

		void addEvent(EmuTime t, TraceValue v);
		void attachProbe(Debugger& debugger, ProbeBase& probe);
		void detachProbe(ProbeBase& probe);
		void update(const ProbeBase& subject) noexcept override;

		[[nodiscard]] bool isBool() const { return format == Format::BOOL; }
		[[nodiscard]] Format getFormat() const { return format; }
		[[nodiscard]] bool isProbe() const { return motherBoard != nullptr; }
		[[nodiscard]] bool isUserTrace() const { return !isProbe(); }

		std::string name;
		std::vector<Event> events;
		MSXMotherBoard* motherBoard = nullptr; // non-nullptr if attached to a Probe
		Format format = Format::MONOSTATE;
	};

public:
	explicit Tracer(Debugger& debugger);
	~Tracer();
	Tracer(const Tracer&) = delete;
	Tracer(Tracer&&) = delete;
	Tracer& operator=(const Tracer&) = delete;
	Tracer& operator=(Tracer&&) = delete;

	void execute(Debugger& debugger, std::span<const TclObject> tokens, TclObject& result, EmuTime time);
	void tabCompletion(const Debugger& debugger, std::vector<std::string>& tokens) const;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const;

	[[nodiscard]] const auto& getTraces() const { return traces; }
	void probeCreated(Debugger& debugger, ProbeBase& probe);
	void probeRemoved(ProbeBase& probe);
	void selectProbe  (Debugger& debugger, std::string_view name);
	void unselectProbe(Debugger& debugger, std::string_view name);

	void exportVCD(zstring_view filename);

	void transfer(Debugger& oldDebugger, Debugger& newDebugger);

private:
	[[nodiscard]] Trace& getOrCreateTrace(Debugger& debugger, std::string_view name);
	[[nodiscard]] Trace* findTrace(std::string_view name);
	void add  (Debugger& debugger, std::span<const TclObject> tokens, TclObject& result, EmuTime time);
	void list (Command& cmd, std::span<const TclObject> tokens, TclObject& result);
	void drop (Debugger& debugger, std::span<const TclObject> tokens, TclObject& result);
	void probe(Debugger& debugger, std::span<const TclObject> tokens, TclObject& result);
	void dropTrace(Debugger& debugger, std::string_view name);

	// StateChangeListener
	void signalStateChange(const StateChange& /*event*/) override {}
	void stopReplay(EmuTime time) noexcept override;

private:
	StateChangeDistributor& stateChangeDistributor;
	std::vector<std::unique_ptr<Trace>> traces; // sorted on name
};

} // namespace openmsx

#endif
