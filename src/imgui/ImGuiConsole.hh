#ifndef IMGUI_CONSOLE_HH
#define IMGUI_CONSOLE_HH

#include "ImGuiPart.hh"
#include "ImGuiUtils.hh"

#include "BooleanSetting.hh"
#include "ConsoleLine.hh"
#include "InterpreterOutput.hh"

#include "Observer.hh"
#include "circular_buffer.hh"
#include "function_ref.hh"

#include <string>

struct ImGuiInputTextCallbackData;

namespace openmsx {

class ImGuiConsole final : public ImGuiPart
                         , private InterpreterOutput
                         , private Observer<Setting>
{
public:
	explicit ImGuiConsole(ImGuiManager& manager);
	~ImGuiConsole();

	[[nodiscard]] zstring_view iniName() const override { return "console"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = false;

private:
	void print(std::string_view text, imColor color = imColor::TEXT);
	void newLineConsole(ConsoleLine line);
	static int textEditCallbackStub(ImGuiInputTextCallbackData* data);
	int textEditCallback(ImGuiInputTextCallbackData* data);
	void tabEdit(ImGuiInputTextCallbackData* data, function_ref<std::string(std::string_view)> action);
	void colorize(std::string_view line);
	void putHistory(std::string command);
	void saveHistory();
	void loadHistory();

	void computeCompletionColumns(float availableWidth);

	// InterpreterOutput
	void output(std::string_view text) override;
	[[nodiscard]] unsigned getOutputColumns() const override;
	void setCompletions(std::span<const std::string_view> completions) override;

	// Observer
	void update(const Setting& setting) noexcept override;

private:
	BooleanSetting consoleSetting;

	circular_buffer<std::string> history;
	std::string historyBackupLine;
	int historyPos = -1; // -1: new line, 0..history.size()-1 browsing history.

	circular_buffer<ConsoleLine> lines; // output lines

	std::string commandBuffer; // collect multi-line commands

	std::string prompt;
	std::string inputBuf;
	ConsoleLine coloredInputBuf;

	std::vector<ImWchar> replayInput;
	std::vector<std::string> completions;
	std::vector<float> colWidths;
	std::string completionReplacement;
	gl::vec2 popupSize;
	int completionIndex = -1;
	bool completionPopupOpen = false;

	unsigned columns = 80; // gets recalculated
	bool scrollToBottom = false;
	bool wasShown = false;
	bool wrap = true;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show", &ImGuiConsole::show},
		PersistentElement{"wrap", &ImGuiConsole::wrap}
	};
};

} // namespace openmsx

#endif
