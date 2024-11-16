#ifndef IMGUI_MANAGER_HH
#define IMGUI_MANAGER_HH

#include "ImGuiPartInterface.hh"
#include "ImGuiUtils.hh"
#include "Shortcuts.hh"

#include "EmuTime.hh"
#include "EventListener.hh"
#include "FilenameSetting.hh"
#include "IntegerSetting.hh"
#include "Reactor.hh"
#include "RomTypes.hh"
#include "TclObject.hh"

#include "Observer.hh"
#include "strCat.hh"
#include "StringReplacer.hh"

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

struct ImFont;
struct ImGuiTextBuffer;

namespace openmsx {

class CliComm;
class ImGuiBreakPoints;
class ImGuiCheatFinder;
class ImGuiConnector;
class ImGuiConsole;
class ImGuiDebugger;
class ImGuiDiskManipulator;
class ImGuiHelp;
class ImGuiKeyboard;
class ImGuiMachine;
class ImGuiMedia;
class ImGuiMessages;
class ImGuiOpenFile;
class ImGuiOsdIcons;
class ImGuiPalette;
class ImGuiReverseBar;
class ImGuiSCCViewer;
class ImGuiSettings;
class ImGuiSoundChip;
class ImGuiSymbols;
class ImGuiTools;
class ImGuiTrainer;
class ImGuiVdpRegs;
class ImGuiWatchExpr;
class ImGuiWaveViewer;
class RomInfo;
class SettingsConfig;

class ImGuiManager final : public ImGuiPartInterface, private EventListener, private Observer<Setting>
{
public:
	explicit ImGuiManager(Reactor& reactor_);
	ImGuiManager(const ImGuiManager&) = delete;
	ImGuiManager(ImGuiManager&&) = delete;
	ImGuiManager& operator=(const ImGuiManager&) = delete;
	ImGuiManager& operator=(ImGuiManager&&) = delete;
	~ImGuiManager();

	void   registerPart(ImGuiPartInterface* part);
	void unregisterPart(ImGuiPartInterface* part);

	[[nodiscard]] Reactor& getReactor() { return reactor; }
	[[nodiscard]] Shortcuts& getShortcuts() { return reactor.getShortcuts(); }
	[[nodiscard]] Interpreter& getInterpreter();
	[[nodiscard]] CliComm& getCliComm();
	std::optional<TclObject> execute(TclObject command);
	void executeDelayed(std::function<void()> action);
	void executeDelayed(TclObject command,
	                    const std::function<void(const TclObject&)>& ok,
	                    const std::function<void(const std::string&)>& error);
	void executeDelayed(TclObject command,
	                    const std::function<void(const TclObject&)>& ok = {});

	void printError(std::string_view message);
	template<typename... Ts> void printError(Ts&&... ts) {
		printError(static_cast<std::string_view>(tmpStrCat(std::forward<Ts>(ts)...)));
	}

	void preNewFrame();
	void paintImGui();

	void storeWindowPosition(gl::ivec2 pos) { windowPos = pos; }
	[[nodiscard]] gl::ivec2 retrieveWindowPosition() const { return windowPos; }

private:
	void initializeImGui();
	[[nodiscard]] ImFont* addFont(zstring_view filename, int fontSize);
	void loadFont();
	void reloadFont();
	void drawStatusBar(MSXMotherBoard* motherBoard);

