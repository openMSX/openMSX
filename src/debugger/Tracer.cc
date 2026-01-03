#include "Tracer.hh"

#include "Debugger.hh"
#include "ProbeBreakPoint.hh"

#include "Interpreter.hh"
#include "MSXMotherBoard.hh"
#include "ReverseManager.hh"
#include "StateChangeDistributor.hh"
#include "TclArgParser.hh"

#include "Date.hh"
#include "one_of.hh"
#include "stl.hh"
#include "strCat.hh"

#include <cassert>
#include <queue>

namespace openmsx {

Tracer::Tracer(Debugger& debugger)
	: stateChangeDistributor(debugger.getMotherBoard().getStateChangeDistributor())
{
	stateChangeDistributor.registerListener(*this);
}

Tracer::~Tracer()
{
	stateChangeDistributor.unregisterListener(*this);
}

void Tracer::Trace::addEvent(EmuTime t, TraceValue v, bool merge)
{
	assert(events.empty() || t >= events.back().time);

	if (merge && !events.empty() && (events.back().value == v)) {
		return;
	}

	using enum Type;
	auto valueFormat = v.visit(overloaded{
		[](std::monostate)   { return MONOSTATE; },
		[](uint64_t u)       { return (u == one_of(0ull, 1ull)) ? BOOL : INTEGER; },
		[](double)           { return DOUBLE; },
		[](std::string_view) { return STRING; }
	});
	type = std::max(type, valueFormat);
	events.emplace_back(t, std::move(v));
}

void Tracer::Trace::clear()
{
	events.clear();
}

void Tracer::Trace::attachProbe(Debugger& debugger, ProbeBase& probe)
{
	probe.attach(*this);
	motherBoard = &debugger.getMotherBoard();
	description = probe.getDescription();
}

void Tracer::Trace::detachProbe(ProbeBase& probe)
{
	probe.detach(*this);
	motherBoard = nullptr;
	description.clear();
}

void Tracer::Trace::update(const ProbeBase& subject) noexcept
{
	assert(motherBoard);
	if (motherBoard->getReverseManager().isReplaying()) {
		return;
	}
	addEvent(motherBoard->getCurrentTime(), subject.getTraceValue(), false);
}

void Tracer::probeCreated(Debugger& debugger, ProbeBase& probe)
{
	if (auto* trace = findTrace(probe.getName())) {
		trace->attachProbe(debugger, probe);
	}
}

void Tracer::probeRemoved(ProbeBase& probe)
{
	if (auto* trace = findTrace(probe.getName())) {
		trace->detachProbe(probe);
	}
}

void Tracer::selectProbe(Debugger& debugger, std::string_view name)
{
	(void)getOrCreateTrace(debugger, name);
}

void Tracer::unselectProbe(Debugger& debugger, std::string_view name)
{
	dropTrace(debugger, name);
}

void Tracer::transfer(Debugger& oldDebugger, Debugger& newDebugger)
{
	assert(traces.empty());
	auto& oldTracer = oldDebugger.getTracer();
	for (auto& trace : oldTracer.traces) {
		if (trace->isProbe()) {
			if (auto* oldProbe = oldDebugger.findProbe(trace->name)) {
				trace->detachProbe(*oldProbe);
			}
			if (auto* newProbe = newDebugger.findProbe(trace->name)) {
				trace->attachProbe(newDebugger, *newProbe);
			}
		}
		traces.push_back(std::move(trace));
	}
	oldTracer.traces.clear();
}

void Tracer::stopReplay(EmuTime time) noexcept
{
	// when replay stops, drop all future events
	for (auto& trace : traces) {
		auto it = std::ranges::lower_bound(trace->events, time, {}, &Event::time);
		trace->events.erase(it, trace->events.end());
	}
}

Tracer::Trace& Tracer::getOrCreateTrace(Debugger& debugger, std::string_view name)
{
	Trace* trace = [&] {
		auto it = std::ranges::lower_bound(traces, name, {}, &Trace::name);
		if (it != traces.end() && (*it)->name == name) {
			return it->get();
		} else {
			it = traces.insert(it, std::make_unique<Trace>(std::string(name)));
			return it->get();
		}
	}();
	if (auto* probe = debugger.findProbe(name)) {
		trace->attachProbe(debugger, *probe);
	}
	return *trace;
}

Tracer::Trace* Tracer::findTrace(std::string_view name)
{
	auto it = std::ranges::lower_bound(traces, name, {}, &Trace::name);
	return (it != traces.end() && (*it)->name == name) ? it->get() : nullptr;
}

void Tracer::execute(Debugger& debugger, std::span<const TclObject> tokens, TclObject& result, EmuTime time)
{
	auto& cmd = debugger.cmd;
	cmd.checkNumArgs(tokens, Completer::AtLeast{3}, "subcommand ?arg ...?");
	cmd.executeSubCommand(tokens[2].getString(),
		"add",   [&]{ add(debugger, tokens, result, time); },
		"list",  [&]{ list(cmd, tokens, result); },
		"drop",  [&]{ drop(debugger, tokens, result); },
		"probe", [&]{ probe(debugger, tokens, result); });
}

void Tracer::add(Debugger& debugger, std::span<const TclObject> tokens, TclObject& /*result*/, EmuTime time)
{
	if (debugger.getMotherBoard().getReverseManager().isReplaying()) {
		return;
	}

	std::optional<std::string_view> description;
	std::optional<std::string_view> type;
	std::optional<std::string_view> format;
	bool merge = false;
	std::array info = {
		valueArg("-description", description),
		valueArg("-type", type),
		valueArg("-format", format),
		flagArg("-merge", merge),
	};
	auto& cmd = debugger.cmd;
	auto& interp = cmd.getInterpreter();
	auto arguments = parseTclArgs(interp, tokens.subspan(3), info);
	if (arguments.size() != one_of(size_t(1), size_t(2))) {
		interp.wrongNumArgs(3, tokens, "name ?value?");
	}

	Trace& trace = getOrCreateTrace(debugger, arguments[0].getString());
	if (description) {
		trace.description = *description;
	}
	if (type) {
		using enum Tracer::Trace::Type;
		if      (*type == "void")   trace.type = MONOSTATE;
		else if (*type == "bool")   trace.type = BOOL;
		else if (*type == "int")    trace.type = INTEGER;
		else if (*type == "double") trace.type = DOUBLE;
		else if (*type == "string") trace.type = STRING;
		else throw CommandException(
			"Invalid value for -type: ", *type,
			", must be one of void, bool, int, double, string");
	}
	if (format) {
		using enum Tracer::Trace::Format;
		if      (*format == "bin") trace.format = BIN;
		else if (*format == "dec") trace.format = DEC;
		else if (*format == "hex") trace.format = HEX;
		else throw CommandException(
			"Invalid value for -format: ", *format,
			", must be one of bin, dec, hex");
	}

	TraceValue value = [&]{
		if (arguments.size() < 2) {
			return TraceValue{std::monostate{}};
		}
		const auto& val = arguments[1];
		if (auto i = val.getOptionalInt()) {
			return TraceValue{uint64_t(*i)};
		} else if (auto i64 = val.getOptionalInt64()) {
			return TraceValue{uint64_t(*i64)};
		} else if (auto d = val.getOptionalDouble()) {
			return TraceValue{*d};
		} else {
			return TraceValue{val.getString()};
		}
	}();
	trace.addEvent(time, std::move(value), merge);
}

[[nodiscard]] static TclObject toTclObject(const TraceValue& value)
{
	return value.visit(overloaded{
		[](std::monostate)     { return TclObject(); },
		[](uint64_t u)         { return TclObject(u); },
		[](double d)           { return TclObject(d); },
		[](std::string_view s) { return TclObject(s); }
	});
}

void Tracer::list(Command& cmd, std::span<const TclObject> tokens, TclObject& result)
{
	cmd.checkNumArgs(tokens, Completer::Between{3, 4}, "?name?");
	if (tokens.size() == 3) {
		// list all traces
		result.addListElements(std::views::transform(traces, &Trace::name));
	} else {
		// list specific trace
		std::string_view name = tokens[3].getString();
		const Trace* trace = findTrace(name);
		if (!trace) {
			throw CommandException("No such trace: ", name);
		}
		for (const auto& event : trace->events) {
			result.addListElement(makeTclList(
				event.time.toDouble(),
				toTclObject(event.value)));
		}
	}
}

void Tracer::drop(Debugger& debugger, std::span<const TclObject> tokens, TclObject& /*result*/)
{
	auto& cmd = debugger.cmd;
	cmd.checkNumArgs(tokens, Completer::Between{3, 4}, "?name?");
	if (tokens.size() == 3) {
		// drop all traces
		traces.clear();
	} else {
		// drop specific trace
		dropTrace(debugger, tokens[3].getString());
	}
}

void Tracer::dropTrace(Debugger& debugger, std::string_view name)
{
	auto it = std::ranges::lower_bound(traces, name, {}, &Trace::name);
	if (it == traces.end() || (*it)->name != name) {
		// not found, not an error
		return;
	}
	if ((*it)->isProbe()) {
		if (auto* probe = debugger.findProbe(name)) {
			(*it)->detachProbe(*probe);
		}
	}
	traces.erase(it);
}

void Tracer::probe(Debugger& debugger, std::span<const TclObject> tokens, TclObject& /*result*/)
{
	auto& cmd = debugger.cmd;
	cmd.checkNumArgs(tokens, 4, "name");
	Trace& trace = getOrCreateTrace(debugger, tokens[3].getString());
	(void)trace;
}

void Tracer::tabCompletion(const Debugger& debugger, std::vector<std::string>& tokens) const
{
	static constexpr std::array cmds = {
		"add"sv, "list"sv, "drop"sv, "probe"sv,
	};
	auto& cmd = debugger.cmd;
	if (tokens.size() == 3) {
		cmd.completeString(tokens, cmds);
	} else if (tokens.size() == 4) {
		if (tokens[2] == one_of("add"sv, "list"sv, "drop"sv)) {
			cmd.completeString(tokens, std::views::transform(traces, &Trace::name));
		} else if (tokens[2] == "probe"sv) {
			cmd.completeString(tokens, std::views::transform(debugger.probes, &ProbeBase::getName));
		}
	}
}

[[nodiscard]] std::string Tracer::help(std::span<const TclObject> tokens) const
{
	constexpr auto generalHelp =
		"debug trace <subcommand> [<arguments>]\n"
		"  Possible subcommands are:\n"
		"    add    add a new trace data point\n"
		"    list   list the created traces\n"
		"    drop   drop trace data\n"
		"    probe  start collecting probe changes\n"
		"  The arguments are specific for each subcommand.\n"
		"  Type 'help debug trace <subcommand>' for help about a specific subcommand.\n";

	constexpr auto addHelp =
		"debug trace add <name> [<value>] [-merge] [-description <text>] [-type <t>] [-format <f>]\n"
		"  Add a new trace data point to the trace with the given name.\n"
		"  If the trace does not exist yet, it will be created.\n"
		"  The timestamp of the data point is the current emulation time.\n"
		"  The optional value can be an integer, a floating point number,\n"
		"  or a string. If no value is given, a void value is used.\n"
		"\n"
		"  There are also a few flags that influence the behavior:\n"
		"  * -merge: ignore consecutive equal values\n"
		"  * -description <text>: gives a description for this trace\n"
		"  * -type <t>: must be one of: void, bool, int, double, string\n"
		"               E.g. this ensures that a string like '1' is not accidentally interpreted as a bool\n"
		"  * -format <f>: must be one of: bin, dec, hex\n"
		"                 only relevant for type=int\n"
		"  The latter 3 options are typically only used once. The first is typically repeated on every invocation.";

	constexpr auto listHelp =
		"debug trace list [<name>]\n"
		"  Without arguments, list the names of all created traces.\n"
		"  With a name argument, list all data points of the given trace.\n"
		"  Each data point consists of a timestamp and a value.\n"
		"  The timestamp is given as the time (in seconds) since emulation start.\n"
		"  Data points are listed in chronological order.\n";

	constexpr auto dropHelp =
		"debug trace drop [<name>]\n"
		"  Without arguments, drop all traces and their data points.\n"
		"  With a name argument, drop only the trace with the given name.\n";

	constexpr auto probeHelp =
		"debug trace probe <name>\n"
		"  Start collecting changes for the given probe. As if on each change there's an automatic 'debug trace add <name> <value>'.\n";

	constexpr auto unknownHelp =
		"Unknown subcommand, use 'help debug trace' to see a list of valid subcommands.\n";

	auto size = tokens.size();
	assert(size >= 2);
	if (size == 2) {
		return generalHelp;
	} else if (tokens[2] == "add") {
		return addHelp;
	} else if (tokens[2] == "list") {
		return listHelp;
	} else if (tokens[2] == "drop") {
		return dropHelp;
	} else if (tokens[2] == "probe") {
		return probeHelp;
	} else {
		return unknownHelp;
	}
}

void Tracer::exportVCD(zstring_view filename)
{
	// k-way merge using a min-heap over traces to avoid collecting & sorting all events
	std::vector<size_t> idx(traces.size(), 0);
	using HeapEntry = std::pair<uint64_t, size_t>; // time, trace
	std::priority_queue<HeapEntry, std::vector<HeapEntry>, std::greater<HeapEntry>> heap;
	for (size_t i = 0; i < traces.size(); ++i) {
		const auto& t = *traces[i];
		if (!t.events.empty()) {
			heap.emplace(t.events.front().time.toUint64(), i);
		}
	}

	// open file and stream output
	std::ofstream os(filename.c_str());
	if (!os) throw MSXException("Cannot open file for writing: ", filename);

	// header
	os << "$date " << Date::toString(time(nullptr)) << " $end\n"
	      "$version openMSX Tracer exportVCD $end\n"
	      "$timescale 291005 fs $end\n"
	      "$scope module top $end\n";

	// declare variables
	for (size_t i = 0; i < traces.size(); ++i) {
		const auto& t = *traces[i];
		os << "$var ";
		switch (t.getType()) {
			using enum Tracer::Trace::Type;
			case MONOSTATE: os << "event 1";    break;
			case BOOL:      os << "wire 1";     break;
			case INTEGER:   os << "integer 64"; break;
			case DOUBLE:    os << "real 64";    break;
			case STRING:    os << "string 1";   break;
		}
		os << " t" << i << ' ' << t.name << " $end\n";
	}

	os << "$upscope $end\n"
	      "$enddefinitions $end\n";

	std::string tmpBuf;
	auto escapeString = [&](std::string_view str) {
		auto needEscape = [](char c) { return c == '\\' || c <= 32 || c >= 127; };
		auto it = std::ranges::find_if(str, needEscape);
		if (it == str.end()) return str; // fast path

		tmpBuf.assign(str.begin(), it);
		do {
			char c = *it;
			if (needEscape(c)) {
				strAppend(tmpBuf, "\\x", hex_string<2>(c));
			} else {
				tmpBuf += c;
			}
		} while (++it != str.end());
		return std::string_view{tmpBuf};
	};

	// helper: emit a value for given trace format. For events, 'flag' selects 1 (true) or 0 (false).
	auto emitValue = [&](Tracer::Trace::Type type, size_t id, const TraceValue& val, bool flag) {
		switch (type) {
		using enum Tracer::Trace::Type;
		case MONOSTATE:
			os << (flag ? '1' : '0') << 't' << id << '\n';
			break;
		case BOOL: {
			uint64_t v = val.get_if<uint64_t>().value_or(0);
			os << (v ? '1' : '0') << 't' << id << '\n';
			break;
		}
		case INTEGER: {
			uint64_t v = val.get_if<uint64_t>().value_or(0);
			os << 'b' << bin_string(v) << " t" << id << '\n';
			break;
		}
		case DOUBLE: {
			os << 'r';
			val.visit(overloaded{
				[&](std::monostate)   { os << '0'; },
				[&](uint64_t u)       { os << u; },
				[&](double d)         { os << std::setprecision(17) << d; },
				[&](std::string_view) { os << '0'; } // this should not happen
			});
			os << " t" << id << '\n';
			break;
		}
		case STRING: {
			os << "s";
			val.visit(overloaded{
				[] (std::monostate)      { /* nothing */ },
				[&](uint64_t u)          { os << u; },
				[&](double d)            { os << std::setprecision(17) << d; },
				[&](std::string_view sv) { os << escapeString(sv); }
			});
			os << " t" << id << '\n';
			break;
		}
		}
	};

	// write initial values in a $dumpvars block at time 0
	os << "#0\n"
	      "$dumpvars\n";
	for (size_t i = 0; i < traces.size(); ++i) {
		const auto& t = *traces[i];
		TraceValue initVal = std::monostate{};
		if (!t.events.empty()) {
			uint64_t firstNs = t.events.front().time.toUint64();
			if (firstNs == 0) initVal = t.events.front().value;
		}
		emitValue(t.getType(), i, initVal, false);
	}
	os << "$end\n";

	// iterate changes via k-way merge heap
	uint64_t curTime = UINT64_MAX;
	while (!heap.empty()) {
		auto [time, tr] = heap.top();
		heap.pop();
		if (time != curTime) {
			curTime = time;
			os << '#' << time << '\n';
		}

		size_t pos = idx[tr];
		const auto& t = *traces[tr];
		const auto& val = t.events[pos].value;
		emitValue(t.getType(), tr, val, true);

		// advance index and push next event for this trace
		++idx[tr];
		if (idx[tr] < t.events.size()) {
			heap.emplace(t.events[idx[tr]].time.toUint64(), tr);
		}
	}
}

} // namespace openmsx
