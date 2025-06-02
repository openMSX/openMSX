#include "ImGuiMachine.hh"

#include "CustomFont.h"
#include "ImGuiConnector.hh"
#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiMedia.hh"
#include "ImGuiUtils.hh"

#include "BooleanSetting.hh"
#include "CartridgeSlotManager.hh"
#include "Debuggable.hh"
#include "Debugger.hh"
#include "FileException.hh"
#include "GlobalSettings.hh"
#include "HardwareConfig.hh"
#include "MSXCommandController.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "RealDrive.hh"
#include "RomInfo.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"

#include "enumerate.hh"
#include "narrow.hh"

#include <imgui_stdlib.h>
#include <imgui.h>

#include <algorithm>
#include <memory>

using namespace std::literals;

constexpr static std::string_view EMPTY = "(empty)";

static void showMachineWithoutInfo(const std::string_view configName)
{
	ImGui::StrCat("Current machine: ", configName, " (can't load this machine config to show more info)");
}

namespace openmsx {

static constexpr array_with_enum_index<SetupDepth, zstring_view> depthNodeNames = {
	"None",
	"Machine",
	"Extensions",
	"Connectors",
	"Media",
	"Run time state"
};


ImGuiMachine::ImGuiMachine(ImGuiManager& manager_)
	: ImGuiPart(manager_)
	, setupFileList("setup", Reactor::SETUP_EXTENSION, Reactor::SETUP_DIR)
	, confirmDialog("Confirm##setup")
{

	setupFileList.drawAction = [&] {
		im::Group([&]{
			setupFileList.drawTable();
			im::Disabled(previewSetup.fullName.empty(), [&] {
				if (ImGui::Button("Load")) {
					manager.executeDelayed([this] {
						try {
							auto& reactor = manager.getReactor();
							reactor.switchMachineFromSetup(previewSetup.fullName);
							if (setSetupAsDefault) {
								reactor.getDefaultSetupSetting().setString(previewSetup.name);
							}
						} catch (MSXException& e) {
							// this will be very rare, don't bother showing the error
							previewSetup.lastExceptionMessage = e.getMessage();
						}
					});
				}		
			});
		});
		ImGui::SameLine();
		im::Group([&]{
			if (previewSetup.motherBoard) {
				showSetupOverview(*previewSetup.motherBoard);
			} else {
				showNonExistingPreview();
			}
		});
	};

	setupFileList.displayColor = [&](const FileListWidget::Entry& entry) {
		return !previewSetup.lastExceptionMessage.empty() && previewSetup.fullName == entry.fullName ? imColor::ERROR : imColor::TEXT;
	};

	setupFileList.singleClickAction = [&](const FileListWidget::Entry& entry) {
		if (previewSetup.fullName == entry.fullName) return;

		// record entry names, but (so far) without loaded motherboard
		// this prevents that when loading failed, we don't continue retrying
		previewSetup.fullName = entry.fullName;
		previewSetup.name = entry.getDefaultDisplayName();
		// but we shouldn't reset the motherBoard yet during painting...

		loadPreviewSetup();
	};

	setupFileList.doubleClickAction = [&](const FileListWidget::Entry& entry) {
		// only execute if there was no error when previewing this entry (if we did)
		if (entry.fullName == previewSetup.fullName && !previewSetup.lastExceptionMessage.empty()) return;
		manager.executeDelayed([&entry = entry, &manager = manager] {
			try {
				manager.getReactor().switchMachineFromSetup(entry.fullName);
			} catch (MSXException& e) {
				// this will be very rare, don't bother showing the error
			}

		});
	};

	setupFileList.displayName = [&](const FileListWidget::Entry& entry) {
		auto defaultDisplayName = entry.getDefaultDisplayName();
		bool isDefault = manager.getReactor().getDefaultSetupSetting().getString() == defaultDisplayName;
		return strCat(defaultDisplayName, isDefault ? " [default]" : "");
	};

	setupFileList.deleteAction = [&](const FileListWidget::Entry& entry) {
		setupFileList.defaultDeleteAction(entry);
		auto& defaultSetting = manager.getReactor().getDefaultSetupSetting();
		if (defaultSetting.getString() == previewSetup.name) {
			// user just deleted the default. Let's clear it then as well, to avoid an error next startup.
			defaultSetting.setString("");
		}
	};
}

void ImGuiMachine::save(ImGuiTextBuffer& buf)
{
	for (const auto& item : recentMachines) {
		buf.appendf("machine.recent=%s\n", item.c_str());
	}
}

void ImGuiMachine::loadLine(std::string_view name, zstring_view value)
{
	if (name == "machine.recent") {
		recentMachines.push_back(value);
	}
}

void ImGuiMachine::showMenu(MSXMotherBoard* motherBoard)
{
	bool loadSetupOpen = false;
	im::Menu("Machine", [&]{
		using enum SetupDepth;

		auto& reactor = manager.getReactor();
		const auto& hotKey = reactor.getHotKey();

		ImGui::MenuItem("Select MSX machine...", nullptr, &showSelectMachine);

		auto showSetupLevelSelector = [&](const std::string& displayText, const bool includeNone, SetupDepth currentDepth) -> SetupDepth {

			static constexpr array_with_enum_index<SetupDepth, zstring_view> helpText = {
				"Do not save.",
				"Only the machine itself, without anything in it.",
				"The machine with all plugged in extensions.",
				"The machine, with all plugged in extensions and all things that are plugged into the connectors.",
				"The machine, with all plugged in extensions and all things that are plugged into the connectors and also all inserted media.",
				"The full state of the machine, with everything that's in it at the current time."
			};

			auto depthNodeNameForCombo = [&](SetupDepth depth){
				return tmpStrCat(depth == one_of(NONE, MACHINE) ? "" : "+ ", depthNodeNames[depth]).c_str();
			};

			SetupDepth selectedDepth = currentDepth;
			im::Combo(displayText.c_str(), depthNodeNameForCombo(currentDepth), [&]{
				const auto indent = ImGui::CalcTextSize("m").x;
				for (auto d_ : xrange(std::to_underlying(includeNone ? NONE : MACHINE), std::to_underlying(NUM))) {
					const auto d = static_cast<SetupDepth>(d_);
					if (d != one_of(NONE, MACHINE)) {
						ImGui::Indent(indent);
					}
					if (ImGui::Selectable(depthNodeNameForCombo(d))) {
						selectedDepth = d;
					}
					simpleToolTip(helpText[d]);
				}
			});
			HelpMarker("Select the level to include in the setup that will be saved. "
				"All levels above the one you selected will also be included.");
			return selectedDepth;
		};

		if (motherBoard) {
			ImGui::Separator();

			loadSetupOpen = setupFileList.menu("Load setup");

			saveSetupOpen = im::Menu("Save setup", true, [&]{
				ImGui::TextUnformatted("Save current setup:");

				saveSetupDepth = showSetupLevelSelector("Select level", false, saveSetupDepth);

				auto exists = [&]{
					auto filename = FileOperations::parseCommandFileArgument(
						saveSetupName, Reactor::SETUP_DIR, "", Reactor::SETUP_EXTENSION);
					return FileOperations::exists(filename);
				};
				if (!saveSetupOpen) {
					// on each re-open of this menu, create a suggestion for a name
					auto configName = motherBoard->getMachineName();
					auto* info = findMachineInfo(configName);
					auto initialSaveSetupName = info ? info->displayName : configName;
					saveSetupName = initialSaveSetupName;
					if (exists()) {
						saveSetupName = FileOperations::stem(FileOperations::getNextNumberedFileName(
							Reactor::SETUP_DIR, initialSaveSetupName, Reactor::SETUP_EXTENSION, true));
					}
				}
				ImGui::InputText("##save-setup-name", &saveSetupName);
				simpleToolTip(saveSetupName);
				ImGui::SameLine();
				if (ImGui::Button("Save")) {
					ImGui::CloseCurrentPopup();

					auto action = [manager = &manager, saveSetupName = saveSetupName, saveSetupDepth = saveSetupDepth] {
						if (auto motherBoard_ = manager->getReactor().getMotherBoard()) {
							// pass full filename
							auto filename = FileOperations::parseCommandFileArgument(
								saveSetupName, Reactor::SETUP_DIR, "", Reactor::SETUP_EXTENSION);
							motherBoard_->storeAsSetup(filename, saveSetupDepth);
						}
					};
					auto delayedAction = [manager = &manager, action] {
						manager->executeDelayed(action);
					};
					if (exists()) {
						confirmDialog.open(
							strCat("Overwrite setup with name '", saveSetupName, "'?"),
							delayedAction);
					} else {
						delayedAction();
						manager.getCliComm().printInfo(strCat("Setup saved to ", saveSetupName));
						if (setSetupAsDefault) {
							manager.getReactor().getDefaultSetupSetting().setString(saveSetupName);
						}
						setSetupAsDefault = false;
					}
				}
				ImGui::Checkbox("Set as default", &setSetupAsDefault);
				simpleToolTip("Check this to set the setup you are saving as default setup: load this setup when starting up openMSX if no other setup is specified.");
				ImGui::Separator();
				showSetupOverview(*motherBoard, ViewMode::SAVE);
			});

			im::Menu("Current setup", true, [&]{
				showSetupOverview(*motherBoard);
			});
		}

		setupSettingsOpen = im::Menu("Setup settings", true, [&]{
			auto currentSaveAtExitName = manager.getReactor().getSaveSetupAtExitNameSetting().getString();
			auto& defaultSetupSetting = reactor.getDefaultSetupSetting();
			bool startMachine = defaultSetupSetting.getString().empty();
			if (!setupSettingsOpen) {
				setups = Reactor::getSetups();
				// make sure previousDefaultSetup is initialized properly
				if (defaultSetupSetting.getString().empty()) {
					previousDefaultSetup = setups.empty() ? currentSaveAtExitName : setups.front();
				} else {
					previousDefaultSetup = defaultSetupSetting.getString();
				}
			}
			ImGui::TextUnformatted("When openMSX starts:");
			if (ImGui::RadioButton("Load a machine", startMachine)) {
				previousDefaultSetup = defaultSetupSetting.getValue().getString();
				defaultSetupSetting.setValue(TclObject());
			}
			HelpMarker("Select this option to load the specified machine when openMSX starts up, instead of a setup.");
			im::DisabledIndent(!startMachine, [&] {
				auto& allMachines = getAllMachines();
				auto currentDefault = reactor.getDefaultMachineSetting().getString();
				auto currentDefaultDisplay = [&] {
					if (auto* info = findMachineInfo(currentDefault)) {
						return info->displayName;
					}
					return std::string(currentDefault);
				}();
				im::Combo("##defaultMachine", currentDefaultDisplay.c_str(), [&]{
					for (auto& info: allMachines) {
						bool ok = getTestResult(info).empty();
						im::StyleColor(!ok, ImGuiCol_Text, getColor(imColor::ERROR), [&]{
							if (ImGui::Selectable(info.displayName.c_str())) {
								reactor.getDefaultMachineSetting().setValue(TclObject(info.configName));
								// clear the default setup setting to avoid loading that at startup
								defaultSetupSetting.setValue(TclObject());
							}
							if (info.configName == currentDefault && ImGui::IsWindowAppearing()) ImGui::SetScrollHereY();
							if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay | ImGuiHoveredFlags_Stationary)) {
								im::ItemTooltip([&]{
									printConfigInfo(info);
								});
							}
						});
					}
				});
			});
			if (ImGui::RadioButton("Load a setup", !startMachine)) {
				defaultSetupSetting.setValue(TclObject(previousDefaultSetup));
			}

			HelpMarker("Select this option to load the specified setup when openMSX starts up. "
				"Note that in case the setup cannot be loaded, the machine shown at the other "
				"option will be loaded after all.");
			im::DisabledIndent(startMachine, [&] {
				auto currentDefault = startMachine ? previousDefaultSetup : defaultSetupSetting.getString();
				auto showSetup = [&](zstring_view setup) {
					if (ImGui::Selectable(setup.c_str())) {
						defaultSetupSetting.setValue(TclObject(setup));
					}
					if (setup == currentDefault && ImGui::IsWindowAppearing()) ImGui::SetScrollHereY();
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay | ImGuiHoveredFlags_Stationary)) {
						if (previewSetup.name != setup) {
							try {
								previewSetup.fullName = userDataFileContext(Reactor::SETUP_DIR).resolve(tmpStrCat(setup, Reactor::SETUP_EXTENSION));
								loadPreviewSetup();
							} catch (FileException&) {
								manager.executeDelayed([this] {
									previewSetup.motherBoard.reset();
								});
							}
						}
						im::ItemTooltip([&]{
							if (previewSetup.motherBoard) {
								showSetupOverview(*previewSetup.motherBoard, ViewMode::NO_CONTROLS);
							} else {
								showNonExistingPreview();
							}
						});
					}
				};

				im::Combo("##defaultSetup", currentDefault.c_str(), [&]{
					for (auto& setup: setups) {
						showSetup(setup);
					}
					if (!contains(setups, currentSaveAtExitName)) {
						showSetup(currentSaveAtExitName);
					}
				});
			});
			ImGui::Separator();
			ImGui::TextUnformatted("When openMSX exits:");
			im::Indent([&] {
				ImGui::TextUnformatted("Save setup level");
				auto& depthSetting = manager.getReactor().getSaveSetupAtExitDepthSetting();
				auto currentDepth = depthSetting.getEnum();
				auto newDepth = showSetupLevelSelector("##empty", true, currentDepth);
				im::Disabled(currentDepth == NONE, [&] {
					ImGui::TextUnformatted("as");
					InputText("##save-setup-name", manager.getReactor().getSaveSetupAtExitNameSetting());
					HelpMarker("The setup name given here will be used when saving the setup at exit. "
						"Select 'None' for the level to not save the current setup at exit.");
				});
				if (newDepth != currentDepth) {
					depthSetting.setEnum(newDepth);
				}
			});
		});

		ImGui::Separator();

		if (motherBoard) {
			const auto& controller = motherBoard->getMSXCommandController();
			if (auto* firmwareSwitch = dynamic_cast<BooleanSetting*>(controller.findSetting("firmwareswitch"))) {
				Checkbox(hotKey, "Firmware switch", *firmwareSwitch);
			}
		}

		auto& pauseSetting = reactor.getGlobalSettings().getPauseSetting();
		bool pause = pauseSetting.getBoolean();
		if (auto shortCut = getShortCutForCommand(hotKey, "toggle pause");
		    ImGui::MenuItem("Pause", shortCut.c_str(), &pause)) {
			pauseSetting.setBoolean(pause);
		}

		if (auto shortCut = getShortCutForCommand(hotKey, "reset");
		    ImGui::MenuItem("Reset", shortCut.c_str(), nullptr, motherBoard != nullptr)) {
			manager.executeDelayed(TclObject("reset"));
		}

		auto& powerSetting = reactor.getGlobalSettings().getPowerSetting();
		bool power = powerSetting.getBoolean();
		if (auto shortCut = getShortCutForCommand(hotKey, "toggle power");
		    ImGui::MenuItem("Power", shortCut.c_str(), &power)) {
			powerSetting.setBoolean(power);
		}

		ImGui::Separator();
		ImGui::MenuItem("Test MSX hardware", nullptr, &showTestHardware);
	});

	confirmDialog.execute();

	if (!loadSetupOpen && !setupSettingsOpen && previewSetup.motherBoard) {
		manager.executeDelayed([this] {
			previewSetup.motherBoard.reset();
		});
	}
}