	// ImGuiPartInterface
	[[nodiscard]] zstring_view iniName() const override { return "manager"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void loadEnd() override;

	// EventListener
	bool signalEvent(const Event& event) override;

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

	// ini handler callbacks
	void iniReadInit();
	void* iniReadOpen(std::string_view name);
	void loadLine(void* entry, const char* line) const;
	void iniApplyAll();
	void iniWriteAll(ImGuiTextBuffer& buf);

	void updateParts();

private:
	Reactor& reactor;
	std::vector<ImGuiPartInterface*> parts;
	std::vector<ImGuiPartInterface*> toBeAddedParts;
	bool removeParts = false;

public:
	FilenameSetting fontPropFilename;
	FilenameSetting fontMonoFilename;
	IntegerSetting fontPropSize;
	IntegerSetting fontMonoSize;
	ImFont* fontProp = nullptr;
	ImFont* fontMono = nullptr;

	std::unique_ptr<ImGuiMachine> machine;
	std::unique_ptr<ImGuiDebugger> debugger;
	std::unique_ptr<ImGuiBreakPoints> breakPoints;
	std::unique_ptr<ImGuiSymbols> symbols;
	std::unique_ptr<ImGuiWatchExpr> watchExpr;
	std::unique_ptr<ImGuiVdpRegs> vdpRegs;
	std::unique_ptr<ImGuiPalette> palette;
	std::unique_ptr<ImGuiReverseBar> reverseBar;
	std::unique_ptr<ImGuiHelp> help;
	std::unique_ptr<ImGuiOsdIcons> osdIcons;
	std::unique_ptr<ImGuiOpenFile> openFile;
	std::unique_ptr<ImGuiMedia> media;
	std::unique_ptr<ImGuiConnector> connector;
	std::unique_ptr<ImGuiTools> tools;
	std::unique_ptr<ImGuiTrainer> trainer;
	std::unique_ptr<ImGuiSCCViewer> sccViewer;
	std::unique_ptr<ImGuiWaveViewer> waveViewer;
	std::unique_ptr<ImGuiCheatFinder> cheatFinder;
	std::unique_ptr<ImGuiDiskManipulator> diskManipulator;
	std::unique_ptr<ImGuiSettings> settings;
	std::unique_ptr<ImGuiSoundChip> soundChip;
	std::unique_ptr<ImGuiKeyboard> keyboard;
	std::unique_ptr<ImGuiConsole> console;
	std::unique_ptr<ImGuiMessages> messages;

	bool menuFade = true;
	bool needReloadFont = false;
	bool statusBarVisible = false;
	std::string loadIniFile;

private:
	std::vector<std::function<void()>> delayedActionQueue;
	float menuAlpha = 1.0f;

	std::string droppedFile;
	std::string insertedInfo;
	std::vector<std::string> selectList;
	std::string selectedMedia;
	const RomInfo* romInfo = nullptr;
	RomType selectedRomType = RomType::UNKNOWN;
	float insertedInfoTimeout = 0.0f;
	gl::ivec2 windowPos;
	bool mainMenuBarUndocked = false;
	bool handleDropped = false;
	bool openInsertedInfo = false;
	bool guiActive = false;

	EmuTime prevBoardTime = EmuTime::zero();
	float speedDrawTimeOut = 0.0f;
	float fpsDrawTimeOut = 0.0f;
	float fps = 0.0f;
	float speed = 0.0f;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"mainMenuBarUndocked", &ImGuiManager::mainMenuBarUndocked},
		PersistentElement{"mainMenuBarFade",     &ImGuiManager::menuFade},
		PersistentElement{"windowPos",           &ImGuiManager::windowPos},
		PersistentElement{"statusBarVisible",    &ImGuiManager::statusBarVisible}
	};
};

// Parse machine or extension config files:
//  store config-name, display-name and meta-data
template<typename InfoType>
std::vector<InfoType> parseAllConfigFiles(ImGuiManager& manager, std::string_view type, std::initializer_list<std::string_view> topics)
{
	static constexpr auto replacer = StringReplacer::create(
		"name",         "Name",
		"manufacturer", "Manufacturer",
		"code",         "Product code",
		"region",       "Region",
		"release_year", "Release year",
		"description",  "Description",
		"type",         "Type");

	std::vector<InfoType> result;

	const auto& configs = Reactor::getHwConfigs(type);
	result.reserve(configs.size());
	for (const auto& config : configs) {
		auto& info = result.emplace_back();
		info.configName = config;

		// get machine meta-data
		auto& configInfo = info.configInfo;
		if (auto r = manager.execute(makeTclList("openmsx_info", type, config))) {
			auto first = r->begin();
			auto last = r->end();
			while (first != last) {
				auto desc = *first++;
				if (first == last) break; // shouldn't happen
				auto value = *first++;
				if (!value.empty()) {
					configInfo.emplace_back(std::string(replacer(desc)),
								std::string(value));
				}
			}
		}

		// Based on the above meta-data, try to construct a more
		// readable name Unfortunately this new name is no
		// longer guaranteed to be unique, we'll address this
		// below.
		auto& display = info.displayName;
		for (const auto topic : topics) {
			if (const auto* value = getOptionalDictValue(configInfo, topic)) {
				if (!value->empty()) {
					if (!display.empty()) strAppend(display, ' ');
					strAppend(display, *value);
				}
			}
		}
		if (display.empty()) display = config;
	}

	ranges::sort(result, StringOp::caseless{}, &InfoType::displayName);

	// make 'displayName' unique again
	auto sameDisplayName = [](InfoType& x, InfoType& y) {
		StringOp::casecmp cmp;
		return cmp(x.displayName, y.displayName);
	};
	chunk_by(result, sameDisplayName, [](auto first, auto last) {
		if (std::distance(first, last) == 1) return; // no duplicate name
		for (auto it = first; it != last; ++it) {
			strAppend(it->displayName, " (", it->configName, ')');
		}
		ranges::sort(first, last, StringOp::caseless{}, &InfoType::displayName);
	});

	return result;
}

} // namespace openmsx

#endif
