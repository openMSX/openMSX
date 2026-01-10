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

	[[nodiscard]] const std::vector<Tracer::Trace*>& getTraces(MSXMotherBoard& motherBoard);
	[[nodiscard]] EmuTime getMarker(int i) const { return i ? selectedTime2 : selectedTime1; }
	void setMarker(int i, EmuTime t) { (i ? selectedTime2 : selectedTime1) = t; }
	[[nodiscard]] static std::string_view formatTraceValue(const TraceValue& value, std::span<char, 64> tmpBuf,
	                                                       Tracer::Trace::Format format);

public:
	bool show = false;
	bool showSelect = false;

private:
	void calcTraces(Tracer& tracer);
	[[nodiscard]] double getUnitConversionFactor() const;

	void zoomToFit(EmuTime minT, EmuDuration totalT);
	void doZoom(double factor, double xFract);
	void scrollTo(EmuTime time);
	void gotoPrevPosEdge(EmuTime& selectedTime);
	void gotoPrevNegEdge(EmuTime& selectedTime);
	void gotoPrevEdge   (EmuTime& selectedTime);
	void gotoNextEdge   (EmuTime& selectedTime);
	void gotoNextPosEdge(EmuTime& selectedTime);
	void gotoNextNegEdge(EmuTime& selectedTime);

	void drawMenuBar(EmuTime minT, EmuDuration totalT, Tracer& tracer);
	void drawToolBar(EmuTime minT, EmuDuration totalT, float graphWidth);
	void drawNames(float rulerHeight, float rowHeight, int mouseRow);
	void drawSplitter(float width);
	void drawRuler(gl::vec2 size, const Convertor& convertor, EmuTime from, EmuTime to, EmuTime now);
	void drawGraphs(float rulerHeight, float rowHeight, int mouseRow, const Convertor& convertor,
	                EmuTime minT, EmuTime maxT, EmuTime now, MSXMotherBoard& motherBoard);

	void paintMain(MSXMotherBoard& motherBoard);
	void paintSelect(MSXMotherBoard& motherBoard);
	void paintHelp();

private:
	std::vector<Tracer::Trace*> traces; // recalculated each frame, but can be queried by ImGuiRasterViewer
	TimelineFormatter timelineFormatter;
	EmuTime viewStartTime = EmuTime::zero();
	EmuDuration viewDuration = EmuDuration::sec(1.0);
	EmuTime selectedTime1 = EmuTime::zero();
	EmuTime selectedTime2 = EmuTime::zero();
	float col0_width = 100.0f; // TODO
	float scrollY = 0.0f;
	int selectedRow = 1;

	std::string ctxInputPos = "0";
	float ctxMouseX = 0.0f;
	int ctxMouseRow = 0;

	enum Units : int {
		SECONDS,
		MILLI_SECONDS,
		MICRO_SECONDS,
		CYCLES,
		PAL,
		NTSC,
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
