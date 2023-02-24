#include "ImGuiLayer.hh"

#include "CartridgeSlotManager.hh"
#include "CassettePlayerCLI.hh"
#include "Connector.hh"
#include "Debugger.hh"
#include "DiskImageCLI.hh"
#include "Display.hh"
#include "EnumSetting.hh"
#include "FileContext.hh"
#include "FilenameSetting.hh"
#include "FileOperations.hh"
#include "FloatSetting.hh"
#include "GlobalCommandController.hh"
#include "GLImage.hh"
#include "IntegerSetting.hh"
#include "KeyCodeSetting.hh"
#include "Mixer.hh"
#include "MSXMixer.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPUInterface.hh"
#include "MSXRomCLI.hh"
#include "Pluggable.hh"
#include "PluggingController.hh"
#include "ProxySetting.hh"
#include "Reactor.hh"
#include "ReadOnlySetting.hh"
#include "RealDrive.hh"
#include "ReverseManager.hh"
#include "SettingsManager.hh"
#include "SoundDevice.hh"
#include "StringSetting.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "checked_cast.hh"
#include "join.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "strCat.hh"
#include "view.hh"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <imgui_stdlib.h>
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
static bool InputText(const char* label, Setting& setting)
{
	auto value = std::string(setting.getValue().getString());
	bool changed = ImGui::InputText(label, &value);
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
	// TODO read from some config file
	setDefaultIcons();
}

