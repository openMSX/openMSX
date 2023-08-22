#include "ImGuiManager.hh"

#include "ImGuiCpp.hh"
#include "ImGuiUtils.hh"

#include "CartridgeSlotManager.hh"
#include "CommandException.hh"
#include "EventDistributor.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "Reactor.hh"
#include "RealDrive.hh"

#include "stl.hh"
#include "strCat.hh"
#include "StringOp.hh"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
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
	: ImGuiPart(*this)
	, reactor(reactor_)
	, machine(*this)
	, debugger(*this)
	, breakPoints(*this)
	, symbols(*this)
	, watchExpr(*this)
	, bitmap(*this)
	, character(*this)
	, sprite(*this)
	, vdpRegs(*this)
	, reverseBar(*this)
	, help(*this)
	, osdIcons(*this)
	, openFile(*this)
	, media(*this)
	, connector(*this)
	, settings(*this)
	, soundChip(*this)
	, keyboard(*this)
	, console(*this)
	, messages(*this)
{
	initializeImGui();
	debugger.loadIcons();

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
	eventDistributor.registerEventListener(EventType::MOUSE_BUTTON_UP,   *this, EventDistributor::IMGUI);
	eventDistributor.registerEventListener(EventType::MOUSE_BUTTON_DOWN, *this, EventDistributor::IMGUI);
	eventDistributor.registerEventListener(EventType::MOUSE_MOTION,      *this, EventDistributor::IMGUI);
	eventDistributor.registerEventListener(EventType::MOUSE_WHEEL,       *this, EventDistributor::IMGUI);
	eventDistributor.registerEventListener(EventType::KEY_UP,            *this, EventDistributor::IMGUI);
	eventDistributor.registerEventListener(EventType::KEY_DOWN,          *this, EventDistributor::IMGUI);
	eventDistributor.registerEventListener(EventType::TEXT,              *this, EventDistributor::IMGUI);
	eventDistributor.registerEventListener(EventType::WINDOW,            *this, EventDistributor::IMGUI);
	eventDistributor.registerEventListener(EventType::FILE_DROP, *this);
	eventDistributor.registerEventListener(EventType::IMGUI_DELAYED_COMMAND, *this);
	eventDistributor.registerEventListener(EventType::BREAK, *this);

	// In order that they appear in the menubar
	append(parts, std::initializer_list<ImGuiPart*>{
		this,
		&machine, &media, &connector, &reverseBar, &settings, &debugger, &help,
		&soundChip, &keyboard, &symbols, &breakPoints, &watchExpr,
		&bitmap, &character, &sprite, &vdpRegs, &palette,
		&osdIcons, &openFile, &console, &messages});
}

ImGuiManager::~ImGuiManager()
{
	auto& eventDistributor = reactor.getEventDistributor();
	eventDistributor.unregisterEventListener(EventType::BREAK, *this);
	eventDistributor.unregisterEventListener(EventType::IMGUI_DELAYED_COMMAND, *this);
	eventDistributor.unregisterEventListener(EventType::FILE_DROP, *this);
	eventDistributor.unregisterEventListener(EventType::WINDOW, *this);
	eventDistributor.unregisterEventListener(EventType::TEXT, *this);
	eventDistributor.unregisterEventListener(EventType::KEY_DOWN, *this);
	eventDistributor.unregisterEventListener(EventType::KEY_UP, *this);
	eventDistributor.unregisterEventListener(EventType::MOUSE_WHEEL, *this);
	eventDistributor.unregisterEventListener(EventType::MOUSE_MOTION, *this);
	eventDistributor.unregisterEventListener(EventType::MOUSE_BUTTON_DOWN, *this);
	eventDistributor.unregisterEventListener(EventType::MOUSE_BUTTON_UP, *this);

	cleanupImGui();
}

void ImGuiManager::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiManager::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
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
	reactor.getEventDistributor().distributeEvent(ImGuiDelayedCommandEvent());
}

void ImGuiManager::executeDelayed(TclObject command,
                                  std::function<void(const TclObject&)> ok)
{
	executeDelayed(std::move(command), ok,
		[this](const std::string& message) { this->printError(message); });
}

void ImGuiManager::printError(std::string_view message)
{
	auto& cliComm = getReactor().getCliComm();
	cliComm.printError(message);
}

int ImGuiManager::signalEvent(const Event& event)
{
	if (auto* evt = get_event_if<SdlEvent>(event)) {
		const SDL_Event& sdlEvent = evt->getSdlEvent();
		ImGui_ImplSDL2_ProcessEvent(&sdlEvent);
		ImGuiIO& io = ImGui::GetIO();
		if ((io.WantCaptureMouse &&
		     sdlEvent.type == one_of(SDL_MOUSEMOTION, SDL_MOUSEWHEEL,
		                             SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP)) ||
		    (io.WantCaptureKeyboard &&
		     sdlEvent.type == one_of(SDL_KEYDOWN, SDL_KEYUP, SDL_TEXTINPUT))) {
			return EventDistributor::MSX; // block event for the MSX
		}
	} else {
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
		case EventType::FILE_DROP: {
			const auto& fde = get_event<FileDropEvent>(event);
			droppedFile = fde.getFileName();
			handleDropped = true;
			break;
		}
		case EventType::BREAK:
			debugger.signalBreak();
			break;
		default:
			UNREACHABLE;
		}
	}
	return 0;
}

