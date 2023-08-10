#ifndef IMGUI_MANAGER_HH
#define IMGUI_MANAGER_HH

#include "ImGuiBitmapViewer.hh"
#include "ImGuiBreakPoints.hh"
#include "ImGuiCharacter.hh"
#include "ImGuiConnector.hh"
#include "ImGuiConsole.hh"
#include "ImGuiDebugger.hh"
#include "ImGuiHelp.hh"
#include "ImGuiKeyboard.hh"
#include "ImGuiMachine.hh"
#include "ImGuiMedia.hh"
#include "ImGuiOpenFile.hh"
#include "ImGuiPalette.hh"
#include "ImGuiOsdIcons.hh"
#include "ImGuiPart.hh"
#include "ImGuiReverseBar.hh"
#include "ImGuiSettings.hh"
#include "ImGuiSoundChip.hh"
#include "ImGuiSpriteViewer.hh"
#include "ImGuiVdpRegs.hh"
#include "ImGuiSymbols.hh"

#include "EventListener.hh"
#include "TclObject.hh"

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

	Reactor& getReactor() { return reactor; }
	Interpreter& getInterpreter();
	std::optional<TclObject> execute(TclObject command);
	void executeDelayed(TclObject command,
	                    std::function<void(const TclObject&)> ok = {},
	                    std::function<void(const std::string&)> error = {});

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
	// TODO dynamic font loading in ImGui is technically possible, though not trivial
	// So for now pre-load all the fonts we'll need.
	//   see https://github.com/ocornut/imgui/issues/2311
	ImFont* vera13 = nullptr;
	ImFont* veraBold13 = nullptr;
	ImFont* veraBold16 = nullptr;
	ImFont* veraItalic13 = nullptr;
	ImFont* veraBoldItalic13 = nullptr;

	ImGuiMachine machine;
	ImGuiDebugger debugger;
	ImGuiBreakPoints breakPoints;
	ImGuiSymbols symbols;
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
	ImGuiSettings settings;
	ImGuiSoundChip soundChip;
	ImGuiKeyboard keyboard;
	ImGuiConsole console;

	bool menuFade = true;

private:
	struct DelayedCommand {
		TclObject command;
		std::function<void(const TclObject&)> ok;
		std::function<void(const std::string&)> error;
	};
	std::vector<DelayedCommand> commandQueue;
	std::vector<ImGuiPart*> parts;
	float menuAlpha = 1.0f;

	std::string droppedFile;
	std::string newDropMessage, dropMessage;
	std::string selectText;
	std::vector<std::string> selectList;
	bool mainMenuBarUndocked = false;
	bool handleDropped = false;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"mainMenuBarUndocked", &ImGuiManager::mainMenuBarUndocked}
	};
};

} // namespace openmsx

#endif
