#include "ImGuiManager.hh"

#include "ImGuiUtils.hh"

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
	, debugger(*this)
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

	// In order that they appear in the menubar
	append(parts, std::initializer_list<ImGuiPart*>{
		&machine, &media, &connector, &reverseBar, &settings, &debugger, &help,
		&soundChip, &keyboard, &bitmap, &osdIcons, &openFile});
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
	for (auto* part : parts) {
		part->paint(motherBoard);
	}
	menuAlpha = [&] {
		if (!menuFade) return 1.0f;
		bool active = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
		              ImGui::IsWindowFocused(ImGuiHoveredFlags_AnyWindow);
		auto target = active ? 1.0f : 0.0001f;
		auto period = active ? 0.5f : 5.0f;
		return calculateFade(menuAlpha, target, period);
	}();
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, menuAlpha);
	if (ImGui::BeginMainMenuBar()) {
		for (auto* part : parts) {
			part->showMenu(motherBoard);
		}
		ImGui::EndMainMenuBar();
	}
	ImGui::PopStyleVar();
}

void ImGuiManager::iniReadInit()
{
	for (auto* part : parts) {
		part->loadStart();
	}
}

void* ImGuiManager::iniReadOpen(std::string_view name)
{
	for (auto* part : parts) {
		if (part->iniName() == name) return part;
	}
	return nullptr;
}

void ImGuiManager::loadLine(void* entry, const char* line_)
{
	zstring_view line = line_;
	auto pos = line.find('=');
	if (pos == zstring_view::npos) return;
	std::string_view name = line.substr(0, pos);
	zstring_view value = line.substr(pos + 1);

	assert(entry);
	static_cast<ImGuiPart*>(entry)->loadLine(name, value);
}

void ImGuiManager::iniApplyAll()
{
	for (auto* part : parts) {
		part->loadEnd();
	}
}

void ImGuiManager::iniWriteAll(ImGuiTextBuffer& buf)
{
	for (auto* part : parts) {
		if (auto name = part->iniName(); !name.empty()) {
			buf.appendf("[openmsx][%s]\n", name.c_str());
			part->save(buf);
			buf.append("\n");
		}
	}
}

} // namespace openmsx