// TODO share code with ImGuiMedia
static std::vector<std::string> getDrives(MSXMotherBoard* motherBoard)
{
	std::vector<std::string> result;
	if (!motherBoard) return result;

	std::string driveName = "diskX";
	auto drivesInUse = RealDrive::getDrivesInUse(*motherBoard);
	for (auto i : xrange(RealDrive::MAX_DRIVES)) {
		if (!(*drivesInUse)[i]) continue;
		driveName[4] = char('a' + i);
		result.push_back(driveName);
	}
	return result;
}

static std::vector<std::string> getSlots(MSXMotherBoard* motherBoard)
{
	std::vector<std::string> result;
	if (!motherBoard) return result;

	auto& slotManager = motherBoard->getSlotManager();
	std::string cartName = "cartX";
	for (auto slot : xrange(CartridgeSlotManager::MAX_SLOTS)) {
		if (!slotManager.slotExists(slot)) continue;
		cartName[4] = char('a' + slot);
		result.push_back(cartName);
	}
	return result;
}

void ImGuiManager::paintImGui()
{
	auto* motherBoard = reactor.getMotherBoard();
	for (auto* part : parts) {
		part->paint(motherBoard);
	}

	auto drawMenu = [&]{
		for (auto* part : parts) {
			part->showMenu(motherBoard);
		}
	};
	if (mainMenuBarUndocked) {
		im::Window("OpenMSX menu", &mainMenuBarUndocked, ImGuiWindowFlags_MenuBar, [&]{
			im::MenuBar([&]{
				drawMenu();
			});
		});
	} else {
		menuAlpha = [&] {
			if (!menuFade) return 1.0f;
			bool active = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
				ImGui::IsWindowFocused(ImGuiHoveredFlags_AnyWindow);
			auto target = active ? 1.0f : 0.0001f;
			auto period = active ? 0.5f : 5.0f;
			return calculateFade(menuAlpha, target, period);
		}();
		im::StyleVar(ImGuiStyleVar_Alpha, menuAlpha, [&]{
			im::MainMenuBar([&]{
				if (ImGui::ArrowButton("undock-button", ImGuiDir_Up)) {
					mainMenuBarUndocked = true;
				}
				simpleToolTip("Undock the menu bar from the main openMSX window.");
				drawMenu();
			});
		});
	}

	// drag and drop  (move this to ImGuiMedia ?)
	if (handleDropped) {
		handleDropped = false;
		auto category = execute(makeTclList("openmsx_info", "file_type_category", droppedFile))->getString();
		if (category == "unknown" && FileOperations::isDirectory(droppedFile)) {
			category = "disk";
		}

		TclObject command;
		auto error = [&](auto&& ...message) {
			command = makeTclList("error", strCat(message...));
		};
		auto cantHandle = [&](auto&& ...message) {
			error("Can't handle dropped file ", droppedFile, ": ", message...);
		};
		auto notPresent = [&](const auto& mediaType) {
			cantHandle("no ", mediaType, " present.");
		};

		auto selectMedia = [&](std::string_view type, std::vector<std::string> list) {
			if (list.empty()) {
				notPresent(type);
			} else if (list.size() == 1) {
				command = makeTclList(list.front(), "insert", droppedFile);
			} else {
				selectText = strCat("Select ", type, " for ", droppedFile);
				selectList = std::move(list);
				ImGui::OpenPopup("select-media");
			}
		};
		auto testMedia = [&](std::string_view type, std::string_view cmd) {
			if (auto cmdResult = execute(TclObject(cmd))) {
				command = makeTclList(cmd, "insert", droppedFile);
			} else {
				notPresent(type);
			}
		};

		if (category == "disk") {
			selectMedia("disk drive", getDrives(motherBoard));
		} else if (category == "rom") {
			selectMedia("cartridge slot", getSlots(motherBoard));
		} else if (category == "cassette") {
			testMedia("casette port", "cassetteplayer");
		} else if (category == "laserdisc") {
			testMedia("laser disc player", "laserdiscplayer");
		} else if (category == "savestate") {
			command = makeTclList("loadstate", droppedFile);
		} else if (category == "replay") {
			command = makeTclList("reverse", "loadreplay", droppedFile);
		} else if (category == "script") {
			command = makeTclList("source", droppedFile);
		} else if (FileOperations::getExtension(droppedFile) == ".txt") {
			command = makeTclList("type_from_file", droppedFile);
		} else {
			cantHandle("unknown file type");
		}
		executeDelayed(command);
	}
	im::Popup("select-media", [&]{
		ImGui::TextUnformatted(selectText);
		im::ListBox("##select-media", [&]{
			for (const auto& item : selectList) {
				if (ImGui::Selectable(item.c_str())) {
					executeDelayed(makeTclList(item, "insert", droppedFile));
					ImGui::CloseCurrentPopup();
				}
			}
		});
	});
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
