#ifndef IMGUI_MANAGER_HH
#define IMGUI_MANAGER_HH

#include "ImGuiBitmapViewer.hh"
#include "ImGuiBreakPoints.hh"
#include "ImGuiCharacter.hh"
#include "ImGuiCheatFinder.hh"
#include "ImGuiConnector.hh"
#include "ImGuiConsole.hh"
#include "ImGuiDebugger.hh"
#include "ImGuiDiskManipulator.hh"
#include "ImGuiHelp.hh"
#include "ImGuiKeyboard.hh"
#include "ImGuiMachine.hh"
#include "ImGuiMedia.hh"
#include "ImGuiMessages.hh"
#include "ImGuiOpenFile.hh"
#include "ImGuiPalette.hh"
#include "ImGuiOsdIcons.hh"
#include "ImGuiPart.hh"
#include "ImGuiReverseBar.hh"
#include "ImGuiSettings.hh"
#include "ImGuiSoundChip.hh"
#include "ImGuiSpriteViewer.hh"
#include "ImGuiSymbols.hh"
#include "ImGuiTools.hh"
#include "ImGuiTrainer.hh"
#include "ImGuiVdpRegs.hh"
#include "ImGuiWatchExpr.hh"

#include "EventListener.hh"
#include "RomTypes.hh"
#include "TclObject.hh"

#include "strCat.hh"

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

struct ImGuiTextBuffer;

namespace openmsx {

class Reactor;

class ImGuiManager : public ImGuiPart, public EventListener
{
public:
	ImGuiManager(const ImGuiManager&) = delete;
	ImGuiManager& operator=(const ImGuiManager&) = delete;

	explicit ImGuiManager(Reactor& reactor_);
	~ImGuiManager();

	[[nodiscard]] Reactor& getReactor() { return reactor; }
	[[nodiscard]] Interpreter& getInterpreter();
	[[nodiscard]] CliComm& getCliComm();
	std::optional<TclObject> execute(TclObject command);
	void executeDelayed(std::function<void()> action);
	void executeDelayed(TclObject command,
	                    std::function<void(const TclObject&)> ok,
	                    std::function<void(const std::string&)> error);
	void executeDelayed(TclObject command,
	                    std::function<void(const TclObject&)> ok = {});

	void printError(std::string_view message);
	template<typename... Ts> void printError(Ts&&... ts) {
		printError(static_cast<std::string_view>(tmpStrCat(std::forward<Ts>(ts)...)));
	}

	void paintImGui();

private:
	// ImGuiPart
	[[nodiscard]] zstring_view iniName() const override { return "manager"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;

	// EventListener
	int signalEvent(const Event& event) override;

	// ini handler callbacks
	void iniReadInit();
	void* iniReadOpen(std::string_view name);
	void loadLine(void* entry, const char* line);
	void iniApplyAll();
	void iniWriteAll(ImGuiTextBuffer& buf);

private:
	Reactor& reactor;

public:
	ImGuiMachine machine;
	ImGuiDebugger debugger;
	ImGuiBreakPoints breakPoints;
	ImGuiSymbols symbols;
	ImGuiWatchExpr watchExpr;
	ImGuiBitmapViewer bitmap;
	ImGuiCharacter character;
	ImGuiSpriteViewer sprite;
	ImGuiVdpRegs vdpRegs;
	ImGuiPalette palette;
	ImGuiReverseBar reverseBar;
	ImGuiHelp help;
	ImGuiOsdIcons osdIcons;
	ImGuiOpenFile openFile;
	ImGuiMedia media;
	ImGuiConnector connector;
	ImGuiTools tools;
	ImGuiTrainer trainer;
	ImGuiCheatFinder cheatFinder;
	ImGuiDiskManipulator diskManipulator;
	ImGuiSettings settings;
	ImGuiSoundChip soundChip;
	ImGuiKeyboard keyboard;
	ImGuiConsole console;
	ImGuiMessages messages;

	bool menuFade = true;

private:
	std::vector<std::function<void()>> delayedActionQueue;
	std::vector<ImGuiPart*> parts;
	float menuAlpha = 1.0f;

	std::string droppedFile;
	std::string insertedInfo;
	std::vector<std::string> selectList;
	std::string selectedMedia;
	const RomInfo* romInfo = nullptr;
	RomType selectedRomType = ROM_UNKNOWN;
	float insertedInfoTimeout = 0.0f;
	bool mainMenuBarUndocked = false;
	bool handleDropped = false;
	bool openInsertedInfo = false;
	bool guiActive = false;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"mainMenuBarUndocked", &ImGuiManager::mainMenuBarUndocked}
	};
};

} // namespace openmsx

#endif
