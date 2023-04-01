#include "ImGuiManager.hh"

#include "CommandException.hh"
#include "EventDistributor.hh"
#include "File.hh"
#include "FileContext.hh"
#include "Reactor.hh"
#include "stl.hh"
#include "strCat.hh"
#include "StringOp.hh"

#include <imgui.h>
#include <imgui_internal.h>
#include <CustomFont.ii> // icons for ImGuiFileDialog

#include <ranges>

namespace openmsx {

static void initializeImGui()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard |
	                  ImGuiConfigFlags_NavEnableGamepad |
	                  ImGuiConfigFlags_DockingEnable |
	                  ImGuiConfigFlags_ViewportsEnable;
	static auto iniFilename = systemFileContext().resolveCreate("imgui.ini");
	io.IniFilename = iniFilename.c_str();

	// load icon font file (CustomFont.cpp)
	io.Fonts->AddFontDefault();
	static const ImWchar icons_ranges[] = { ICON_MIN_IGFD, ICON_MAX_IGFD, 0 };
	ImFontConfig icons_config; icons_config.MergeMode = true; icons_config.PixelSnapH = true;
	io.Fonts->AddFontFromMemoryCompressedBase85TTF(FONT_ICON_BUFFER_NAME_IGFD, 15.0f, &icons_config, icons_ranges);
}

static ImFont* addFont(const std::string& filename, int fontSize)
{
	File file(filename);
	auto fileSize = file.getSize();
	auto ttfData = std::span(static_cast<uint8_t*>(ImGui::MemAlloc(fileSize)), fileSize);
	file.read(ttfData);

	auto& io = ImGui::GetIO();
	return io.Fonts->AddFontFromMemoryTTF(
		ttfData.data(), // transfer ownership of 'ttfData' buffer
		narrow<int>(ttfData.size()), narrow<float>(fontSize));
}

static void cleanupImGui()
{
	ImGui::DestroyContext();
}


ImGuiManager::ImGuiManager(Reactor& reactor_)
	: reactor(reactor_)
	, machine(*this)
	, reverseBar(*this)
	, help(*this)
	, osdIcons(*this)
	, openFile(*this)
	, media(*this)
	, connector(*this)
	, settings(*this)
	, soundChip(*this)
	, keyboard(*this)
{
	initializeImGui();

	// load extra fonts
	const auto& context = systemFileContext();
	vera13           = addFont(context.resolve("skins/Vera.ttf.gz"), 13);
	veraBold13       = addFont(context.resolve("skins/Vera-Bold.ttf.gz"), 13);
	veraBold16       = addFont(context.resolve("skins/Vera-Bold.ttf.gz"), 16);
	veraItalic13     = addFont(context.resolve("skins/Vera-Italic.ttf.gz"), 13);
	veraBoldItalic13 = addFont(context.resolve("skins/Vera-Bold-Italic.ttf.gz"), 13);

	ImGuiSettingsHandler ini_handler;
	ini_handler.TypeName = "openmsx";
	ini_handler.TypeHash = ImHashStr("openmsx");
	ini_handler.UserData = this;
	//ini_handler.ClearAllFn = [](ImGuiContext*, ImGuiSettingsHandler* handler) { // optional
	//      // Clear all settings data
	//      static_cast<ImGuiManager*>(handler->UserData)->iniClearAll();
	//};
	ini_handler.ReadInitFn = [](ImGuiContext*, ImGuiSettingsHandler* handler) { // optional
	      // Read: Called before reading (in registration order)
	      static_cast<ImGuiManager*>(handler->UserData)->iniReadInit();
	};
	ini_handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler* handler, const char* name) -> void* { // required
	        // Read: Called when entering into a new ini entry e.g. "[Window][Name]"
	        return static_cast<ImGuiManager*>(handler->UserData)->iniReadOpen(name);
	};
	ini_handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler* handler, void* entry, const char* line) { // required
	        // Read: Called for every line of text within an ini entry
	        static_cast<ImGuiManager*>(handler->UserData)->loadLine(entry, line);
	};
	ini_handler.ApplyAllFn = [](ImGuiContext*, ImGuiSettingsHandler* handler) { // optional
	      // Read: Called after reading (in registration order)
	      static_cast<ImGuiManager*>(handler->UserData)->iniApplyAll();
	};
	ini_handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf) { // required
	        // Write: Output every entries into 'out_buf'
	        static_cast<ImGuiManager*>(handler->UserData)->iniWriteAll(*out_buf);
	};
	ImGui::AddSettingsHandler(&ini_handler);

	auto& eventDistributor = reactor.getEventDistributor();
	eventDistributor.registerEventListener(EventType::IMGUI_DELAYED_COMMAND, *this);
	eventDistributor.registerEventListener(EventType::BREAK, *this);
}

