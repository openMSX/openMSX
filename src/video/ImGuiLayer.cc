#include "ImGuiLayer.hh"

#include "CartridgeSlotManager.hh"
#include "Connector.hh"
#include "Debugger.hh"
#include "Display.hh"
#include "EnumSetting.hh"
#include "FilenameSetting.hh"
#include "FileOperations.hh"
#include "FloatSetting.hh"
#include "GlobalCommandController.hh"
#include "GLImage.hh"
#include "GLUtil.hh"
#include "IntegerSetting.hh"
#include "KeyCodeSetting.hh"
#include "Mixer.hh"
#include "MSXMixer.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPUInterface.hh"
#include "Pluggable.hh"
#include "PluggingController.hh"
#include "ProxySetting.hh"
#include "Reactor.hh"
#include "ReadOnlySetting.hh"
#include "RealDrive.hh"
#include "SettingsManager.hh"
#include "SoundDevice.hh"
#include "StringSetting.hh"
#include "checked_cast.hh"
#include "narrow.hh"
#include "ranges.hh"

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_opengl3.h>
#include <imgui_memory_editor.h>
#include <ImGuiFileDialog.h>

#include <SDL.h>

namespace openmsx {

static void simpleToolTip(const char* desc)
{
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}
static void simpleToolTip(const std::string& desc) { simpleToolTip(desc.c_str()); }
static void simpleToolTip(std::string_view desc) { simpleToolTip(std::string(desc)); }

static void HelpMarker(const char* desc)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	simpleToolTip(desc);
}

