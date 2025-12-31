#ifndef IMGUI_TRACE_VIEWER_HH
#define IMGUI_TRACE_VIEWER_HH

#include "ImGuiPart.hh"

#include "EmuDuration.hh"
#include "EmuTime.hh"
#include "Tracer.hh"

#include "gl_vec.hh"

namespace openmsx {

struct Convertor;

class TimelineFormatter
{
public:
	// A timeline label with its position and formatted text
	struct Label {
		double value;     // Numeric position (e.g., timestamp)
		std::string text; // Formatted label text (e.g., "1.23e+06" or "12.345")
	};

	std::pair<const std::vector<Label>&, double> calc(double from, double to, double numLabels, double unitFactor);

private:
	struct Params {
		double from = 0.0;
		double to = 0.0;
		double numLabels = 0.0;
		double unitFactor = 1.0;
		bool operator==(const Params&) const = default;
	} prevParams;

	std::vector<Label> labels;
	double step;
};

class ImGuiTraceViewer final : public ImGuiPart
{
public:
	explicit ImGuiTraceViewer(ImGuiManager& manager);

	[[nodiscard]] zstring_view iniName() const override { return "Probe/Trace Viewer"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = false;

private:
	using Traces = std::span<Tracer::Trace*>;

	[[nodiscard]] double getUnitConversionFactor() const;

	void zoomToFit(EmuTime minT, EmuDuration totalT);
	void doZoom(double factor, double xFract);
	void scrollTo(EmuTime time);
	void gotoPrevPosEdge(Traces traces, EmuTime& selectedTime);
	void gotoPrevNegEdge(Traces traces, EmuTime& selectedTime);
	void gotoPrevEdge   (Traces traces, EmuTime& selectedTime);
	void gotoNextEdge   (Traces traces, EmuTime& selectedTime);
	void gotoNextPosEdge(Traces traces, EmuTime& selectedTime);
	void gotoNextNegEdge(Traces traces, EmuTime& selectedTime);

	void drawMenuBar(EmuTime minT, EmuDuration totalT, Tracer& tracer, Traces traces);
	void drawToolBar(EmuTime minT, EmuDuration totalT, float graphWidth, Traces traces);
	void drawNames(Traces traces, float rulerHeight, float rowHeight, int mouseRow, Debugger& debugger);
	void drawSplitter(float width);
	void drawRuler(gl::vec2 size, const Convertor& convertor, EmuTime from, EmuTime to, EmuTime now);
	void drawGraphs(Traces traces, float rulerHeight, float rowHeight, int mouseRow, const Convertor& convertor,
	                EmuTime minT, EmuTime maxT, EmuTime now);

	void paintMain(MSXMotherBoard& motherBoard);
	void paintSelect(MSXMotherBoard& motherBoard);
	void paintHelp();

private:
	TimelineFormatter timelineFormatter;
	EmuTime viewStartTime = EmuTime::zero();
	EmuDuration viewDuration = EmuDuration::sec(1.0);
	EmuTime selectedTime1 = EmuTime::zero();
	EmuTime selectedTime2 = EmuTime::zero();
	float col0_width = 100.0f; // TODO
	float scrollY = 0.0f;
	int selectedRow = 1;

	enum Units : int {
		SECONDS,
		MILLI_SECONDS,
		MICRO_SECONDS,
		CYCLES,
		NUM_UNITS // must be last
	};
	int units = Units::SECONDS;

	struct SelectedTrace {
		std::string name;
		bool selected = false; // always true for probes
	};
	std::vector<SelectedTrace> selectedTraces;
	bool allUserTraces = true;

	enum Origin : int {
		ZERO,
		PRIMARY,
		SECONDARY,
		NOW,
		NUM_ORIGIN // must be last
	};
	int timelineOrigin = Origin::ZERO;
	bool timelineStart = false;
	bool timelineStop = false;

	bool showSelect = false;
	bool showMenuBar = true;

	enum class HelpSection : uint8_t{
		OVERVIEW,
		PROBES_AND_TRACES,
		GRAPHS,
		NAVIGATION,
		EXAMPLE,
	};
	std::optional<HelpSection> helpSection;
	bool showHelp = false;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement   {"show",             &ImGuiTraceViewer::show},
		PersistentElement   {"showSelect",       &ImGuiTraceViewer::showSelect},
		PersistentElement   {"showMenuBar",      &ImGuiTraceViewer::showMenuBar},
		PersistentElementMax{"units",            &ImGuiTraceViewer::units, Units::NUM_UNITS},
		PersistentElement   {"allUserTraces",    &ImGuiTraceViewer::allUserTraces},
		PersistentElementMax{"timelineOrigin",   &ImGuiTraceViewer::timelineOrigin, Origin::NUM_ORIGIN},
		PersistentElement   {"timelineStart",    &ImGuiTraceViewer::timelineStart},
		PersistentElement   {"timelineStop",     &ImGuiTraceViewer::timelineStop},
	};
};

} // namespace openmsx

#endif