void ImGuiMachine::signalQuit()
{
	previewSetup.motherBoard.reset();
}

void ImGuiMachine::loadPreviewSetup()
{
	manager.executeDelayed([this] {
		try {
			// already reset, so that it's also gone in case of an exception
			previewSetup.motherBoard.reset();
			previewSetup.lastExceptionMessage.clear();
			auto newBoard = manager.getReactor().createEmptyMotherBoard();
			XmlInputArchive in(previewSetup.fullName);
			in.serialize("machine", *newBoard);
			previewSetup.motherBoard = newBoard;
		} catch (MSXException& e) {
			previewSetup.lastExceptionMessage = e.getMessage();
		}
	});
}

void ImGuiMachine::showNonExistingPreview()
{
	if (previewSetup.lastExceptionMessage.empty()) {
		ImGui::TextUnformatted("Nothing to preview...");
	} else {
		im::StyleColor(ImGuiCol_Text, getColor(imColor::ERROR), [&]{
			ImGui::StrCat("Setup ", previewSetup.name, " cannot be loaded:");
			ImGui::TextUnformatted(previewSetup.lastExceptionMessage);
		});
	}
}

void ImGuiMachine::showSetupOverview(MSXMotherBoard& motherBoard, ViewMode viewMode)
{
	using enum SetupDepth;

	auto configName = motherBoard.getMachineName();
	if (auto* info = findMachineInfo(configName)) {
		if (viewMode != ViewMode::SAVE) {
			ImGui::TextUnformatted(info->displayName);
		}
		if (viewMode != ViewMode::NO_CONTROLS) {
			im::TreeNode(depthNodeNames[MACHINE].c_str(), [&]{
				// alternatively, put this info in a tooltip instead of a collapsed TreeNode
				printConfigInfo(*info);
				});
		}
	} else {
		// machine config is gone... fallback: just show configName
		showMachineWithoutInfo(configName);
	}

	const ImGuiTreeNodeFlags flags = viewMode == ViewMode::VIEW ? ImGuiTreeNodeFlags_DefaultOpen :
					viewMode == ViewMode::NO_CONTROLS ? (ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet) :
					ImGuiTreeNodeFlags_None;

	im::StyleColor(viewMode == ViewMode::SAVE && saveSetupDepth < EXTENSIONS, ImGuiCol_Text, getColor(imColor::TEXT_DISABLED), [&]{
		im::TreeNode(depthNodeNames[EXTENSIONS].c_str(), flags, [&]{
			const auto& slotManager = motherBoard.getSlotManager();
			bool anySlot = false;
			im::Table("##ExtTable", 2, [&]{
				for (auto i : xrange(CartridgeSlotManager::MAX_SLOTS)) {
					if (!slotManager.slotExists(i)) continue;
					anySlot = true;
					if (ImGui::TableNextColumn()) {
						ImGui::StrCat("Slot ", char('A' + i), " (", slotManager.getPsSsString(i), ")");
					}
					if (ImGui::TableNextColumn()) {
						if (const auto* config = slotManager.getConfigForSlot(i)) {
							if (config->getType() == HardwareConfig::Type::EXTENSION) {
								ImGui::TextUnformatted(manager.media->displayNameForExtension(config->getConfigName()));
								if (auto* extInfo = manager.media->findExtensionInfo(config->getConfigName())) {
									manager.media->extensionTooltip(*extInfo);
								}
							} else {
								ImGui::TextDisabledUnformatted(manager.media->displayNameForRom(std::string(config->getRomFilename()), true));
							}
						} else {
							ImGui::TextUnformatted(EMPTY);
						}
					}
				}
				if (!anySlot) {
					ImGui::TextDisabledUnformatted("No cartridge slots present");
				}
				// still, there could be I/O port only extensions present.
				for (const auto& ext : motherBoard.getExtensions()) {
					if (!slotManager.findSlotWith(*ext)) {
						if (ImGui::TableNextColumn()) {
							ImGui::TextUnformatted("I/O only");
						}
						if (ImGui::TableNextColumn()) {
							ImGui::TextUnformatted(manager.media->displayNameForExtension(ext->getConfigName()));
							if (auto* extInfo = manager.media->findExtensionInfo(ext->getConfigName())) {
								manager.media->extensionTooltip(*extInfo);
							}
						};
					}
				}
			});
		});
	});
	im::StyleColor(viewMode == ViewMode::SAVE && saveSetupDepth < CONNECTORS, ImGuiCol_Text, getColor(imColor::TEXT_DISABLED), [&]{
		im::TreeNode(depthNodeNames[CONNECTORS].c_str(), flags, [&]{
			manager.connector->showPluggables(motherBoard.getPluggingController(), true);
		});
	});
	im::StyleColor(viewMode == ViewMode::SAVE && saveSetupDepth < MEDIA, ImGuiCol_Text, getColor(imColor::TEXT_DISABLED), [&]{
		im::TreeNode(depthNodeNames[MEDIA].c_str(), flags, [&]{
			im::Table("##MediaTable", 2, [&]{
				for (const auto& media : motherBoard.getMediaProviders()) {
					TclObject info;
					media.provider->getMediaInfo(info);
					if (auto target = info.getOptionalDictValue(TclObject("target"))) {
						bool isEmpty = target->getString().empty();
						auto targetStr = isEmpty ? EMPTY : target->getString();

						auto formatMediaName = [](std::string_view name) {
							constexpr auto multiSlotMediaDeviceTab = std::to_array<std::pair<std::string_view, std::string_view>>({
								{"cart", "Cartridge Slot"},
								{"disk", "Disk Drive"    },
								{"hd"  , "Hard Disk"     },
								{"cd"  , "CDROM Drive"   },
								{"ls"  , "LS120 Drive"   },
							});
							for (auto [s, l] : multiSlotMediaDeviceTab) {
								if (name.starts_with(s)) {
									return strCat(l, ' ', char('A' + (name.back() - 'a')));
								}
							}
							constexpr auto singleSlotMediaDeviceTab = std::to_array<std::pair<std::string_view, std::string_view>>({
								{"cassetteplayer" , "Tape Deck"       },
								{"laserdiscplayer", "LaserDisc Player"},
							});
							for (auto [s, l] : singleSlotMediaDeviceTab) {
								if (name == s) return std::string(l);
							}
							// fallback in case we add stuff and forget to update the tables (no need to crash on this)
							return std::string(name);
						};

						if (media.name.starts_with("cart")) {
							unsigned num = media.name[4] - 'a';
							const auto& slotManager = motherBoard.getSlotManager();
							if (ImGui::TableNextColumn()) {
								ImGui::StrCat(formatMediaName(media.name), " (", slotManager.getPsSsString(num), ")");
							}
							if (ImGui::TableNextColumn()) {
								auto type = info.getOptionalDictValue(TclObject("type"));
								if (type && type->getString() == "extension") {
									ImGui::TextDisabledUnformatted(manager.media->displayNameForExtension(targetStr));
								} else {
									ImGui::TextUnformatted(isEmpty ? EMPTY : manager.media->displayNameForRom(std::string(targetStr), true));
									if (!isEmpty) {
										im::ItemTooltip([&]{
											RomType romType = RomType::UNKNOWN;
											if (auto mapper = info.getOptionalDictValue(TclObject("mappertype"))) {
												romType = RomInfo::nameToRomType(mapper->getString());
											}
											ImGuiMedia::printRomInfo(manager, info, targetStr, romType);
										});
									}
								}
							}
						} else {
							if (ImGui::TableNextColumn()) {
								ImGui::TextUnformatted(formatMediaName(media.name));
							}
							if (ImGui::TableNextColumn()) {
								ImGui::TextUnformatted(FileOperations::getFilename(targetStr));
								simpleToolTip(targetStr);
							}
						}
					}
				}
			});
		});
	});
	auto time = (motherBoard.getCurrentTime() - EmuTime::zero()).toDouble();
	if (time > 0) {
		// this is only useful if the time is not 0
		im::StyleColor(viewMode == ViewMode::SAVE && saveSetupDepth < COMPLETE_STATE, ImGuiCol_Text, getColor(imColor::TEXT_DISABLED), [&]{
			im::TreeNode(depthNodeNames[COMPLETE_STATE].c_str(), flags, [&]{
				ImGui::StrCat("Machine time: ", formatTime(time));
			});
		});
	}
}

