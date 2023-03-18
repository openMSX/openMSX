#include "ImGuiManager.hh"
#include "FileContext.hh"
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
	ImGui::GetIO().Fonts->AddFontDefault();
	static const ImWchar icons_ranges[] = { ICON_MIN_IGFD, ICON_MAX_IGFD, 0 };
	ImFontConfig icons_config; icons_config.MergeMode = true; icons_config.PixelSnapH = true;
	ImGui::GetIO().Fonts->AddFontFromMemoryCompressedBase85TTF(FONT_ICON_BUFFER_NAME_IGFD, 15.0f, &icons_config, icons_ranges);
}

static void cleanupImGui()
{
	ImGui::DestroyContext();
}


ImGuiManager::ImGuiManager()
{
	auto& debugSection = sections.emplace_back("debugger");
	auto& debugItems = debugSection.items;
	debugItems.emplace_back("showDisassembly", false);
	debugItems.emplace_back("showRegisters", false);
	debugItems.emplace_back("showStack", false);
	debugItems.emplace_back("showFlags", false);
	debugItems.emplace_back("showXYFlags", false);
	debugItems.emplace_back("flagsLayout", 1);
	debugSection.loadDynamic = [](ImGuiPersistentSection& section,
	                              std::string_view name,
	                              std::optional<std::string_view> value)
	        -> ImGuiPersistentItem* {
		if (name.starts_with("showDebuggable.")) {
			auto& result = section.items.emplace_back(std::string(name), false);
			if (value) {
				result.currentValue = StringOp::stringToBool(*value);
			}
			return &result;
		}
		return nullptr;
	};

	auto& bitmapSection = sections.emplace_back("bitmap viewer");
	auto& bitmapItems = bitmapSection.items;
	bitmapItems.emplace_back("show", false);
	bitmapItems.emplace_back("override", 0);
	bitmapItems.emplace_back("scrnMode", 0); // TODO 5-12
	bitmapItems.emplace_back("page", 0);
	bitmapItems.emplace_back("lines", 1); // TODO 192,212,256
	bitmapItems.emplace_back("color0", 16); // TODO -1, 0-15
	bitmapItems.emplace_back("palette", 0);
	bitmapItems.emplace_back("zoom", 1); // TODO 1-8
	bitmapItems.emplace_back("showGrid", true);
	bitmapItems.emplace_back("gridColor", gl::vec4{0.0f, 0.0f, 0.0f, 0.5f});

	auto& pathSection = sections.emplace_back("open file dialog");
	pathSection.loadDynamic = [](ImGuiPersistentSection& section,
	                             std::string_view name,
	                             std::optional<std::string_view> value)
	        -> ImGuiPersistentItem* {
		auto& result = section.items.emplace_back(std::string(name), std::string("."));
		if (value) {
			result.currentValue = std::string(*value);
		}
		return &result;
	};

	auto& soundSection = sections.emplace_back("sound chip settings");
	auto& soundItems = soundSection.items;
	soundItems.emplace_back("show", false);
	soundSection.loadDynamic = [](ImGuiPersistentSection& section,
	                              std::string_view name,
	                              std::optional<std::string_view> value)
	        -> ImGuiPersistentItem* {
		if (name.starts_with("showChannels.")) {
			auto& result = section.items.emplace_back(std::string(name), false);
			if (value) {
				result.currentValue = StringOp::stringToBool(*value);
			}
			return &result;
		}
		return nullptr;
	};

	auto& reverseBarSection = sections.emplace_back("reverse bar");
	auto& reverseItems = reverseBarSection.items;
	reverseItems.emplace_back("show", true);
	reverseItems.emplace_back("hideTitle", false);
	reverseItems.emplace_back("fadeOut", false);
	reverseItems.emplace_back("allowMove", true);

	auto& iconsSection = sections.emplace_back("OSD icons");
	auto& iconsItems = iconsSection.items;
	iconsItems.emplace_back("show", true);
	iconsItems.emplace_back("hideTitle", false);
	iconsItems.emplace_back("allowMove", true);
	iconsItems.emplace_back("layout", 1);
	iconsItems.emplace_back("fadeDuration", 5.0f);
	iconsItems.emplace_back("fadeDelay", 5.0f);
	iconsItems.emplace_back("showConfig", false);
	iconsSection.loadDynamic = [](ImGuiPersistentSection& section,
	                              std::string_view name,
	                              std::optional<std::string_view> value)
	        -> ImGuiPersistentItem* {
		if (!name.starts_with("icon.")) return nullptr;
		if (name.ends_with(".enabled")) {
			auto& result = section.items.emplace_back(std::string(name), true);
			if (value) {
				result.currentValue = StringOp::stringToBool(*value);
			}
			return &result;
		}
		if (name.ends_with(".fade")) {
			auto& result = section.items.emplace_back(std::string(name), false);
			if (value) {
				result.currentValue = StringOp::stringToBool(*value);
			}
			return &result;
		}
		if (name.ends_with(".on-image")) {
			auto& result = section.items.emplace_back(std::string(name), std::string{});
			if (value) {
				result.currentValue = std::string(*value);
			}
			return &result;
		}
		if (name.ends_with(".off-image")) {
			auto& result = section.items.emplace_back(std::string(name), std::string{});
			if (value) {
				result.currentValue = std::string(*value);
			}
			return &result;
		}
		if (name.ends_with(".expr")) {
			auto& result = section.items.emplace_back(std::string(name), std::string{});
			if (value) {
				result.currentValue = std::string(*value);
			}
			return &result;
		}
		return nullptr;
	};

	initializeImGui();

	ImGuiSettingsHandler ini_handler;
	ini_handler.TypeName = "openmsx";
	ini_handler.TypeHash = ImHashStr("openmsx");
	ini_handler.UserData = this;
	//ini_handler.ClearAllFn = [](ImGuiContext*, ImGuiSettingsHandler* handler) { // optional
	//      // Clear all settings data
	//      static_cast<ImGuiLayer*>(handler->UserData)->iniClearAll();
	//};
	//ini_handler.ReadInitFn = [](ImGuiContext*, ImGuiSettingsHandler* handler) { // optional
	//      // Read: Called before reading (in registration order)
	//      static_cast<ImGuiLayer*>(handler->UserData)->iniReadInit();
	//};
	ini_handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler* handler, const char* name) -> void* { // required
	        // Read: Called when entering into a new ini entry e.g. "[Window][Name]"
	        return static_cast<ImGuiManager*>(handler->UserData)->iniReadOpen(name);
	};
	ini_handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler* handler, void* entry, const char* line) { // required
	        // Read: Called for every line of text within an ini entry
	        static_cast<ImGuiManager*>(handler->UserData)->iniReadLine(entry, line);
	};
	//ini_handler.ApplyAllFn = [](ImGuiContext*, ImGuiSettingsHandler* handler) { // optional
	//      // Read: Called after reading (in registration order)
	//      static_cast<ImGuiLayer*>(handler->UserData)->iniApplyAll();
	//};
	ini_handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf) { // required
	        // Write: Output every entries into 'out_buf'
	        static_cast<ImGuiManager*>(handler->UserData)->iniWriteAll(out_buf);
	};
	ImGui::AddSettingsHandler(&ini_handler);
}