void ImGuiLayer::setDefaultIcons()
{
	iconInfo.clear();
	iconInfo.emplace_back(TclObject("$led_power"), "skins/set1/power-on.png", "skins/set1/power-off.png", true);
	iconInfo.emplace_back(TclObject("$led_caps" ), "skins/set1/caps-on.png",  "skins/set1/caps-off.png", true);
	iconInfo.emplace_back(TclObject("$led_kana" ), "skins/set1/kana-on.png",  "skins/set1/kana-off.png", true);
	iconInfo.emplace_back(TclObject("$led_pause"), "skins/set1/pause-on.png", "skins/set1/pause-off.png", true);
	iconInfo.emplace_back(TclObject("$led_turbo"), "skins/set1/turbo-on.png", "skins/set1/turbo-off.png", true);
	iconInfo.emplace_back(TclObject("$led_FDD"  ), "skins/set1/fdd-on.png",   "skins/set1/fdd-off.png", true);
	iconInfo.emplace_back(TclObject("$pause"    ), "skins/set1/pause.png",    "", false);
	iconInfo.emplace_back(TclObject("!$throttle || $fastforward"), "skins/set1/throttle.png", "", false);
	iconInfo.emplace_back(TclObject("$mute"     ), "skins/set1/mute.png",     "", false);
	iconInfo.emplace_back(TclObject("$breaked"  ), "skins/set1/breaked.png",  "", false);
	iconInfoDirty = true;
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

void ImGuiLayer::selectFileCommand(const std::string& title, std::string filters, TclObject command)
{
	selectFile(title, filters, [this, command](const std::string& filename) mutable {
		command.addListElement(filename);
		execute(command);
	});
}
void ImGuiLayer::selectFile(const std::string& title, std::string filters,
                            std::function<void(const std::string&)> callback)
{
	filters += ",All files (*){.*}";
	ImGuiFileDialogFlags flags =
		ImGuiFileDialogFlags_DontShowHiddenFiles |
		ImGuiFileDialogFlags_CaseInsensitiveExtention |
		ImGuiFileDialogFlags_Modal |
		ImGuiFileDialogFlags_DisableCreateDirectoryButton;
	//flags |= ImGuiFileDialogFlags_ConfirmOverwrite |
	lastFileDialog = title;
	auto [it, inserted] = lastPath.try_emplace(lastFileDialog, ".");
	ImGuiFileDialog::Instance()->OpenDialog(
		"FileDialog", title, filters.c_str(),
		it->second, "", 1, nullptr, flags);
	openFileCallback = callback;
}

static std::string buildFilter(std::string_view description, std::span<const std::string_view> extensions)
{
	return strCat(
		description, " (",
		join(view::transform(extensions,
		                     [](const auto& ext) { return strCat("*.", ext); }),
		     ' '),
		"){",
		join(view::transform(extensions,
		                     [](const auto& ext) { return strCat('.', ext); }),
		     ','),
		",.gz,.zip}");
}

void ImGuiLayer::mediaMenu(MSXMotherBoard* motherBoard)
{
	if (!ImGui::BeginMenu("Media", motherBoard != nullptr)) {
		return;
	}
	assert(motherBoard);

	TclObject command;

	enum { NONE, ITEM, SEPARATOR } status = NONE;
	auto endGroup = [&] {
		if (status == ITEM) status = SEPARATOR;
	};
	auto elementInGroup = [&] {
		if (status == SEPARATOR) {
			ImGui::Separator();
		}
		status = ITEM;
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
			elementInGroup();
			if (ImGui::BeginMenu(driveName.c_str())) {
				auto currentImage = cmdResult->getListIndex(interp, 1);
				showCurrent(currentImage, "disk");
				if (ImGui::MenuItem("Eject", nullptr, false, !currentImage.empty())) {
					command.addListElement(driveName, "eject");
				}
				if (ImGui::MenuItem("Insert disk image")) {
					selectFileCommand("Select disk image for " + driveName,
					                  buildFilter("disk images", DiskImageCLI::getExtensions()),
					                  makeTclList(driveName, "insert"));
				}
				ImGui::EndMenu();
			}
		}
	}
	endGroup();

	// cartA / extX TODO
	auto& slotManager = motherBoard->getSlotManager();
	std::string cartName = "cartX";
	for (auto slot : xrange(CartridgeSlotManager::MAX_SLOTS)) {
		if (!slotManager.slotExists(slot)) continue;
		cartName[4] = char('a' + slot);
		if (auto cmdResult = execute(TclObject(cartName))) {
			elementInGroup();
			if (ImGui::BeginMenu(cartName.c_str())) {
				auto currentImage = cmdResult->getListIndex(interp, 1);
				showCurrent(currentImage, "cart"); // TODO cart/ext
				if (ImGui::MenuItem("Eject", nullptr, false, !currentImage.empty())) {
					command.addListElement(cartName, "eject");
				}
				if (ImGui::BeginMenu("ROM cartridge")) {
					if (ImGui::MenuItem("select ROM file")) {
						selectFileCommand("Select ROM image for " + cartName,
						                  buildFilter("Rom images", MSXRomCLI::getExtensions()),
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
		}
	}
	endGroup();

	// cassetteplayer
	if (auto cmdResult = execute(TclObject("cassetteplayer"))) {
		elementInGroup();
		if (ImGui::BeginMenu("cassetteplayer")) {
			auto currentImage = cmdResult->getListIndex(interp, 1);
			showCurrent(currentImage, "cassette");
			if (ImGui::MenuItem("eject", nullptr, false, !currentImage.empty())) {
				command.addListElement("cassetteplayer", "eject");
			}
			if (ImGui::MenuItem("insert cassette image")) {
				selectFileCommand("Select cassette image",
				                  buildFilter("Cassette images", CassettePlayerCLI::getExtensions()),
				                  makeTclList("cassetteplayer", "insert"));
			}
			ImGui::EndMenu();
		}
	}
	endGroup();

	// laserdisc
	if (auto cmdResult = execute(TclObject("laserdiscplayer"))) {
		elementInGroup();
		if (ImGui::BeginMenu("laserdisc")) {
			auto currentImage = cmdResult->getListIndex(interp, 1);
			showCurrent(currentImage, "laserdisc");
			if (ImGui::MenuItem("eject", nullptr, false, !currentImage.empty())) {
				command.addListElement("laserdiscplayer", "eject");
			}
			if (ImGui::MenuItem("insert laserdisc image")) {
				selectFileCommand("Select laserdisc image",
				                  buildFilter("Laserdisk images", std::array<std::string_view, 1>{"ogv"}),
				                  makeTclList("laserdiscplayer", "insert"));
			}
			ImGui::EndMenu();
		}
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
	ImGui::MenuItem("Show reverse bar", nullptr, &showReverseBar);

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

static std::string formatTime(double time)
{
	assert(time >= 0.0);
	int hours = int(time / 3600.0);
	time -= double(hours * 3600);
	int minutes = int(time / 60.0);
	time -= double(minutes * 60);
	int seconds = int(time);
	time -= double(seconds);
	int hundreds = int(100.0 * time);

	std::string result = "00:00:00.00";
	auto insert = [&](size_t pos, unsigned value) {
		assert(value < 100);
		result[pos + 0] = char('0' + (value / 10));
		result[pos + 1] = char('0' + (value % 10));
	};
	insert(0, hours % 100);
	insert(3, minutes);
	insert(6, seconds);
	insert(9, hundreds);
	return result;
}

void ImGuiLayer::drawReverseBar(MSXMotherBoard* motherBoard)
{
	const auto& style = ImGui::GetStyle();
	auto textHeight = ImGui::GetTextLineHeight();
	auto windowHeight = style.WindowPadding.y + 2.0f * textHeight + style.WindowPadding.y;
	if (!reverseHideTitle) {
		windowHeight += style.FramePadding.y + textHeight + style.FramePadding.y;
	}
	ImGui::SetNextWindowSizeConstraints(ImVec2(250, windowHeight), ImVec2(FLT_MAX, windowHeight));

	int flags = reverseHideTitle ? ImGuiWindowFlags_NoTitleBar |
	                               ImGuiWindowFlags_NoResize |
	                               //ImGuiWindowFlags_NoMove |
	                               ImGuiWindowFlags_NoScrollbar |
	                               ImGuiWindowFlags_NoScrollWithMouse |
	                               ImGuiWindowFlags_NoCollapse |
	                               ImGuiWindowFlags_NoBackground
	                             : 0;
	ImGui::Begin("Reverse bar", &showReverseBar, flags);
	if (!motherBoard) {
		ImGui::End();
		return;
	}

	TclObject command;
	auto& reverseManager = motherBoard->getReverseManager();
	if (reverseManager.isCollecting()) {
		auto b = reverseManager.getBegin();
		auto e = reverseManager.getEnd();
		auto c = reverseManager.getCurrent();
		auto snapshots = reverseManager.getSnapshotTimes();

		auto totalLength = e - b;
		auto playLength = c - b;
		auto recipLength = (totalLength != 0.0) ? (1.0 / totalLength) : 0.0;
		auto fraction = narrow_cast<float>(playLength * recipLength);

		gl::vec2 pos = ImGui::GetCursorScreenPos();
		gl::vec2 availableSize = ImGui::GetContentRegionAvail();
		gl::vec2 outerSize(availableSize[0], 2.0f * textHeight);
		gl::vec2 outerTopLeft = pos;
		gl::vec2 outerBottomRight = outerTopLeft + outerSize;

		gl::vec2 innerSize = outerSize - gl::vec2(2, 2);
		gl::vec2 innerTopLeft = outerTopLeft + gl::vec2(1, 1);
		gl::vec2 innerBottomRight = innerTopLeft + innerSize;
		gl::vec2 barBottomRight = innerTopLeft + gl::vec2(innerSize[0] * fraction, innerSize[1]);

		gl::vec2 middleTopLeft    (barBottomRight[0] - 2.0f, innerTopLeft[1]);
		gl::vec2 middleBottomRight(barBottomRight[0] + 2.0f, innerBottomRight[1]);

		const auto& io = ImGui::GetIO();
		bool hovered = ImGui::IsWindowHovered();
		bool replaying = reverseManager.isReplaying();
		if (!reverseHideTitle || !reverseFadeOut || replaying ||
		    ImGui::IsWindowDocked() || (ImGui::GetWindowViewport() != ImGui::GetMainViewport())) {
			reverseAlpha = 1.0f;
		} else {
			auto target = hovered ? 1.0f : 0.0f;
			auto period = hovered ? 0.5f : 5.0f; // TODO configurable speed
			auto step = io.DeltaTime / period;
			if (target > reverseAlpha) {
				reverseAlpha = std::min(target, reverseAlpha + step);
			} else {
				reverseAlpha = std::max(target, reverseAlpha - step);
			}
		}
		auto color = [&](gl::vec4 col) {
			return ImGui::ColorConvertFloat4ToU32(col * reverseAlpha);
		};

		auto* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(innerTopLeft, innerBottomRight, color(gl::vec4(0.0f, 0.0f, 0.0f, 0.5f)));

		for (double s : snapshots) {
			float x = narrow_cast<float>((s - b) * recipLength) * innerSize[0];
			drawList->AddLine(gl::vec2(innerTopLeft[0] + x, innerTopLeft[1]),
			                  gl::vec2(innerTopLeft[0] + x, innerBottomRight[1]),
			                  color(gl::vec4(0.25f, 0.25f, 0.25f, 1.00f)));
		}

		static constexpr std::array barColors = {
			std::array{gl::vec4(0.00f, 1.00f, 0.27f, 0.63f), gl::vec4(0.00f, 0.73f, 0.13f, 0.63f),
			           gl::vec4(0.07f, 0.80f, 0.80f, 0.63f), gl::vec4(0.00f, 0.87f, 0.20f, 0.63f)}, // view-only
			std::array{gl::vec4(0.00f, 0.27f, 1.00f, 0.63f), gl::vec4(0.00f, 0.13f, 0.73f, 0.63f),
			           gl::vec4(0.07f, 0.80f, 0.80f, 0.63f), gl::vec4(0.00f, 0.20f, 0.87f, 0.63f)}, // replaying
			std::array{gl::vec4(1.00f, 0.27f, 0.00f, 0.63f), gl::vec4(0.87f, 0.20f, 0.00f, 0.63f),
			           gl::vec4(0.80f, 0.80f, 0.07f, 0.63f), gl::vec4(0.73f, 0.13f, 0.00f, 0.63f)}, // recording
		};
		int barColorsIndex = replaying ? (reverseManager.isViewOnlyMode() ? 0 : 1)
		                               : 2;
		const auto& barColor = barColors[barColorsIndex];
		drawList->AddRectFilledMultiColor(
			innerTopLeft, barBottomRight,
			color(barColor[0]), color(barColor[1]), color(barColor[2]), color(barColor[3]));

		drawList->AddRectFilled(middleTopLeft, middleBottomRight, color(gl::vec4(1.0f, 0.5f, 0.0f, 0.75f)));
		drawList->AddRect(
			outerTopLeft, outerBottomRight, color(gl::vec4(1.0f)), 0.0f, 0, 2.0f);

		gl::vec2 cursor = ImGui::GetCursorPos();
		ImGui::SetCursorPos(cursor + gl::vec2(outerSize[0] * 0.15f, textHeight * 0.5f));
		auto time1 = formatTime(playLength);
		auto time2 = formatTime(totalLength);
		ImGui::TextColored(gl::vec4(1.0f) * reverseAlpha, "%s / %s", time1.c_str(), time2.c_str());

		if (hovered && ImGui::IsMouseHoveringRect(outerTopLeft, outerBottomRight)) {
			float ratio = (io.MousePos.x - pos[0]) / outerSize[0];
			auto timeOffset = totalLength * double(ratio);

			ImGui::BeginTooltip();
			ImGui::TextUnformatted(formatTime(timeOffset).c_str());
			ImGui::EndTooltip();

			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
				command.addListElement("reverse", "goto", b + timeOffset);
			}
		}

		ImGui::SetCursorPos(cursor); // cover full window for context menu
		ImGui::Dummy(availableSize);
		if (ImGui::BeginPopupContextItem("reverse context menu")) {
			ImGui::Checkbox("Hide title", &reverseHideTitle);
			ImGui::Indent();
			ImGui::BeginDisabled(!reverseHideTitle);
			ImGui::Checkbox("Fade out", &reverseFadeOut);
			ImGui::EndDisabled();
			ImGui::Unindent();
			ImGui::EndPopup();
		}
	} else {
		ImGui::TextUnformatted("Reverse is disabled.");
		if (ImGui::Button("Enable")) {
			command.addListElement("reverse", "start");
		}
	}

	ImGui::End();
	execute(command);
}

void ImGuiLayer::loadIcons()
{
	iconsTotalSize = gl::ivec2();
	iconsMaxSize = gl::ivec2();
	iconsNumEnabled = 0;
	FileContext context = systemFileContext();
	for (auto& icon : iconInfo) {
		auto load = [&](IconInfo::Icon& i) {
			try {
				if (!i.filename.empty()) {
					auto r = context.resolve(i.filename);
					i.tex = loadTexture(context.resolve(i.filename), i.size);
					return;
				}
			} catch (...) {
				// nothing
			}
			i.tex.reset();
			i.size = {};
		};
		load(icon.on);
		load(icon.off);
		if (icon.enable) {
			++iconsNumEnabled;
			auto m = max(icon.on.size, icon.off.size);
			iconsTotalSize += m;
			iconsMaxSize = max(iconsMaxSize, m);
		}
	}
	iconInfoDirty = false;
}

void ImGuiLayer::drawIcons()
{
	if (iconInfoDirty) {
		loadIcons();
	}

	const auto& style = ImGui::GetStyle();
	auto windowPadding = 2.0f * gl::vec2(style.WindowPadding);
	auto totalSize = windowPadding + gl::vec2(iconsTotalSize) + float(iconsNumEnabled) * gl::vec2(style.ItemSpacing);
	auto minSize = iconsHorizontal
		? gl::vec2(totalSize[0], float(iconsMaxSize[1]) + windowPadding[1])
		: gl::vec2(float(iconsMaxSize[0]) + windowPadding[0], totalSize[1]);
	if (!iconsHideTitle) {
		minSize[1] += 2.0f * style.FramePadding.y + ImGui::GetTextLineHeight();
	}
	auto maxSize = iconsHorizontal
		? gl::vec2(FLT_MAX, minSize[1])
		: gl::vec2(minSize[0], FLT_MAX);
	ImGui::SetNextWindowSizeConstraints(minSize, maxSize);

	int flags = iconsHideTitle ? ImGuiWindowFlags_NoTitleBar |
	                             ImGuiWindowFlags_NoResize |
	                             ImGuiWindowFlags_NoScrollbar |
	                             ImGuiWindowFlags_NoScrollWithMouse |
	                             ImGuiWindowFlags_NoCollapse |
	                             ImGuiWindowFlags_NoBackground |
	                             (iconsAllowMove ? 0 : ImGuiWindowFlags_NoMove)
	                           : 0;
	if (ImGui::Begin("Icons", &showIcons, flags | ImGuiWindowFlags_HorizontalScrollbar)) {
		auto cursor0 = ImGui::GetCursorPos();
		auto availableSize = ImGui::GetContentRegionAvail();
		float slack = iconsHorizontal ? (availableSize.x - totalSize[0])
		                              : (availableSize.y - totalSize[1]);
		float spacing = (iconsNumEnabled >= 2) ? (std::max(0.0f, slack) / float(iconsNumEnabled)) : 0.0f;

		bool fade = iconsHideTitle && !ImGui::IsWindowDocked() &&
		            (ImGui::GetWindowViewport() == ImGui::GetMainViewport());
		for (auto& icon : iconInfo) {
			if (!icon.enable) continue;

			bool state = [&] {
				try {
					return icon.expr.evalBool(interp);
				} catch (CommandException&) {
					return false; // TODO report warning??
				}
			}();
			if (state != icon.lastState) {
				icon.lastState = state;
				icon.time = 0.0f;
			}
			const auto& io = ImGui::GetIO();
			icon.time += io.DeltaTime;
			float alpha = [&] {
				if (!fade || !icon.fade) return 1.0f;
				auto t = icon.time - iconsFadeDelay;
				if (t <= 0.0f) return 1.0f;
				if (t >= iconsFadeDuration) return 0.0f;
				return 1.0f - (t / iconsFadeDuration);
			}();

			auto& ic = state ? icon.on : icon.off;
			gl::vec2 cursor = ImGui::GetCursorPos();
			ImGui::Image(reinterpret_cast<void*>(ic.tex.get()), gl::vec2(ic.size),
			             {0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, alpha});

			ImGui::SetCursorPos(cursor);
			auto size = gl::vec2(max(icon.on.size, icon.off.size));
			(iconsHorizontal ? size[0] : size[1]) += spacing;
			ImGui::Dummy(size);
			if (iconsHorizontal) ImGui::SameLine();
		}

		ImGui::SetCursorPos(cursor0); // cover full window for context menu
		ImGui::Dummy(availableSize);
		if (ImGui::BeginPopupContextItem("icons context menu")) {
			if (ImGui::MenuItem("Configure ...")) {
				showConfigureIcons = true;
			}
			ImGui::EndPopup();
		}
	}
	ImGui::End();
}

[[nodiscard]] static bool validExpression(const TclObject& expr, Interpreter& interp)
{
	try {
		[[maybe_unused]] bool val = expr.evalBool(interp);
		return true;
	} catch (...) {
		return false;
	}
}

void ImGuiLayer::drawConfigureIcons()
{
	if (iconInfoDirty) {
		loadIcons();
	}

	if (ImGui::Begin("Configure Icons", &showConfigureIcons)) {
		ImGui::TextUnformatted("Layout:");
		ImGui::SameLine();
		ImGui::RadioButton("Horizontal", &iconsHorizontal, 1);
		ImGui::SameLine();
		ImGui::RadioButton("Vertical", &iconsHorizontal, 0);
		ImGui::Separator();

		ImGui::Checkbox("Hide Title", &iconsHideTitle);
		ImGui::Indent();
		ImGui::BeginDisabled(!iconsHideTitle);
		ImGui::Checkbox("Allow move", &iconsAllowMove);
		HelpMarker("Click and drag the icon box.");
		ImGui::SliderFloat("Fade-out duration", &iconsFadeDuration, 0.0f, 30.0f);
		ImGui::SliderFloat("Fade-out delay",    &iconsFadeDelay,    0.0f, 30.0f);
		ImGui::EndDisabled();
		ImGui::Unindent();
		ImGui::Separator();

		ImGui::TextUnformatted("Advanced:");
		HelpMarker("Right-click to reorder, insert, delete.");
		int flags = ImGuiTableFlags_RowBg |
		            ImGuiTableFlags_BordersOuterH |
		            ImGuiTableFlags_BordersInnerH |
		            ImGuiTableFlags_BordersV |
		            ImGuiTableFlags_BordersOuter |
		            ImGuiTableFlags_Resizable;
		if (ImGui::BeginTable("table", 5, flags)) {
			ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Fade-out", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("True-image", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("False-image", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Expression");
			ImGui::TableHeadersRow();

			enum Cmd { MOVE_FRONT, MOVE_FWD, MOVE_BWD, MOVE_BACK, INSERT, DELETE };
			std::pair<int, Cmd> cmd(-1, MOVE_FRONT);
			auto lastRow = iconInfo.size() - 1;
			for (auto [row, icon] : enumerate(iconInfo)) {
				ImGui::PushID(narrow<int>(row));

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				auto pos = ImGui::GetCursorPos();
				const auto& style = ImGui::GetStyle();
				auto textHeight = ImGui::GetTextLineHeight();
				float rowHeight = std::max(2.0f * style.FramePadding.y + textHeight,
				                  std::max(float(icon.on.size[1]), float(icon.off.size[1])));
				ImGui::Selectable("##row", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap, ImVec2(0, rowHeight));
				if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
					ImGui::OpenPopup("config-icon-context");
				}
				if (ImGui::BeginPopup("config-icon-context")) {
					if (lastRow >= 1) { // at least 2 rows
						if (row != 0) {
							if (ImGui::MenuItem("Move to front")) cmd = {row, MOVE_FRONT};
							if (ImGui::MenuItem("Move forwards")) cmd = {row, MOVE_FWD};
						}
						if (row != lastRow) {
							if (ImGui::MenuItem("Move backwards"))cmd = {row, MOVE_BWD};
							if (ImGui::MenuItem("Move to back"))  cmd = {row, MOVE_BACK};
						}
						ImGui::Separator();
					}
					if (ImGui::MenuItem("Insert new row"))     cmd = {row, INSERT};
					if (ImGui::MenuItem("Delete current row")) cmd = {row, DELETE};
					ImGui::EndPopup();
				}

				ImGui::SetCursorPos(pos);
				if (ImGui::Checkbox("##enabled", &icon.enable)) {
					iconInfoDirty = true;
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::BeginDisabled(!iconsHideTitle);
				ImGui::Checkbox("##fade-out", &icon.fade);
				ImGui::EndDisabled();

				auto image = [&](IconInfo::Icon& icon) {
					if (icon.tex.get()) {
						ImGui::Image(reinterpret_cast<void*>(icon.tex.get()),
						             gl::vec2(icon.size));
					} else {
						ImGui::Button("Select ...");
					}
					if (ImGui::IsItemClicked()) {
						selectFile("Select image for icon", "PNG (*.png){.png}",
							[this, &icon](const std::string& filename) {
								icon.filename = filename;
								iconInfoDirty = true;
							});
						std::cerr << "clicked\n";
					//if (InputText("##true", icon.off.filename)) {
					//	iconInfoDirty = true;
					//}
					}
				};

				ImGui::TableSetColumnIndex(2);
				image(icon.on);

				ImGui::TableSetColumnIndex(3);
				image(icon.off);

				ImGui::TableSetColumnIndex(4);
				ImGui::SetNextItemWidth(-FLT_MIN);
				bool valid = validExpression(icon.expr, interp);
				ImGui::PushStyleColor(ImGuiCol_Text, valid ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
				                                           : ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
				auto expr = std::string(icon.expr.getString());
				if (ImGui::InputText("##expr", &expr)) {
					icon.expr = expr;
				}
				ImGui::PopStyleColor();

				ImGui::PopID();
			}
			ImGui::EndTable();

			if (int row = cmd.first; row != -1) {
				switch (cmd.second) {
				case MOVE_FRONT:
					assert(row >= 1);
					std::rotate(&iconInfo[0], &iconInfo[row], &iconInfo[row + 1]);
					break;
				case MOVE_FWD:
					assert(row >= 1);
					std::swap(iconInfo[row], iconInfo[row - 1]);
					break;
				case MOVE_BWD:
					assert(row < narrow<int>(iconInfo.size() - 1));
					std::swap(iconInfo[row], iconInfo[row + 1]);
					break;
				case MOVE_BACK:
					assert(row < narrow<int>(iconInfo.size() - 1));
					std::rotate(&iconInfo[row], &iconInfo[row + 1], &iconInfo[lastRow + 1]);
					break;
				case INSERT:
					iconInfo.emplace(iconInfo.begin() + row, TclObject("true"), "", "", true);
					break;
				case DELETE:
					iconInfo.erase(iconInfo.begin() + row);
					break;
				}
				iconInfoDirty = true;
			}
		}

		if (ImGui::Button("Restore default")) {
			setDefaultIcons();
		}
	}
	ImGui::End();
}

void ImGuiLayer::debuggableMenu(MSXMotherBoard* motherBoard)
{
	if (!ImGui::BeginMenu("Debugger", motherBoard != nullptr)) {
		return;
	}
	assert(motherBoard);

	ImGui::MenuItem("disassembly TODO");
	ImGui::MenuItem("CPU registers TODO");
	ImGui::MenuItem("stack TODO");
	ImGui::MenuItem("memory TODO");
	ImGui::Separator();
	ImGui::MenuItem("VDP bitmap viewer", nullptr, &showBitmapViewer);
	ImGui::MenuItem("TODO several more");
	ImGui::Separator();
	if (ImGui::BeginMenu("All debuggables")) {
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
	ImGui::EndMenu();
}

static const char* getComboString(int item, const char* itemsSeparatedByZeros)
{
	const char* p = itemsSeparatedByZeros;
	while (true) {
		assert(*p);
		if (item == 0) return p;
		--item;
		while (*p) ++p;
		++p;
	}
}

// TODO avoid code duplication with src/video/BitmapConverter
void ImGuiLayer::renderBitmap(std::span<const uint8_t> vram, std::span<const uint32_t, 16> palette16,
                              int mode, int lines, int page, uint32_t* output)
{
	auto yjk2rgb = [](int y, int j, int k) -> std::tuple<int, int, int> {
		// Note the formula for 'blue' differs from the 'traditional' formula
		// (e.g. as specified in the V9958 datasheet) in the rounding behavior.
		// Confirmed on real turbor machine. For details see:
		//    https://github.com/openMSX/openMSX/issues/1394
		//    https://twitter.com/mdpc___/status/1480432007180341251?s=20
		int r = std::clamp(y + j,                       0, 31);
		int g = std::clamp(y + k,                       0, 31);
		int b = std::clamp((5 * y - 2 * j - k + 2) / 4, 0, 31);
		return {r, g, b};
	};

	// TODO handle less than 128kB VRAM (will crash now)
	size_t addr = 0x8000 * page;
	switch (mode) {
	case SCR5:
		for (auto y : xrange(lines)) {
			auto* line = &output[256 * y];
			for (auto x : xrange(128)) {
				auto value = vram[addr];
				line[2 * x + 0] = palette16[(value >> 4) & 0x0f];
				line[2 * x + 1] = palette16[(value >> 0) & 0x0f];
				++addr;
			}
		}
		break;

	case SCR6:
		for (auto y : xrange(lines)) {
			auto* line = &output[512 * y];
			for (auto x : xrange(128)) {
				auto value = vram[addr];
				line[4 * x + 0] = palette16[(value >> 6) & 3];
				line[4 * x + 1] = palette16[(value >> 4) & 3];
				line[4 * x + 2] = palette16[(value >> 2) & 3];
				line[4 * x + 3] = palette16[(value >> 0) & 3];
				++addr;
			}
		}
		break;

	case SCR7:
		for (auto y : xrange(lines)) {
			auto* line = &output[512 * y];
			for (auto x : xrange(128)) {
				auto value0 = vram[addr + 0x00000];
				auto value1 = vram[addr + 0x10000];
				line[4 * x + 0] = palette16[(value0 >> 4) & 0x0f];
				line[4 * x + 1] = palette16[(value0 >> 0) & 0x0f];
				line[4 * x + 2] = palette16[(value1 >> 4) & 0x0f];
				line[4 * x + 3] = palette16[(value1 >> 0) & 0x0f];
				++addr;
			}
		}
		break;

	case SCR8: {
		auto toColor = [](uint8_t value) {
			int r = (value & 0x1c) >> 2;
			int g = (value & 0xe0) >> 5;
			int b = (value & 0x03) >> 0;
			int rr = (r << 5) | (r << 2) | (r >> 1);
			int gg = (g << 5) | (g << 2) | (g >> 1);
			int bb = (b << 6) | (b << 4) | (b << 2) | (b << 0);
			int aa = 255;
			return (rr << 0) | (gg << 8) | (bb << 16) | (aa << 24);
		};
		for (auto y : xrange(lines)) {
			auto* line = &output[256 * y];
			for (auto x : xrange(128)) {
				line[2 * x + 0] = toColor(vram[addr + 0x00000]);
				line[2 * x + 1] = toColor(vram[addr + 0x10000]);
				++addr;
			}
		}
		break;
	}

	case SCR11:
		for (auto y : xrange(lines)) {
			auto* line = &output[256 * y];
			for (auto x : xrange(64)) {
				std::array<unsigned, 4> p = {
					vram[addr + 0 + 0x00000],
					vram[addr + 0 + 0x10000],
					vram[addr + 1 + 0x00000],
					vram[addr + 1 + 0x10000],
				};
				addr += 2;
				int j = narrow<int>((p[2] & 7) + ((p[3] & 3) << 3)) - narrow<int>((p[3] & 4) << 3);
				int k = narrow<int>((p[0] & 7) + ((p[1] & 3) << 3)) - narrow<int>((p[1] & 4) << 3);
				for (auto n : xrange(4)) {
					uint32_t pix;
					if (p[n] & 0x08) {
						pix = palette16[p[n] >> 4];
					} else {
						int Y = narrow<int>(p[n] >> 3);
						auto [r, g, b] = yjk2rgb(Y, j, k);
						int rr = (r << 3) | (r >> 2);
						int gg = (g << 3) | (g >> 2);
						int bb = (b << 3) | (b >> 2);
						int aa = 255;
						pix = (rr << 0) | (gg << 8) | (bb << 16) | (aa << 24);
					}
					line[4 * x + n] = pix;
				}
			}
		}
		break;

	case SCR12:
		for (auto y : xrange(lines)) {
			auto* line = &output[256 * y];
			for (auto x : xrange(64)) {
				std::array<unsigned, 4> p = {
					vram[addr + 0 + 0x00000],
					vram[addr + 0 + 0x10000],
					vram[addr + 1 + 0x00000],
					vram[addr + 1 + 0x10000],
				};
				addr += 2;
				int j = narrow<int>((p[2] & 7) + ((p[3] & 3) << 3)) - narrow<int>((p[3] & 4) << 3);
				int k = narrow<int>((p[0] & 7) + ((p[1] & 3) << 3)) - narrow<int>((p[1] & 4) << 3);
				for (auto n : xrange(4)) {
					int Y = narrow<int>(p[n] >> 3);
					auto [r, g, b] = yjk2rgb(Y, j, k);
					int rr = (r << 3) | (r >> 2);
					int gg = (g << 3) | (g >> 2);
					int bb = (b << 3) | (b >> 2);
					int aa = 255;
					line[4 * x + n] = (rr << 0) | (gg << 8) | (bb << 16) | (aa << 24);
				}
			}
		}
		break;

	case OTHER:
		for (auto y : xrange(lines)) {
			auto* line = &output[256 * y];
			for (auto x : xrange(256)) {
				line[x] = 0xFF808080; // gray
			}
		}
		break;
	}
}

void ImGuiLayer::bitmapViewer(MSXMotherBoard* motherBoard)
{
	auto parseMode = [](DisplayMode mode) {
		auto base = mode.getBase();
		if (base == DisplayMode::GRAPHIC4) return SCR5;
		if (base == DisplayMode::GRAPHIC5) return SCR6;
		if (base == DisplayMode::GRAPHIC6) return SCR7;
		if (base != DisplayMode::GRAPHIC7) return OTHER;
		if (mode.getByte() & DisplayMode::YJK) {
			if (mode.getByte() & DisplayMode::YAE) {
				return SCR11;
			} else {
				return SCR12;
			}
		} else {
			return SCR8;
		}
	};
	auto modeToStr = [](int vdpMode) {
		if (vdpMode == SCR5 ) return "screen 5";
		if (vdpMode == SCR6 ) return "screen 6";
		if (vdpMode == SCR7 ) return "screen 7";
		if (vdpMode == SCR8 ) return "screen 8";
		if (vdpMode == SCR11) return "screen 11";
		if (vdpMode == SCR12) return "screen 12";
		if (vdpMode == OTHER) return "non-bitmap";
		assert(false); return "ERROR";
	};

	VDP* vdp = dynamic_cast<VDP*>(motherBoard->findDevice("VDP")); // TODO name based OK?
	if (ImGui::Begin("Bitmap viewer", &showBitmapViewer) && vdp) {
		int vdpMode = parseMode(vdp->getDisplayMode());

		int vdpPages = vdpMode <= SCR6 ? 4 : 2;
		int vdpPage = vdp->getDisplayPage();
		if (vdpPage >= vdpPages) vdpPage &= 1;

		int vdpLines = (vdp->getNumberOfLines() == 192) ? 0 : 1;

		int vdpColor0 = [&]{
			if (vdpMode == one_of(SCR8, SCR11, SCR12) || !vdp->getTransparency()) {
				return 16; // no replacement
			}
			return vdp->getBackgroundColor() & 15;
		}();

		ImGui::BeginGroup();
		ImGui::RadioButton("Use VDP settings", &bitmapManual, 0);
		ImGui::BeginDisabled(bitmapManual != 0);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Screen mode: %s", modeToStr(vdpMode));
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Display page: %d", vdpPage);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Visible lines: %d", vdpLines ? 212 : 192);
		ImGui::AlignTextToFramePadding();
		static const char* const color0Str = "0\0001\0002\0003\0004\0005\0006\0007\0008\0009\00010\00011\00012\00013\00014\00015\000none\000";
		ImGui::Text("Replace color 0: %s", getComboString(vdpColor0, color0Str));
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Interlace: %s", "TODO");
		ImGui::EndDisabled();
		ImGui::EndGroup();

		ImGui::SameLine();
		ImGui::BeginGroup();
		ImGui::RadioButton("Manual override", &bitmapManual, 1);
		ImGui::BeginDisabled(bitmapManual != 1);
		ImGui::PushItemWidth(ImGui::GetFontSize() * 9.0f);
		ImGui::Combo("##Screen mode", &bitmapScrnMode, "screen 5\000screen 6\000screen 7\000screen 8\000screen 11\000screen 12\000");
		int numPages = bitmapScrnMode <= SCR6 ? 4 : 2; // TODO extended VRAM
		if (bitmapPage >= numPages) bitmapPage = numPages - 1;
		ImGui::Combo("##Display page", &bitmapPage, numPages == 2 ? "0\0001\000" : "0\0001\0002\0003\000");
		ImGui::Combo("##Visible lines", &bitmapLines, "192\000212\000256\000");
		ImGui::Combo("##Color 0 replacement", &bitmapColor0, color0Str);
		ImGui::PopItemWidth();
		ImGui::EndDisabled();
		ImGui::EndGroup();

		ImGui::SameLine();
		ImGui::Dummy(ImVec2(25, 1));
		ImGui::SameLine();
		ImGui::BeginGroup();
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
		ImGui::Combo("Palette", &bitmapPalette, "Current VDP\000Fixed\000Custom (TODO)\000");
		ImGui::Button("Edit custom palette TODO");
		ImGui::Separator();
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.0f);
		ImGui::Combo("Zoom", &bitmapZoom, "1x\0002x\0003x\0004x\0005x\0006x\0007x\0008x\000");
		ImGui::Checkbox("grid", &bitmapGrid);
		ImGui::SameLine();
		ImGui::BeginDisabled(!bitmapGrid);
		ImGui::ColorEdit4("Grid color", &bitmapGridColor[0],
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
		ImGui::EndDisabled();
		ImGui::EndGroup();

		ImGui::Separator();

		auto& vram = vdp->getVRAM();
		int mode   = bitmapManual ? bitmapScrnMode : vdpMode;
		int page   = bitmapManual ? bitmapPage     : vdpPage;
		int lines  = bitmapManual ? bitmapLines    : vdpLines;
		int color0 = bitmapManual ? bitmapColor0   : vdpColor0;
		int width  = mode == one_of(SCR6, SCR7) ? 512 : 256;
		int height = (lines == 0) ? 192
		           : (lines == 1) ? 212
		                          : 256;

		auto msxColor = [](int r, int g, int b) -> uint32_t {
			int rr = (r << 5) | (r << 2) | (r >> 1);
			int gg = (g << 5) | (g << 2) | (g >> 1);
			int bb = (b << 5) | (b << 2) | (b >> 1);
			int aa = 255;
			return (rr << 0) | (gg << 8) | (bb << 16) | (aa << 24);
		};
		std::array<uint32_t, 16> palette16;
		if (bitmapPalette == 0) {
			auto msxPalette = vdp->getPalette();
			for (auto i : xrange(16)) {
				int r = (msxPalette[i] >> 4) & 7;
				int g = (msxPalette[i] >> 8) & 7;
				int b = (msxPalette[i] >> 0) & 7;
				palette16[i] = msxColor(r, g, b);
			}
		} else {
			static constexpr std::array<uint32_t, 16> fixedPalette = {
				msxColor(0, 0, 0),
				msxColor(0, 0, 0),
				msxColor(1, 6, 1),
				msxColor(3, 7, 3),
				msxColor(1, 1, 7),
				msxColor(2, 3, 7),
				msxColor(5, 1, 1),
				msxColor(2, 6, 7),
				msxColor(7, 1, 1),
				msxColor(7, 3, 3),
				msxColor(6, 6, 1),
				msxColor(6, 6, 4),
				msxColor(1, 4, 1),
				msxColor(6, 2, 5),
				msxColor(5, 5, 5),
				msxColor(7, 7, 7),
			};
			ranges::copy(fixedPalette, palette16);
		}
		if (color0 < 16) palette16[0] = palette16[color0];

		std::array<uint32_t, 512 * 256> pixels;
		renderBitmap(vram.getData(), palette16, mode, height, page,
		             pixels.data());
		bitmapTex.bind();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
		             GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		int zx = (1 + bitmapZoom) * (width == 256 ? 2 : 1);
		int zy = (1 + bitmapZoom) * 2;

		if (ImGui::BeginChild("##bitmap", ImGui::GetContentRegionAvail(), false, ImGuiWindowFlags_HorizontalScrollbar)) {
			auto pos = ImGui::GetCursorPos();
			ImVec2 size(float(width * zx), float(height * zy));
			ImGui::Image(reinterpret_cast<void*>(bitmapTex.get()), size);

			if (bitmapGrid && (zx > 1) && (zy > 1)) {
				auto color = ImGui::ColorConvertFloat4ToU32(bitmapGridColor);
				for (auto y : xrange(zy)) {
					auto* line = &pixels[y * zx];
					for (auto x : xrange(zx)) {
						line[x] = (x == 0 || y == 0) ? color : 0;
					}
				}
				bitmapGridTex.bind();
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, zx, zy, 0,
				             GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
				ImGui::SetCursorPos(pos);
				ImGui::Image(reinterpret_cast<void*>(bitmapGridTex.get()), size,
				             ImVec2(0.0f, 0.0f), ImVec2(float(width), float(height)));
			}
		}
		ImGui::EndChild();
	}
	ImGui::End();
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
	if (showIcons) {
		drawIcons();
	}
	if (showConfigureIcons) {
		drawConfigureIcons();
	}

	if (motherBoard) {
		if (showReverseBar) {
			drawReverseBar(motherBoard);
		}
		// Show the enabled 'debuggables'
		auto& debugger = motherBoard->getDebugger();
		for (const auto& [name, editor] : debuggables) {
			if (editor->Open) {
				if (auto* debuggable = debugger.findDebuggable(name)) {
					editor->DrawWindow(name.c_str(), *debuggable);
				}
			}
		}
		if (showBitmapViewer) {
			bitmapViewer(motherBoard);
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

	if (ImGui::BeginMainMenuBar()) {
		mediaMenu(motherBoard);
		connectorsMenu(motherBoard);
		saveStateMenu(motherBoard);
		settingsMenu();
		debuggableMenu(motherBoard);
		ImGui::EndMainMenuBar();
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

		ImGui::Checkbox("Icons window", &showIcons);

		SliderFloat("noise", rendererSettings.getNoiseSetting(), "%.1f");
	}
	ImGui::End();
	if (first) {
		// on startup, focus main openMSX window instead of the GUI window
		first = false;
		ImGui::SetWindowFocus(nullptr);
	}

	// (Modal) file dialog
	auto* fileDialog = ImGuiFileDialog::Instance();
	if (fileDialog->Display("FileDialog", ImGuiWindowFlags_NoCollapse, ImVec2(480.0f, 360.0f))) {
		if (fileDialog->IsOk() && openFileCallback) {
			lastPath[lastFileDialog] = fileDialog->GetCurrentPath();
			std::string filePathName = fileDialog->GetFilePathName();
			openFileCallback(filePathName);
			openFileCallback = {};
		}
		fileDialog->Close();
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