void ImGuiMachine::paint(MSXMotherBoard* motherBoard)
{
	if (showSelectMachine) {
		paintSelectMachine(motherBoard);
	}
	if (showTestHardware) {
		paintTestHardware();
	}
}


void ImGuiMachine::paintSelectMachine(const MSXMotherBoard* motherBoard)
{
	ImGui::SetNextWindowSize(gl::vec2{29, 26} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Select MSX machine", &showSelectMachine, [&]{
		auto& reactor = manager.getReactor();
		auto instances = reactor.getMachineIDs();
		auto currentInstance = reactor.getMachineID();
		if (instances.size() > 1 || currentInstance.empty()) {
			ImGui::TextUnformatted("Instances:"sv);
			HelpMarker("Switch between different machine instances. Right-click to delete an instance.");
			im::Indent([&]{
				float height = (std::min(4.0f, float(instances.size())) + 0.25f) * ImGui::GetTextLineHeightWithSpacing();
				im::ListBox("##empty", {-FLT_MIN, height}, [&]{
					im::ID_for_range(instances.size(), [&](int i) {
						const auto& name = instances[i];
						bool isCurrent = name == currentInstance;
						auto board = reactor.getMachine(name);
						std::string display = [&]{
							if (board) {
								auto configName = board->getMachineName();
								auto* info = findMachineInfo(configName);
								auto time = (board->getCurrentTime() - EmuTime::zero()).toDouble();
								return strCat(info ? info->displayName : configName, " (", formatTime(time), ')');
							} else {
								return std::string(name);
							}
						}();
						if (ImGui::Selectable(display.c_str(), isCurrent)) {
							manager.executeDelayed(makeTclList("activate_machine", name));
						}
						im::PopupContextItem("instance context menu", [&]{
							if (ImGui::Selectable("Delete instance")) {
								manager.executeDelayed(makeTclList("delete_machine", name));
							}
						});
					});
				});
			});
			ImGui::Separator();
		}

		if (motherBoard) {
			auto configName = motherBoard->getMachineName();
			auto* info = findMachineInfo(configName);
			if (info) {
				std::string display = strCat("Current machine: ", info->displayName);
				im::TreeNode(display.c_str(), [&]{
					printConfigInfo(*info);
				});
			} else {
				// machine config is gone... fallback: just show configName
				showMachineWithoutInfo(configName);
			}
			if (newMachineConfig.empty()) newMachineConfig = configName;
			ImGui::Separator();
		}

		auto showMachine = [&](MachineInfo& info, bool doubleClickToSelect) {
			bool ok = getTestResult(info).empty();
			im::StyleColor(!ok, ImGuiCol_Text, getColor(imColor::ERROR), [&]{
				bool selected = info.configName == newMachineConfig;
				if (ImGui::Selectable(info.displayName.c_str(), selected,
						doubleClickToSelect ? ImGuiSelectableFlags_AllowDoubleClick: ImGuiSelectableFlags_None)) {
					newMachineConfig = info.configName;
					if (ok && (doubleClickToSelect ? ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) : true)) {
						showSelectMachine = false; // close window
						manager.executeDelayed(makeTclList("machine", newMachineConfig));
						addRecentItem(recentMachines, newMachineConfig);
					}
				}
				if (selected) {
					if (ImGui::IsWindowAppearing()) ImGui::SetScrollHereY();
				}
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay | ImGuiHoveredFlags_Stationary)) {
					im::ItemTooltip([&]{
						printConfigInfo(info);
					});
				}
			});
		};

		im::TreeNode("Recently used", recentMachines.empty() ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_DefaultOpen, [&]{
			if (recentMachines.empty()) {
				ImGui::TextUnformatted("(none)"sv);
			} else {
				im::Combo("##recent", "Switch to recently used machine:", [&]{
					for (const auto& item : recentMachines) {
						if (auto* info = findMachineInfo(item)) {
							showMachine(*info, false);
						}
					}
				});
				simpleToolTip("Replace the current with the selected machine.");
			}
		});
		ImGui::Separator();

		ImGui::TextUnformatted("Available machines:"sv);
		auto& allMachines = getAllMachines();
		std::string filterDisplay = "filter";
		if (!filterType.empty() || !filterRegion.empty() || !filterString.empty()) strAppend(filterDisplay, ':');
		if (!filterType.empty()) strAppend(filterDisplay, ' ', filterType);
		if (!filterRegion.empty()) strAppend(filterDisplay, ' ', filterRegion);
		if (!filterString.empty()) strAppend(filterDisplay, ' ', filterString);
		strAppend(filterDisplay, "###filter");
		im::TreeNode(filterDisplay.c_str(), ImGuiTreeNodeFlags_DefaultOpen, [&]{
			displayFilterCombo(filterType, "Type", allMachines);
			displayFilterCombo(filterRegion, "Region", allMachines);
			if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
			ImGui::InputText(ICON_IGFD_FILTER, &filterString);
			simpleToolTip("A list of substrings that must be part of the machine name.\n"
					"\n"
					"For example: enter 'pa' to search for 'Panasonic' machines. "
					"Then refine the search by appending '<space>st' to find the 'Panasonic FS-A1ST' machine.");
		});

		auto filteredMachines = to_vector(xrange(allMachines.size()));
		applyComboFilter("Type",   filterType,   allMachines, filteredMachines);
		applyComboFilter("Region", filterRegion, allMachines, filteredMachines);
		applyDisplayNameFilter(filterString, allMachines, filteredMachines);

		auto it = std::ranges::find(filteredMachines, newMachineConfig,
			[&](auto idx) { return allMachines[idx].configName; });
		bool inFilteredList = it != filteredMachines.end();
		int selectedIdx = inFilteredList ? narrow<int>(*it) : -1;

		im::ListBox("##list", {-FLT_MIN, -ImGui::GetFrameHeightWithSpacing()}, [&]{
			im::ListClipper(filteredMachines.size(), selectedIdx, [&](int i) {
				auto idx = filteredMachines[i];
				auto& info = allMachines[idx];
				showMachine(info, true);
			});
		});

		bool ok = [&]{
			if (!inFilteredList) return false;
			auto* info = findMachineInfo(newMachineConfig);
			if (!info) return false;
			const auto& test = getTestResult(*info);
			return test.empty();
		}();
		im::Disabled(!ok, [&]{
			if (ImGui::Button("Replace current machine")) {
				manager.executeDelayed(makeTclList("machine", newMachineConfig));
			}
			simpleToolTip("Replace the current machine with the selected machine. "
					"Alternatively you can also double click in the list above (in addition that also closes this window).");
			ImGui::SameLine(0.0f, 10.0f);
			if (ImGui::Button("New machine instance")) {
				std::string script = strCat(
					"set id [create_machine]\n"
					"set err [catch {${id}::load_machine ", newMachineConfig, "} error_result]\n"
					"if {$err} {\n"
					"    delete_machine $id\n"
					"    error \"Error activating new machine: $error_result\"\n"
					"} else {\n"
					"    activate_machine $id\n"
					"}\n");
				manager.executeDelayed(TclObject(script));
			}
			simpleToolTip("Create a new machine instance (next to the current machine). "
					"Later you can switch between the different instances (like different tabs in a web browser).");
		});
	});
}