ImGuiManager::~ImGuiManager()
{
	auto& eventDistributor = reactor.getEventDistributor();
	eventDistributor.unregisterEventListener(EventType::BREAK, *this);
	eventDistributor.unregisterEventListener(EventType::IMGUI_DELAYED_COMMAND, *this);

	cleanupImGui();
}

Interpreter& ImGuiManager::getInterpreter()
{
	return reactor.getInterpreter();
}

std::optional<TclObject> ImGuiManager::execute(TclObject command)
{
	try {
		return command.executeCommand(getInterpreter());
	} catch (CommandException&) {
		// ignore
		return {};
	}
}

void ImGuiManager::executeDelayed(TclObject command,
                                  std::function<void(const TclObject&)> ok,
                                  std::function<void(const std::string&)> error)
{
	commandQueue.push_back(DelayedCommand{std::move(command), std::move(ok), std::move(error)});
	reactor.getEventDistributor().distributeEvent(Event::create<ImGuiDelayedCommandEvent>());
}

int ImGuiManager::signalEvent(const Event& event)
{
	switch (getType(event)) {
	case EventType::IMGUI_DELAYED_COMMAND: {
		auto& interp = getInterpreter();
		for (auto& [cmd, ok, error] : commandQueue) {
			try {
				auto result = cmd.executeCommand(interp);
				if (ok) ok(result);
			} catch (CommandException& e) {
				if (error) error(e.getMessage());
			}
		}
		commandQueue.clear();
		break;
	}
	case EventType::BREAK:
		debugger.signalBreak();
		break;
	default:
		UNREACHABLE;
	}
	return 0;
}

void ImGuiManager::paint()
{
	auto* motherBoard = reactor.getMotherBoard();
	if (motherBoard) {
		debugger.paint(*motherBoard);
		reverseBar.paint(*motherBoard);
		soundChip.paint(*motherBoard);
		keyboard.paint(*motherBoard);
	}
	machine.paint(motherBoard);
	help.paint();
	osdIcons.paint();
	openFile.paint();

	if (ImGui::BeginMainMenuBar()) {
		machine.showMenu(motherBoard);
		media.showMenu(motherBoard);
		connector.showMenu(motherBoard);
		reverseBar.showMenu(motherBoard);
		settings.showMenu();
		debugger.showMenu(motherBoard);
		help.showMenu();
		ImGui::EndMainMenuBar();
	}
}

void ImGuiManager::iniReadInit()
{
	debugger.bitmap.loadStart();
	debugger.loadStart();
	reverseBar.loadStart();
	osdIcons.loadStart();
	openFile.loadStart();
	soundChip.loadStart();
	keyboard.loadStart();
}

void* ImGuiManager::iniReadOpen(std::string_view name)
{
	if (name == "bitmap viewer") {
		return &debugger.bitmap;
	} else if (name == "debugger") {
		return &debugger;
	} else if (name == "reverse bar") {
		return &reverseBar;
	} else if (name == "OSD icons") {
		return &osdIcons;
	} else if (name == "open file dialog") {
		return &openFile;
	} else if (name == "sound chip settings") {
		return &soundChip;
	} else if (name == "virtual keyboard") {
		return &keyboard;
	} else {
		return nullptr;
	}
}

void ImGuiManager::loadLine(void* entry, const char* line_)
{
	zstring_view line = line_;
	auto pos = line.find('=');
	if (pos == zstring_view::npos) return;
	std::string_view name = line.substr(0, pos);
	zstring_view value = line.substr(pos + 1);

	assert(entry);
	static_cast<ImGuiReadHandler*>(entry)->loadLine(name, value);
}

void ImGuiManager::iniApplyAll()
{
	debugger.bitmap.loadEnd();
	debugger.loadEnd();
	reverseBar.loadEnd();
	osdIcons.loadEnd();
	openFile.loadEnd();
	soundChip.loadEnd();
	keyboard.loadEnd();
}

void ImGuiManager::iniWriteAll(ImGuiTextBuffer& buf)
{
	debugger.bitmap.save(buf);
	debugger.save(buf);
	reverseBar.save(buf);
	osdIcons.save(buf);
	openFile.save(buf);
	soundChip.save(buf);
	keyboard.save(buf);
}

} // namespace openmsx