static void settingStuff(Setting& setting)
{
	simpleToolTip(setting.getDescription());
	if (ImGui::BeginPopupContextItem()) {
		auto defaultValue = setting.getDefaultValue();
		auto defaultString = defaultValue.getString();
		ImGui::Text("Default value: %s", defaultString.c_str());
		if (defaultString.empty()) {
			ImGui::SameLine();
			ImGui::TextDisabled("<empty>");
		}
		if (ImGui::Button("Restore default")) {
			setting.setValue(defaultValue);
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

static bool Checkbox(const char* label, BooleanSetting& setting)
{
	bool value = setting.getBoolean();
	bool changed = ImGui::Checkbox(label, &value);
	if (changed) setting.setBoolean(value);
	settingStuff(setting);
	return changed;
}

// Should we put these helpers in the ImGui namespace?
static bool SliderInt(const char* label, IntegerSetting& setting, ImGuiSliderFlags flags = 0)
{
	int value = setting.getInt();
	int min = setting.getMinValue();
	int max = setting.getMaxValue();
	bool changed = ImGui::SliderInt(label, &value, min, max, "%d", flags);
	if (changed) setting.setInt(value);
	settingStuff(setting);
	return changed;
}
static bool SliderFloat(const char* label, FloatSetting& setting, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	float value = setting.getFloat();
	float min = narrow_cast<float>(setting.getMinValue());
	float max = narrow_cast<float>(setting.getMaxValue());
	bool changed = ImGui::SliderFloat(label, &value, min, max, format, flags);
	if (changed) setting.setDouble(value); // TODO setFloat()
	settingStuff(setting);
	return changed;
}
static bool InputText(const char* label, std::string& value)
{
	char buffer[256 + 1];
	assert(value.size() < 256); // TODO
	strncpy(buffer, value.data(), 256);
	bool changed = ImGui::InputText(label, buffer, 256);
	if (changed) value = buffer;
	return changed;
}
static bool InputText(const char* label, Setting& setting)
{
	auto value = std::string(setting.getValue().getString());
	bool changed = InputText(label, value);
	if (changed) setting.setValue(TclObject(value));
	settingStuff(setting);
	return changed;
}
static void ComboBox(const char* label, Setting& setting, EnumSettingBase& enumSetting)
{
	auto current = setting.getValue().getString();
	if (ImGui::BeginCombo(label, current.c_str())) {
		for (const auto& entry : enumSetting.getMap()) {
			bool selected = entry.name == current;
			if (ImGui::Selectable(entry.name.c_str(), selected)) {
				setting.setValue(TclObject(entry.name));
			}
		}
		ImGui::EndCombo();
	}
	settingStuff(setting);
}
static void ComboBox(const char* label, VideoSourceSetting& setting) // TODO share code with EnumSetting?
{
	auto current = setting.getValue().getString();
	if (ImGui::BeginCombo(label, current.c_str())) {
		for (const auto& value : setting.getPossibleValues()) {
			bool selected = value == current;
			if (ImGui::Selectable(std::string(value).c_str(), selected)) {
				setting.setValue(TclObject(value));
			}
		}
		ImGui::EndCombo();
	}
	settingStuff(setting);
}

struct DebuggableEditor : public MemoryEditor
{
	DebuggableEditor() {
		Open = false;
		ReadFn = [](const ImU8* userdata, size_t offset) -> ImU8 {
			auto* debuggable = reinterpret_cast<Debuggable*>(const_cast<ImU8*>(userdata));
			return debuggable->read(narrow<unsigned>(offset));
		};
		WriteFn = [](ImU8* userdata, size_t offset, ImU8 data) -> void {
			auto* debuggable = reinterpret_cast<Debuggable*>(userdata);
			debuggable->write(narrow<unsigned>(offset), data);
		};
	}

	void DrawWindow(const char* title, Debuggable& debuggable, size_t base_display_addr = 0x0000) {
		MemoryEditor::DrawWindow(title, &debuggable, debuggable.getSize(), base_display_addr);
	}
};

ImGuiLayer::ImGuiLayer(Reactor& reactor_)
	: Layer(Layer::COVER_PARTIAL, Layer::Z_IMGUI)
	, reactor(reactor_)
	, interp(reactor.getInterpreter())
{
}

ImGuiLayer::~ImGuiLayer()
{
}

std::optional<TclObject> ImGuiLayer::execute(TclObject command)
{
	if (!command.empty()) {
		try {
			return command.executeCommand(interp);
		} catch (CommandException&) {
			// ignore
		}
	}
	return {};
}

void ImGuiLayer::selectFileCommand(const std::string& title, const char* filters, TclObject command)
{
	ImGuiFileDialogFlags flags =
		ImGuiFileDialogFlags_DontShowHiddenFiles |
	//	ImGuiFileDialogFlags_CaseInsensitiveExtention |    // TODO
		ImGuiFileDialogFlags_Modal;
	//flags |= ImGuiFileDialogFlags_ConfirmOverwrite |
	//         ImGuiFileDialogFlags_DisableCreateDirectoryButton;
	ImGuiFileDialog::Instance()->OpenDialog(
		"FileDialog", title, filters,
		".", "", 1, nullptr, flags);
	openFileCallback = [this, command](const std::string& filename) mutable {
		command.addListElement(filename);
		execute(command);
	};
}

void ImGuiLayer::mediaMenu(MSXMotherBoard* motherBoard)
{
	if (!ImGui::BeginMenu("Media", motherBoard != nullptr)) {
		return;
	}
	assert(motherBoard);

	TclObject command;

	bool needSeparator = false;
	auto addSeparator = [&] {
		if (needSeparator) {
			needSeparator = false;
			ImGui::Separator();
		}
	};
	auto showCurrent = [&](TclObject current, const char* type) {
		if (current.empty()) {
			ImGui::Text("no %s inserted", type);
		} else {
			ImGui::Text("Current: %s", current.getString().c_str());
		}
		ImGui::Separator();
	};

	// diskX
	auto drivesInUse = RealDrive::getDrivesInUse(*motherBoard); // TODO use this or fully rely on commands?
	std::string driveName = "diskX";
	for (auto i : xrange(RealDrive::MAX_DRIVES)) {
		if (!(*drivesInUse)[i]) continue;
		driveName[4] = char('a' + i);
		if (auto cmdResult = execute(TclObject(driveName))) {
			if (ImGui::BeginMenu(driveName.c_str())) {
				auto currentImage = cmdResult->getListIndex(interp, 1);
				showCurrent(currentImage, "disk");
				if (ImGui::MenuItem("Eject", nullptr, false, !currentImage.empty())) {
					command.addListElement(driveName, "eject");
				}
				if (ImGui::MenuItem("Insert disk image")) {
					selectFileCommand("Select disk image for " + driveName,
					                  ".dsk,.gz,.*", // TODO
					                  makeTclList(driveName, "insert"));
				}
				ImGui::EndMenu();
			}
			needSeparator = true;
		}
	}
	addSeparator();

	// cartA / extX TODO
	auto& slotManager = motherBoard->getSlotManager();
	std::string cartName = "cartX";
	for (auto slot : xrange(CartridgeSlotManager::MAX_SLOTS)) {
		if (!slotManager.slotExists(slot)) continue;
		cartName[4] = char('a' + slot);
		if (auto cmdResult = execute(TclObject(cartName))) {
			if (ImGui::BeginMenu(cartName.c_str())) {
				auto currentImage = cmdResult->getListIndex(interp, 1);
				showCurrent(currentImage, "cart"); // TODO cart/ext
				if (ImGui::MenuItem("Eject", nullptr, false, !currentImage.empty())) {
					command.addListElement(cartName, "eject");
				}
				if (ImGui::BeginMenu("ROM cartridge")) {
					if (ImGui::MenuItem("select ROM file")) {
						selectFileCommand("Select ROM image for " + cartName,
						                  ".rom,.gz,.*", // TODO
						                  makeTclList(cartName, "insert"));
					}
					ImGui::MenuItem("select ROM type: TODO");
					ImGui::MenuItem("patch files: TODO");
					ImGui::EndMenu();
				}
				if (ImGui::MenuItem("insert extension")) {
					ImGui::TextUnformatted("TODO");
				}
				ImGui::EndMenu();
			}
			needSeparator = true;
		}
	}
	addSeparator();

	// cassetteplayer
	if (auto cmdResult = execute(TclObject("cassetteplayer"))) {
		if (ImGui::BeginMenu("cassetteplayer")) {
			auto currentImage = cmdResult->getListIndex(interp, 1);
			showCurrent(currentImage, "cassette");
			if (ImGui::MenuItem("eject", nullptr, false, !currentImage.empty())) {
				command.addListElement("cassetteplayer", "eject");
			}
			if (ImGui::MenuItem("insert cassette image")) {
				selectFileCommand("Select cassette image",
				                  ".wav,.cas,.gz,.*",
				                  makeTclList("cassetteplayer", "insert"));
			}
			ImGui::EndMenu();
		}
		needSeparator = true;
	}
	addSeparator();

	// laserdisc
	if (auto cmdResult = execute(TclObject("laserdiscplayer"))) {
		if (ImGui::BeginMenu("laserdisc")) {
			auto currentImage = cmdResult->getListIndex(interp, 1);
			showCurrent(currentImage, "laserdisc");
			if (ImGui::MenuItem("eject", nullptr, false, !currentImage.empty())) {
				command.addListElement("laserdiscplayer", "eject");
			}
			if (ImGui::MenuItem("insert laserdisc image")) {
				selectFileCommand("Select laserdisc image",
				                  ".ogv,.*",
				                  makeTclList("laserdiscplayer", "insert"));
			}
			ImGui::EndMenu();
		}
		needSeparator = true;
	}

	ImGui::EndMenu();

	execute(command);
}

void ImGuiLayer::connectorsMenu(MSXMotherBoard* motherBoard)
{
	if (!ImGui::BeginMenu("Connectors", motherBoard != nullptr)) {
		return;
	}
	assert(motherBoard);

	TclObject command;

	const auto& pluggingController = motherBoard->getPluggingController();
	const auto& pluggables = pluggingController.getPluggables();
	for (auto* connector : pluggingController.getConnectors()) {
		const auto& connectorName = connector->getName();
		auto connectorClass = connector->getClass();
		const auto& currentPluggable = connector->getPlugged();
		if (ImGui::BeginCombo(connectorName.c_str(), std::string(currentPluggable.getName()).c_str())) {
			if (!currentPluggable.getName().empty()) {
				if (ImGui::Selectable("[unplug]")) {
					command.addListElement("unplug", connectorName);
				}
			}
			for (auto& plug : pluggables) {
				if (plug->getClass() != connectorClass) continue;
				auto plugName = std::string(plug->getName());
				bool selected = plug.get() == &currentPluggable;
				int flags = !selected && plug->getConnector() ? ImGuiSelectableFlags_Disabled : 0; // plugged in another connector
				if (ImGui::Selectable(plugName.c_str(), selected, flags)) {
					command.addListElement("plug", connectorName, plugName);
				}
				simpleToolTip(plug->getDescription());
			}
			ImGui::EndCombo();
		}
	}

	ImGui::EndMenu();
	execute(command); // only execute the command after the full menu is drawn
}

void ImGuiLayer::saveStateMenu(MSXMotherBoard* motherBoard)
{
	if (!ImGui::BeginMenu("Save state", motherBoard != nullptr)) {
		return;
	}
	assert(motherBoard);

	TclObject command;

	if (ImGui::MenuItem("Quick load state", "ALT+F7")) { // TODO check binding dynamically
		command.addListElement("loadstate");
	}
	if (ImGui::MenuItem("Quick save state", "ALT+F8")) { // TODO
		command.addListElement("savestate");
	}
	ImGui::Separator();

	auto existingStates = execute(TclObject("list_savestates"));
	if (ImGui::BeginMenu("Load state ...", existingStates && !existingStates->empty())) {
		if (ImGui::BeginTable("table", 2, ImGuiTableFlags_BordersInnerV)) {
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("select savestate");
			if (ImGui::BeginListBox("##list", ImVec2(ImGui::GetFontSize() * 20.0f, 240.0f))) {
				for (const auto& name : *existingStates) {
					if (ImGui::Selectable(name.c_str())) {
						command = makeTclList("loadstate", name);
					}
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
						if (previewImage.name != name) {
							// record name, but (so far) without image
							// this prevents that on a missing image, we don't continue retrying
							previewImage.name = std::string(name);
							previewImage.texture = gl::Texture(gl::Null{});

							std::string filename = FileOperations::join(
								FileOperations::getUserOpenMSXDir(),
								"savestates", tmpStrCat(name, ".png"));
							if (FileOperations::exists(filename)) {
								try {
									gl::ivec2 dummy;
									previewImage.texture = loadTexture(filename, dummy);
								} catch (...) {
									// ignore
								}
							}
						}
					}
				}
				ImGui::EndListBox();
			}

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("preview");
			ImVec2 size(320, 240);
			if (previewImage.texture.get()) {
				ImGui::Image(reinterpret_cast<void*>(previewImage.texture.get()), size);
			} else {
				ImGui::Dummy(size);
			}
			ImGui::EndTable();
		}
		ImGui::EndMenu();
	}
	ImGui::TextUnformatted("Save state ... TODO");
	ImGui::Separator();

	ImGui::TextUnformatted("Load replay ... TODO");
	ImGui::TextUnformatted("Save replay ... TODO");

	ImGui::EndMenu();
	execute(command);
}

void ImGuiLayer::settingsMenu()
{
	if (ImGui::BeginMenu("Settings")) {
		if (ImGui::BeginMenu("Video")) {
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Sound")) {
			auto& mixer = reactor.getMixer();
			auto& muteSetting = mixer.getMuteSetting();
			ImGui::BeginDisabled(muteSetting.getBoolean());
			SliderInt("master volume", mixer.getMasterVolume());
			ImGui::EndDisabled();
			Checkbox("mute", muteSetting);
			ImGui::Separator();
			ImGui::MenuItem("Show sound chip settings", nullptr, &showSoundChipSettings);

			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Misc")) {
			ImGui::EndMenu();
		}
		ImGui::Separator();
		if (ImGui::BeginMenu("Advanced")) {
			ImGui::TextUnformatted("All settings");
			ImGui::Separator();
			auto& manager = reactor.getGlobalCommandController().getSettingsManager();
			std::vector<Setting*> settings;
			for (auto* setting : manager.getAllSettings()) {
				if (dynamic_cast<ProxySetting*>(setting)) continue;
				if (dynamic_cast<ReadOnlySetting*>(setting)) continue;
				settings.push_back(checked_cast<Setting*>(setting));
			}
			ranges::sort(settings, {}, &Setting::getBaseName);
			for (auto* setting : settings) {
				std::string name(setting->getBaseName());
				if (auto* bSetting = dynamic_cast<BooleanSetting*>(setting)) {
					Checkbox(name.c_str(), *bSetting);
				} else if (auto* iSetting = dynamic_cast<IntegerSetting*>(setting)) {
					SliderInt(name.c_str(), *iSetting);
				} else if (auto* fSetting = dynamic_cast<FloatSetting*>(setting)) {
					SliderFloat(name.c_str(), *fSetting);
				} else if (auto* sSetting = dynamic_cast<StringSetting*>(setting)) {
					InputText(name.c_str(), *sSetting);
				} else if (auto* fnSetting = dynamic_cast<FilenameSetting*>(setting)) {
					InputText(name.c_str(), *fnSetting); // TODO
				} else if (auto* kSetting = dynamic_cast<KeyCodeSetting*>(setting)) {
					InputText(name.c_str(), *kSetting); // TODO
				} else if (auto* eSetting = dynamic_cast<EnumSettingBase*>(setting)) {
					ComboBox(name.c_str(), *setting, *eSetting);
				} else if (auto* vSetting = dynamic_cast<VideoSourceSetting*>(setting)) {
					ComboBox(name.c_str(), *vSetting);
				} else {
					assert(false);
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}
}

static bool anySpecialChannelSettings(const MSXMixer::SoundDeviceInfo& info)
{
	for (const auto& channel : info.channelSettings) {
		if (channel.mute->getBoolean()) return true;
		if (!channel.record->getString().empty()) return true;
	}
	return false;
}

void ImGuiLayer::soundChipSettings(MSXMotherBoard* motherBoard)
{
	auto restoreDefaultPopup = [](const char* label, Setting& setting) {
		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::Button(label)) {
				setting.setValue(setting.getDefaultValue());
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	};

	ImGui::Begin("Sound chip settings", &showSoundChipSettings);
	if (!motherBoard) {
		ImGui::End();
		return;
	}

	auto& msxMixer = motherBoard->getMSXMixer();
	auto& infos = msxMixer.getDeviceInfos(); // TODO sort on name
	if (ImGui::BeginTable("table", narrow<int>(infos.size()), ImGuiTableFlags_ScrollX)) {
		for (auto& info : infos) {
			auto& device = *info.device;
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(device.getName().c_str());
			simpleToolTip(device.getDescription());
		}
		for (auto& info : infos) {
			auto& volumeSetting = *info.volumeSetting;
			int volume = volumeSetting.getInt();
			int min = volumeSetting.getMinValue();
			int max = volumeSetting.getMaxValue();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("volume");
			std::string id = "##volume-" + info.device->getName();
			if (ImGui::VSliderInt(id.c_str(), ImVec2(18, 120), &volume, min, max)) {
				volumeSetting.setInt(volume);
			}
			restoreDefaultPopup("Set default", volumeSetting);
		}
		for (auto& info : infos) {
			auto& balanceSetting = *info.balanceSetting;
			int balance = balanceSetting.getInt();
			int min = balanceSetting.getMinValue();
			int max = balanceSetting.getMaxValue();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("balance");
			std::string id = "##balance-" + info.device->getName();
			if (ImGui::SliderInt(id.c_str(), &balance, min, max)) {
				balanceSetting.setInt(balance);
			}
			restoreDefaultPopup("Set center", balanceSetting);
		}
		for (auto& info : infos) {
			ImGui::TableNextColumn();
			if (anySpecialChannelSettings(info)) {
				ImU32 color = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 0.0f, 0.75f));
				ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, color);
			}
			ImGui::TextUnformatted("channels");
			const auto& name = info.device->getName();
			std::string id = "##channels-" + name;
			ImGui::Checkbox(id.c_str(), &channels[name]);
		}
		ImGui::EndTable();
	}

	ImGui::End();
}

void ImGuiLayer::channelSettings(MSXMotherBoard* motherBoard, const std::string& name, bool* enabled)
{
	if (!motherBoard) return;
	auto& msxMixer = motherBoard->getMSXMixer();
	auto* info = msxMixer.findDeviceInfo(name);
	if (!info) return;

	std::string label = name + " channel setting";
	if (ImGui::Begin(label.c_str(), enabled)) {
		if (ImGui::BeginTable("table", 3)) {
			int counter = 0;
			for (auto& channel : info->channelSettings) {
				ImGui::PushID(counter);
				++counter;
				ImGui::TableNextColumn();
				ImGui::Text("channel %d", counter);

				ImGui::TableNextColumn();
				Checkbox("mute", *channel.mute);

				ImGui::TableNextColumn();
				InputText("record", *channel.record);
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}

void ImGuiLayer::debuggableMenu(MSXMotherBoard* motherBoard)
{
	if (!ImGui::BeginMenu("Debuggables", motherBoard != nullptr)) {
		return;
	}
	assert(motherBoard);

	auto& debugger = motherBoard->getDebugger();
	// TODO sort debuggable names, either via sort here, or by
	//    storing the debuggable always in sorted order in Debugger
	for (auto& name : view::keys(debugger.getDebuggables())) {
		auto [it, inserted] = debuggables.try_emplace(name);
		auto& editor = it->second;
		if (inserted) editor = std::make_unique<DebuggableEditor>();
		ImGui::MenuItem(name.c_str(), nullptr, &editor->Open);
	}

	ImGui::EndMenu();
}

void ImGuiLayer::paint(OutputSurface& /*surface*/)
{
	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	auto& rendererSettings = reactor.getDisplay().getRenderSettings();
	auto& commandController = reactor.getCommandController();
	auto* motherBoard = reactor.getMotherBoard();

	// Allow docking in main window
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(),
		ImGuiDockNodeFlags_NoDockingInCentralNode |
		ImGuiDockNodeFlags_PassthruCentralNode);

	if (showDemoWindow) {
		ImGui::ShowDemoWindow(&showDemoWindow);
	}

	if (motherBoard) {
		// Show the enabled 'debuggables'
		auto& debugger = motherBoard->getDebugger();
		for (const auto& [name, editor] : debuggables) {
			if (editor->Open) {
				if (auto* debuggable = debugger.findDebuggable(name)) {
					editor->DrawWindow(name.c_str(), *debuggable);
				}
			}
		}

		// Show sound chip settings
		if (showSoundChipSettings) {
			soundChipSettings(motherBoard);
		}

		// Show sound chip channel settings
		for (auto& [name, enabled] : channels) {
			if (enabled) {
				channelSettings(motherBoard, name, &enabled);
			}
		}
	}

	if (ImGui::Begin("main window", nullptr, ImGuiWindowFlags_MenuBar)) {
		if (ImGui::BeginMenuBar()) {
			mediaMenu(motherBoard);
			connectorsMenu(motherBoard);
			saveStateMenu(motherBoard);
			settingsMenu();
			debuggableMenu(motherBoard);
			ImGui::EndMenuBar();
		}

		ImGui::Checkbox("ImGui Demo Window", &showDemoWindow);
		HelpMarker("Show the ImGui demo window.\n"
			"This is purely to demonstrate the ImGui capabilities.\n"
			"There is no connection with any openMSX functionality.");

		if (ImGui::Button("Reset MSX")) {
			commandController.executeCommand("reset");
		}
		HelpMarker("Reset the emulated MSX machine.");

		SliderFloat("noise", rendererSettings.getNoiseSetting(), "%.1f");
	}
	ImGui::End();
	if (first) {
		// on startup, focus main openMSX window instead of the GUI window
		first = false;
		ImGui::SetWindowFocus(nullptr);
	}

	// (Modal) file dialog
	if (ImGuiFileDialog::Instance()->Display("FileDialog")) {
		if (ImGuiFileDialog::Instance()->IsOk() && openFileCallback) {
			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
			openFileCallback(filePathName);
			openFileCallback = {};
		}
		ImGuiFileDialog::Instance()->Close();
	}

	// Rendering
	ImGuiIO& io = ImGui::GetIO();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// Update and Render additional Platform Windows
	// (Platform functions may change the current OpenGL context, so we
	// save/restore it to make it easier to paste this code elsewhere.
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
		SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
	}
}

} // namespace openmsx