void ImGuiMachine::paintTestHardware()
{
	ImGui::SetNextWindowSize(gl::vec2{41.0f, 32.5f} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Test MSX hardware", &showTestHardware, [&]{
		auto formatNum = [](size_t num, std::string_view text) {
			if (num == 0) {
				return strCat("No ", text, "###", text);
			} else {
				return strCat(num, ' ', text, "###", text);
			}
		};
		auto printList = [](const auto& indices, const auto& infos) {
			auto n = narrow<int>(indices.size());
			auto clamped = std::clamp(narrow_cast<float>(n), 2.5f, 7.5f);
			gl::vec2 listSize{0.0f, clamped * ImGui::GetTextLineHeightWithSpacing()};

			ImGui::SetNextItemWidth(-FLT_MIN);
			im::ListBox("##workingList", listSize, [&]{
				im::ListClipper(n, [&](int i) {
					auto& info = infos[indices[i]];
					ImGui::TextUnformatted(info.displayName);
					assert(info.testResult);
					simpleToolTip(*info.testResult);
				});
			});
		};

		bool doTest = true;

		auto& allMachines = getAllMachines();
		std::vector<size_t> workingMachines, nonWorkingMachines;
		for (auto [idx, info] : enumerate(allMachines)) {
			if (!info.testResult.has_value()) {
				if (!doTest) continue; // only 1 test per iteration
				doTest = false;
			}
			const auto& result = getTestResult(info);
			(result.empty() ? workingMachines : nonWorkingMachines).push_back(idx);
		}
		bool allMachinesTested = allMachines.size() == (workingMachines.size() + nonWorkingMachines.size());

		im::VisuallyDisabled(!allMachinesTested, [&]{
			im::TreeNode("Machines", ImGuiTreeNodeFlags_DefaultOpen, [&]{
				auto workingText = formatNum(workingMachines.size(), "working machines");
				im::TreeNode(workingText.c_str(), [&]{
					printList(workingMachines, allMachines);
				});
				auto nonWorkingText = formatNum(nonWorkingMachines.size(), "non-working machines");
				int flags = nonWorkingMachines.empty() ? 0 : ImGuiTreeNodeFlags_DefaultOpen;
				im::TreeNode(nonWorkingText.c_str(), flags, [&]{
					printList(nonWorkingMachines, allMachines);
				});
			});
		});

		auto& allExtensions = manager.media->getAllExtensions();
		std::vector<size_t> workingExtensions, nonWorkingExtensions;
		for (auto [idx, info] : enumerate(allExtensions)) {
			if (!info.testResult.has_value()) {
				if (!doTest) continue; // only 1 test per iteration
				doTest = false;
			}
			const auto& result = manager.media->getTestResult(info);
			(result.empty() ? workingExtensions : nonWorkingExtensions).push_back(idx);
		}
		bool allExtensionsTested = allExtensions.size() == (workingExtensions.size() + nonWorkingExtensions.size());

		im::VisuallyDisabled(!allExtensionsTested, [&]{
			im::TreeNode("Extensions", ImGuiTreeNodeFlags_DefaultOpen, [&]{
				auto workingText = formatNum(workingExtensions.size(), "working extensions");
				im::TreeNode(workingText.c_str(), [&]{
					printList(workingExtensions, allExtensions);
				});
				auto nonWorkingText = formatNum(nonWorkingExtensions.size(), "non-working extensions");
				int flags = nonWorkingExtensions.empty() ? 0 : ImGuiTreeNodeFlags_DefaultOpen;
				im::TreeNode(nonWorkingText.c_str(), flags, [&]{
					printList(nonWorkingExtensions, allExtensions);
				});

			});
		});

		if (!nonWorkingMachines.empty() || !nonWorkingExtensions.empty()) {
			im::Disabled(!allMachinesTested || !allExtensionsTested, [&]{
				if (ImGui::Button("Copy list of non-working hardware to clipboard")) {
					std::string result;
					auto print = [&](std::string_view label, const auto& indices, const auto& infos) {
						if (!indices.empty()) {
							strAppend(result, "Non-working ", label, ":\n");
							for (auto idx : indices) {
								const auto& info = infos[idx];
								strAppend(result, '\t', info.displayName, " (", info.configName, ")\n",
										"\t\t", *info.testResult, '\n');
							}
							strAppend(result, '\n');
						}
					};
					print("machines", nonWorkingMachines, allMachines);
					print("extensions", nonWorkingExtensions, allExtensions);
					ImGui::SetClipboardText(result.c_str());
				}
			});
		}

		im::Disabled(!allMachinesTested || !allExtensionsTested, [&]{
			if (ImGui::Button("Rerun test")) {
				manager.media->resetExtensionInfo();
				machineInfo.clear();
			}
		});
		ImGui::Separator();
		// TODO: what to do if the folder doesn't exist? Should we go
		// one higher? (But then the button doesn't browse to where it
		// promises to browse to and that may be confusing, e.g. if
		// people put their system roms there...
		if (ImGui::Button("Open user system ROMs folder")) {
			SDL_OpenURL(strCat("file://", FileOperations::getUserDataDir(), "/systemroms").c_str());
		}
		if (ImGui::Button("Open system wide system ROMs folder")) {
			SDL_OpenURL(strCat("file://", FileOperations::getSystemDataDir(), "/systemroms").c_str());
		}
	});
}