ImGuiManager::~ImGuiManager()
{
	cleanupImGui();
}

void* ImGuiManager::iniReadOpen(const char* name)
{
	return find(name);
}

void ImGuiManager::iniReadLine(void* entry, const char* line)
{
	assert(entry);
	static_cast<ImGuiPersistentSection*>(entry)->iniReadLine(line);
}

void ImGuiManager::iniWriteAll(ImGuiTextBuffer* buf)
{
	if (preSaveCallback) preSaveCallback(*this);

	for (const auto& section : sections) {
		buf->appendf("[openmsx][%s]\n", section.sectionName.c_str());
		section.iniWrite(buf);
		buf->append("\n");
	}
}

ImGuiPersistentSection* ImGuiManager::find(std::string_view name)
{
	auto it = std::ranges::find(sections, name, &ImGuiPersistentSection::sectionName);
	return (it != sections.end()) ? &*it : nullptr;
}


void ImGuiPersistentSection::iniWrite(ImGuiTextBuffer* buf) const
{
	for (const auto& item : items) {
		auto value = std::visit(
			[](auto t) { return strCat(t); },
			item.currentValue);
		buf->appendf("%s=%s\n", item.name.c_str(), value.c_str());
	}
}

void ImGuiPersistentSection::iniReadLine(const char* line_)
{
	std::string_view line = line_;
	auto pos = line.find('=');
	if (pos == std::string_view::npos) return;

	std::string_view name = line.substr(0, pos);
	std::string_view value = line.substr(pos + 1);

	if (auto* item = find(name)) {
		std::visit(overloaded{
			[&](int& i) {
				if (auto p = StringOp::stringTo<int>(value)) {
					i = *p;
				}
			},
			[&](bool& b) {
				b = StringOp::stringToBool(value);
			},
			[&](std::string& s) {
				s = value;
			},
			[&](float& f) {
				f = strtof(value.data(), nullptr); // TODO error handling
			},
			[&](gl::vec4& v) {
				// TODO error handling
				sscanf(value.data(), "[ %f %f %f %f ]", &v[0], &v[1], &v[2], &v[3]);
			}
		}, item->currentValue);
	} else if (loadDynamic) {
		loadDynamic(*this, name, value);
	}
}

ImGuiPersistentItem* ImGuiPersistentSection::find(std::string_view name)
{
	auto it = std::ranges::find(items, name, &ImGuiPersistentItem::name);
	return (it != items.end()) ? &*it : nullptr;
}


} // namespace openmsx
