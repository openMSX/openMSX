#ifndef PROBEBREAKPOINT_HH
#define PROBEBREAKPOINT_HH

#include "BreakPointBase.hh"
#include "Observer.hh"

namespace openmsx {

class Debugger;
class ProbeBase;

class ProbeBreakPoint final : public BreakPointBase<ProbeBreakPoint>
                            , private Observer<ProbeBase>
{
public:
	static constexpr std::string_view prefix = "pp#";

public:
	ProbeBreakPoint(TclObject command,
	                TclObject condition,
	                Debugger& debugger,
	                ProbeBase& probe,
	                bool once,
	                unsigned newId = -1);
	~ProbeBreakPoint();

	[[nodiscard]] const ProbeBase& getProbe() const { return probe; }

private:
	// Observer<ProbeBase>
	void update(const ProbeBase& subject) noexcept override;
	void subjectDeleted(const ProbeBase& subject) override;

private:
	Debugger& debugger;
	ProbeBase& probe;
};

} // namespace openmsx

#endif