std::vector<ImGuiMachine::MachineInfo>& ImGuiMachine::getAllMachines()
{
	if (machineInfo.empty()) {
		machineInfo = parseAllConfigFiles<MachineInfo>(manager, "machines", {"Manufacturer"sv, "Product code"sv});
	}
	return machineInfo;
}

static void amendConfigInfo(MSXMotherBoard& mb, ImGuiMachine::MachineInfo& info)
{
	auto& configInfo = info.configInfo;

	const auto& debugger = mb.getDebugger();
	unsigned ramSize = 0;
	for (const auto& [name, debuggable] : debugger.getDebuggables()) {
		if (debuggable->getDescription() == one_of("memory mapper", "ram")) {
			ramSize += debuggable->getSize();
		}
	}
	configInfo.emplace_back("RAM size", strCat(ramSize / 1024, "kB"));

	if (auto* vdp = dynamic_cast<VDP*>(mb.findDevice("VDP"))) {
		configInfo.emplace_back("VRAM size", strCat(vdp->getVRAM().getSize() / 1024, "kB"));
		configInfo.emplace_back("VDP version", vdp->getVersionString());
	}

	if (auto drives = RealDrive::getDrivesInUse(mb)) {
		configInfo.emplace_back("Disk drives", strCat(narrow<int>(drives->count())));
	}

	const auto& carts = mb.getSlotManager();
	configInfo.emplace_back("Cartridge slots", strCat(carts.getNumberOfSlots()));
}

