#ifndef IMGUI_CONSOLE_HH
#define IMGUI_CONSOLE_HH

#include "ImGuiPart.hh"

#include "BooleanSetting.hh"
#include "ConsoleLine.hh"
#include "InterpreterOutput.hh"

#include "Observer.hh"
#include "circular_buffer.hh"

#include <string>

struct ImGuiInputTextCallbackData;

namespace openmsx {

class ImGuiManager;

class ImGuiConsole final : public ImGuiPart
                         , private InterpreterOutput
                         , private Observer<Setting>
{
public:
	ImGuiConsole(ImGuiManager& manager);
	~ImGuiConsole();

	[[nodiscard]] zstring_view iniName() const override { return "console"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = true;

private:
	void print(std::string_view text, uint32_t rgba = 0xffffffff);
	void newLineConsole(ConsoleLine line);
	static int textEditCallbackStub(ImGuiInputTextCallbackData* data);
	int textEditCallback(ImGuiInputTextCallbackData* data);
	void colorize(std::string_view line);
	void putHistory(std::string command);
	void saveHistory();
	void loadHistory();

	// InterpreterOutput
	void output(std::string_view text) override;
	[[nodiscard]] unsigned getOutputColumns() const override;

	// Observer
	void update(const Setting& setting) noexcept override;

private:
	ImGuiManager& manager;
	BooleanSetting consoleSetting;

	circular_buffer<std::string> history;
	std::string historyBackupLine;
	int historyPos = -1; // -1: new line, 0..history.size()-1 browsing history.

	circular_buffer<ConsoleLine> lines; // output lines

	std::string commandBuffer; // collect multi-line commands

	std::string prompt;
	std::string inputBuf;
	ConsoleLine coloredInputBuf;

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