const std::string& ImGuiMachine::getTestResult(MachineInfo& info)
{
	if (!info.testResult) {
		info.testResult.emplace(); // empty string (for now)

		auto& reactor = manager.getReactor();
		manager.executeDelayed([&reactor, &info]() mutable {
			// don't create extra mb while drawing
			try {
				MSXMotherBoard mb(reactor);
				mb.getMSXCliComm().setSuppressMessages(true);
				mb.loadMachine(info.configName);
				assert(info.testResult->empty());
				amendConfigInfo(mb, info);
			} catch (MSXException& e) {
				info.testResult = e.getMessage(); // error
			}
		});
	}
	return info.testResult.value();
}

bool ImGuiMachine::printConfigInfo(MachineInfo& info)
{
	const auto& test = getTestResult(info);
	bool ok = test.empty();
	if (ok) {
		im::Table("##machine-info", 2, ImGuiTableFlags_SizingFixedFit, [&]{
			float maxValueWidth = 0.0f;
			for (const auto& [desc, value] : info.configInfo) {
				maxValueWidth = std::max(maxValueWidth, ImGui::CalcTextSize(value).x);
			}
			auto width = std::min(ImGui::GetFontSize() * 20.0f, maxValueWidth);

			ImGui::TableSetupColumn("dummy");
			ImGui::TableSetupColumn("dummy", ImGuiTableColumnFlags_WidthFixed, width);

			for (const auto& [desc, value_] : info.configInfo) {
				const auto& value = value_; // clang workaround
				if (ImGui::TableNextColumn()) {
					ImGui::TextUnformatted(desc);
				}
				if (ImGui::TableNextColumn()) {
					im::TextWrapPos(0.0f, [&] {
						ImGui::TextUnformatted(value);
					});
				}
			}
		});
	} else {
		im::StyleColor(ImGuiCol_Text, getColor(imColor::ERROR), [&]{
			im::TextWrapPos(ImGui::GetFontSize() * 35.0f, [&] {
				ImGui::TextUnformatted(test);
			});
		});
	}
	return ok;
}

ImGuiMachine::MachineInfo* ImGuiMachine::findMachineInfo(std::string_view config)
{
	auto& allMachines = getAllMachines();
	auto it = std::ranges::find(allMachines, config, &MachineInfo::configName);
	if (it == allMachines.end()) {
		// perhaps something changed, let's refresh the cache and try again
		machineInfo.clear();
		allMachines = getAllMachines();
		it = std::ranges::find(allMachines, config, &MachineInfo::configName);
	}
	return (it != allMachines.end()) ? std::to_address(it) : nullptr;
}

} // namespace openmsx
