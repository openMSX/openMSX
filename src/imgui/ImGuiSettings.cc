#include "ImGuiSettings.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiMessages.hh"
#include "ImGuiOsdIcons.hh"
#include "ImGuiSoundChip.hh"
#include "ImGuiUtils.hh"

#include "BooleanInput.hh"
#include "BooleanSetting.hh"
#include "CPUCore.hh"
#include "Display.hh"
#include "EventDistributor.hh"
#include "FileContext.hh"
#include "FilenameSetting.hh"
#include "FloatSetting.hh"
#include "GlobalCommandController.hh"
#include "GlobalSettings.hh"
#include "InputEventFactory.hh"
#include "InputEventGenerator.hh"
#include "IntegerSetting.hh"
#include "JoyMega.hh"
#include "JoyHandle.hh"
#include "KeyCodeSetting.hh"
#include "KeyboardSettings.hh"
#include "MSXCPU.hh"
#include "MSXCommandController.hh"
#include "MSXJoystick.hh"
#include "MSXMotherBoard.hh"
#include "Mixer.hh"
#include "Plotter.hh"
#include "PluggingController.hh"
#include "ProxySetting.hh"
#include "R800.hh"
#include "Reactor.hh"
#include "ReadOnlySetting.hh"
#include "SettingsManager.hh"
#include "StringSetting.hh"
#include "Version.hh"
#include "VideoSourceSetting.hh"
#include "Z80.hh"

#include "StringOp.hh"
#include "checked_cast.hh"
#include "foreach_file.hh"
#include "narrow.hh"
#include "zstring_view.hh"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <SDL.h>

#include <algorithm>
#include <optional>
#include <utility>

using namespace std::literals;

namespace openmsx {

ImGuiSettings::ImGuiSettings(ImGuiManager& manager_)
	: ImGuiPart(manager_)
	, saveLayout("layout", ".ini", "layouts")
	, loadLayout("layout", ".ini", "layouts")
	, confirmOverwrite("Confirm##overwrite-layout")
{
	saveLayout.drawAction = [&]{
		saveLayout.drawTable();
		ImGui::TextUnformatted("Enter name:"sv);
		ImGui::InputText("##save-layout-name", &saveLayoutName);
		ImGui::SameLine();
		im::Disabled(saveLayoutName.empty(), [&]{
			if (ImGui::Button("Save as")) {
				(void)manager.getReactor().getDisplay().getWindowPosition(); // to save up-to-date window position

				auto filename = FileOperations::parseCommandFileArgument(
					saveLayoutName, "layouts", "", ".ini");
				auto action = [filename]{
					ImGui::SaveIniSettingsToDisk(filename.c_str());
					ImGui::CloseCurrentPopup();
				};
				if (FileOperations::exists(filename)) {
					confirmOverwrite.open(
						strCat("Overwrite layout: ", saveLayoutName),
						action);
				} else {
					action();
				}
			}
		});
		confirmOverwrite.execute();
	};
	saveLayout.singleClickAction = [&](const FileListWidget::Entry& entry) {
		saveLayoutName = entry.getDefaultDisplayName();
	};

	loadLayout.singleClickAction = [&](const FileListWidget::Entry& entry) {
		manager.loadIniFile = entry.fullName;
		ImGui::CloseCurrentPopup();
	};
}

ImGuiSettings::~ImGuiSettings()
{
	deinitListener();
}

void ImGuiSettings::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiSettings::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

void ImGuiSettings::loadEnd()
{
	setStyle();
}

void ImGuiSettings::setStyle() const
{
	switch (selectedStyle) {
	case 0: ImGui::StyleColorsDark();    break;
	case 1: ImGui::StyleColorsLight();   break;
	case 2: ImGui::StyleColorsClassic(); break;
	case 3: ImGui::StyleColorsClassic(); break; // colorblind adjustments
	}
	setColors(selectedStyle);
}

// Returns the currently pressed key-chord, or 'ImGuiKey_None' if no
// (non-modifier) key is pressed.
// If more than (non-modifier) one key is pressed, this returns an arbitrary key
// (in the current implementation the one with lowest index).
[[nodiscard]] static ImGuiKeyChord getCurrentlyPressedKeyChord()
{
	static constexpr auto mods = std::array{
		ImGuiKey_LeftCtrl, ImGuiKey_LeftShift, ImGuiKey_LeftAlt, ImGuiKey_LeftSuper,
		ImGuiKey_RightCtrl, ImGuiKey_RightShift, ImGuiKey_RightAlt, ImGuiKey_RightSuper,
		ImGuiKey_ReservedForModCtrl, ImGuiKey_ReservedForModShift, ImGuiKey_ReservedForModAlt,
		ImGuiKey_ReservedForModSuper, ImGuiKey_MouseLeft, ImGuiKey_MouseRight, ImGuiKey_MouseMiddle,
		ImGuiKey_MouseX1, ImGuiKey_MouseX2, ImGuiKey_MouseWheelX, ImGuiKey_MouseWheelY,
	};
	for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key) {
		// This is O(M*N), if needed could be optimized to be O(M+N).
		if (contains(mods, key)) continue; // skip: mods can't be primary keys in a KeyChord
		if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key))) {
			const ImGuiIO& io = ImGui::GetIO();
			return key
			     | (io.KeyCtrl  ? ImGuiMod_Ctrl  : 0)
			     | (io.KeyShift ? ImGuiMod_Shift : 0)
			     | (io.KeyAlt   ? ImGuiMod_Alt   : 0)
			     | (io.KeySuper ? ImGuiMod_Super : 0);
		}
	}
	return ImGuiKey_None;
}

void ImGuiSettings::showMenu(MSXMotherBoard* motherBoard)
{
	im::Menu("Settings", [&]{
		auto& reactor = manager.getReactor();
		auto& globalSettings = reactor.getGlobalSettings();
		auto& renderSettings = reactor.getDisplay().getRenderSettings();
		const auto& settingsManager = reactor.getGlobalCommandController().getSettingsManager();
		const auto& hotKey = reactor.getHotKey();

		im::Menu("Video", [&]{
			im::TreeNode("Look and feel", ImGuiTreeNodeFlags_DefaultOpen, [&]{
				auto& scaler = renderSettings.getScaleAlgorithmSetting();
				ComboBox("Scaler", scaler);
				im::Indent([&]{
					struct AlgoEnable {
						RenderSettings::ScaleAlgorithm algo;
						bool hasScanline;
						bool hasBlur;
					};
					using enum RenderSettings::ScaleAlgorithm;
					static constexpr std::array algoEnables = {
						AlgoEnable{.algo = SIMPLE,     .hasScanline = true,  .hasBlur = true },
						AlgoEnable{.algo = SCALE,      .hasScanline = false, .hasBlur = false},
						AlgoEnable{.algo = HQ,         .hasScanline = false, .hasBlur = false},
						AlgoEnable{.algo = RGBTRIPLET, .hasScanline = true,  .hasBlur = true },
						AlgoEnable{.algo = TV,         .hasScanline = true,  .hasBlur = false},
					};
					auto it = std::ranges::find(algoEnables, scaler.getEnum(), &AlgoEnable::algo);
					assert(it != algoEnables.end());
					im::Disabled(!it->hasScanline, [&]{
						SliderInt("Scanline (%)", renderSettings.getScanlineSetting());
					});
					im::Disabled(!it->hasBlur, [&]{
						SliderInt("Blur (%)", renderSettings.getBlurSetting());
					});
				});

				SliderInt("Scale factor", renderSettings.getScaleFactorSetting());
				Checkbox(hotKey, "Deinterlace", renderSettings.getDeinterlaceSetting());
				Checkbox(hotKey, "Deflicker", renderSettings.getDeflickerSetting());
			});
			im::TreeNode("Colors", ImGuiTreeNodeFlags_DefaultOpen, [&]{
				SliderFloat("Noise (%)", renderSettings.getNoiseSetting());
				SliderFloat("Brightness", renderSettings.getBrightnessSetting());
				SliderFloat("Contrast", renderSettings.getContrastSetting());
				SliderFloat("Gamma", renderSettings.getGammaSetting());
				SliderInt("Glow (%)", renderSettings.getGlowSetting());
				if (auto* monitor = dynamic_cast<Setting*>(settingsManager.findSetting("monitor_type"))) {
					ComboBox("Monitor type", *monitor, [](std::string s) {
						std::ranges::replace(s, '_', ' ');
						return s;
					});
				}
			});
			im::TreeNode("Shape", ImGuiTreeNodeFlags_DefaultOpen, [&]{
				Checkbox(hotKey, "Full Stretch", renderSettings.getFullStretchSetting());
				SliderFloat("Horizontal stretch", renderSettings.getHorizontalStretchSetting(), "%.0f");
				ComboBox("Display deformation", renderSettings.getDisplayDeformSetting());
			});
			im::TreeNode("Misc", ImGuiTreeNodeFlags_DefaultOpen, [&]{
				Checkbox(hotKey, "Full screen", renderSettings.getFullScreenSetting());
				if (motherBoard) {
					ComboBox("Video source to display", motherBoard->getVideoSource());
				}
				Checkbox(hotKey, "VSync", renderSettings.getVSyncSetting());
				SliderInt("Minimum frame-skip", renderSettings.getMinFrameSkipSetting()); // TODO: either leave out this setting, or add a tooltip like, "Leave on 0 unless you use a very slow device and want regular frame skipping");
				SliderInt("Maximum frame-skip", renderSettings.getMaxFrameSkipSetting()); // TODO: either leave out this setting or add a tooltip like  "On slow devices, skip no more than this amount of frames to keep emulation on time.");
			});
			im::TreeNode("Advanced (for debugging)", [&]{ // default collapsed
				Checkbox(hotKey, "Enforce VDP sprites-per-line limit", renderSettings.getLimitSpritesSetting());
				Checkbox(hotKey, "Disable sprites", renderSettings.getDisableSpritesSetting());
				ComboBox("Way to handle too fast VDP access", renderSettings.getTooFastAccessSetting());
				ComboBox("Emulate VDP command timing", renderSettings.getCmdTimingSetting());
				ComboBox("Rendering accuracy", renderSettings.getAccuracySetting());
			});
		});
		im::Menu("Sound", [&]{
			auto& mixer = reactor.getMixer();
			auto& muteSetting = mixer.getMuteSetting();
			im::Disabled(muteSetting.getBoolean(), [&]{
				SliderInt("Master volume", mixer.getMasterVolume());
			});
			Checkbox(hotKey, "Mute", muteSetting);
			ImGui::Separator();
			static constexpr std::array resamplerToolTips = {
				EnumToolTip{.value = "hq",   .tip = "best quality, uses more CPU"},
				EnumToolTip{.value = "blip", .tip = "good speed/quality tradeoff"},
				EnumToolTip{.value = "fast", .tip = "fast but low quality"},
			};
			ComboBox("Resampler", globalSettings.getResampleSetting(), resamplerToolTips);
			ImGui::Separator();

			ImGui::MenuItem("Show sound chip settings", nullptr, &manager.soundChip->showSoundChipSettings);
		});
		im::Menu("Speed", [&]{
			im::TreeNode("Emulation", ImGuiTreeNodeFlags_DefaultOpen, [&]{
				ImGui::SameLine();
				HelpMarker("These control the speed of the whole MSX machine, "
				           "the running MSX software can't tell the difference.");

				auto& speedManager = globalSettings.getSpeedManager();
				auto& fwdSetting = speedManager.getFastForwardSetting();
				int fastForward = fwdSetting.getBoolean() ? 1 : 0;
				ImGui::TextUnformatted("Speed:"sv);
				ImGui::SameLine();
				bool fwdChanged = ImGui::RadioButton("normal", &fastForward, 0);
				ImGui::SameLine();
				fwdChanged |= ImGui::RadioButton("fast forward", &fastForward, 1);
				if (auto fastForwardShortCut = getShortCutForCommand(reactor.getHotKey(), "toggle fastforward");
				    !fastForwardShortCut.empty()) {
					HelpMarker(strCat("Use '", fastForwardShortCut ,"' to quickly toggle between these two"));
				}
				if (fwdChanged) {
					fwdSetting.setBoolean(fastForward != 0);
				}
				im::Indent([&]{
					im::Disabled(fastForward != 0, [&]{
						SliderFloat("Speed (%)", speedManager.getSpeedSetting(), "%.1f", ImGuiSliderFlags_Logarithmic);
					});
					im::Disabled(fastForward != 1, [&]{
						SliderFloat("Fast forward speed (%)", speedManager.getFastForwardSpeedSetting(), "%.1f", ImGuiSliderFlags_Logarithmic);
					});
				});
				Checkbox(hotKey, "Go full speed when loading", globalSettings.getThrottleManager().getFullSpeedLoadingSetting());
			});
			if (motherBoard) {
				im::TreeNode("MSX devices", ImGuiTreeNodeFlags_DefaultOpen, [&]{
					ImGui::SameLine();
					HelpMarker("These control the speed of the specific components in the MSX machine. "
						"So the relative speed between components can change. "
						"And this may lead the emulation problems.");

					MSXCPU& cpu = motherBoard->getCPU();
					auto showFreqSettings = [&](std::string_view name, auto* core) {
						if (!core) return;
						auto& locked = core->getFreqLockedSetting();
						auto& value = core->getFreqValueSetting();
						// Note: GUI shows "UNlocked", while the actual settings is "locked"
						bool unlocked = !locked.getBoolean();
						if (ImGui::Checkbox(tmpStrCat("unlock custom ", name, " frequency").c_str(), &unlocked)) {
							locked.setBoolean(!unlocked);
						}
						simpleToolTip([&]{ return locked.getDescription(); });
						im::Indent([&]{
							im::Disabled(!unlocked, [&]{
								float fval = float(value.getInt()) / 1.0e6f;
								if (ImGui::InputFloat(tmpStrCat("frequency (MHz)##", name).c_str(), &fval, 0.01f,   1.0f, "%.2f")) {
									value.setInt(int(fval * 1.0e6f));
								}
								im::PopupContextItem(tmpStrCat("freq-context##", name).c_str(), [&]{
									const char* F358 = name == "Z80" ? "3.58 MHz (default)"
									                                 : "3.58 MHz";
									if (ImGui::Selectable(F358)) {
										value.setInt(3'579'545);
									}
									if (ImGui::Selectable("5.37 MHz")) {
										value.setInt(5'369'318);
									}
									const char* F716 = name == "R800" ? "7.16 MHz (default)"
									                                  : "7.16 MHz";
									if (ImGui::Selectable(F716)) {
										value.setInt(7'159'090);
									}

								});
								HelpMarker("Right-click to select commonly used values");
							});
						});
					};
					showFreqSettings("Z80",  cpu.getZ80());
					showFreqSettings("R800", cpu.getR800()); // might be nullptr
				});
			}
		});
		im::Menu("Input", [&]{
			static constexpr std::array kbdModeToolTips = {
				EnumToolTip{.value = "CHARACTER",  .tip = "Tries to understand the character you are typing and then attempts to type that character using the current MSX keyboard. May not work very well when using a non-US host keyboard."},
				EnumToolTip{.value = "KEY",        .tip = "Tries to map a key you press to the corresponding MSX key"},
				EnumToolTip{.value = "POSITIONAL", .tip = "Tries to map the keyboard key positions to the MSX keyboard key positions"},
			};
			if (motherBoard) {
				const auto& controller = motherBoard->getMSXCommandController();
				if (auto* turbo = dynamic_cast<IntegerSetting*>(controller.findSetting("renshaturbo"))) {
					SliderInt("Ren Sha Turbo (%)", *turbo);
				}
				if (auto* mappingModeSetting = dynamic_cast<EnumSetting<KeyboardSettings::MappingMode>*>(controller.findSetting("kbd_mapping_mode"))) {
					ComboBox("Keyboard mapping mode", *mappingModeSetting, kbdModeToolTips);
				}
			}
			ImGui::MenuItem("Configure MSX joysticks", nullptr, &showConfigureJoystick);
			auto& grabInputSetting = reactor.getInputEventGenerator().getGrabInput();
			bool grabInput = grabInputSetting.getBoolean();
			if (auto shortCut = getShortCutForCommand(hotKey, "toggle grabinput");
				ImGui::MenuItem("Grab input", shortCut.c_str(), &grabInput)) {
				grabInputSetting.setBoolean(grabInput);
			}
			simpleToolTip("Enable this to help you keep the mouse cursor in the MSX window, e.g. when using drawing programs with mouse in windowed mode.");
		});
		im::Menu("GUI", [&]{
			saveLayout.menu("Save layout");
			loadLayout.menu("Restore layout");
			ImGui::Separator();
			im::Menu("Select style", [&]{
				std::optional<int> newStyle;
				static constexpr std::array names = {"Dark", "Light", "Classic", "Colorblind"}; // must be in sync with setStyle()
				for (auto i : xrange(narrow<int>(names.size()))) {
					if (ImGui::Selectable(names[i], selectedStyle == i)) {
						newStyle = i;
					}
				}
				if (newStyle) {
					selectedStyle = *newStyle;
					setStyle();
				}
			});
			ImGui::MenuItem("Select font", nullptr, &showFont);
			ImGui::MenuItem("Edit shortcuts", nullptr, &showShortcut);
			ImGui::MenuItem("Configure OSD icons", nullptr, &manager.osdIcons->showConfigureIcons);
			ImGui::MenuItem("Fade out menu bar", nullptr, &manager.menuFade);
			im::Menu("Status bar", [&]{
				ImGui::Checkbox("Show", &manager.statusBarVisible);
				im::DisabledIndent(!manager.statusBarVisible, [&]{
					manager.configStatusBarVisibilityItems();
				});
			});
			ImGui::MenuItem("Configure messages", nullptr, &manager.messages->configureWindow.open);
		});
		im::Menu("MSX Plotter", motherBoard != nullptr, [&]{
			// Find the MSXPlotter in the pluggables
			MSXPlotter* plotter = nullptr;
			for (const auto& plug : motherBoard->getPluggingController().getPluggables()) {
				if (auto* p = dynamic_cast<MSXPlotter*>(plug.get())) {
					plotter = p;
					break;
				}
			}

			// Character Set dropdown
			static constexpr std::array charSetNames = { // must be in sync with Plotter
				"International", "Japanese", "DIN (German)"
			};
			auto currentCharSet = int(plotter->getCharacterSet());
			if (ImGui::Combo("Character set", &currentCharSet, charSetNames.data(), int(charSetNames.size()))) {
				plotter->setCharacterSet(static_cast<MSXPlotter::CharacterSet>(currentCharSet));
			}

			// Dipswitch 4
			bool dipSwitch4 = plotter->getDipSwitch4();
			if (ImGui::Checkbox("Dipswitch 4", &dipSwitch4)) {
				plotter->setDipSwitch4(dipSwitch4);
			}

			// Pen thickness
			static constexpr std::array thicknessNames = {
				"Standard (PRK-C41)", "Thick (PRK-C42)"
			};
			auto currentThickness = int(plotter->getPenThicknessSetting().getEnum());
			if (ImGui::Combo("Pen thickness", &currentThickness, thicknessNames.data(), int(thicknessNames.size()))) {
				plotter->getPenThicknessSetting().setEnum(static_cast<MSXPlotter::PenThickness>(currentThickness));
			}
		});
		ImGui::Separator();
		im::Menu("Advanced", [&]{
			ImGui::TextUnformatted("All settings"sv);
			ImGui::Separator();
			std::vector<Setting*> settings;
			for (auto* setting : settingsManager.getAllSettings()) {
				if (dynamic_cast<ProxySetting*>(setting)) continue;
				if (dynamic_cast<ReadOnlySetting*>(setting)) continue;
				settings.push_back(checked_cast<Setting*>(setting));
			}
			std::ranges::sort(settings, StringOp::caseless{}, &Setting::getBaseName);
			for (auto* setting : settings) {
				if (auto* bSetting = dynamic_cast<BooleanSetting*>(setting)) {
					Checkbox(hotKey, *bSetting);
				} else if (auto* iSetting = dynamic_cast<IntegerSetting*>(setting)) {
					SliderInt(*iSetting);
				} else if (auto* fSetting = dynamic_cast<FloatSetting*>(setting)) {
					SliderFloat(*fSetting);
				} else if (auto* sSetting = dynamic_cast<StringSetting*>(setting)) {
					InputText(*sSetting);
				} else if (auto* fnSetting = dynamic_cast<FilenameSetting*>(setting)) {
					InputText(*fnSetting); // TODO
				} else if (auto* kSetting = dynamic_cast<KeyCodeSetting*>(setting)) {
					InputText(*kSetting); // TODO
				} else if (dynamic_cast<EnumSettingBase*>(setting)) {
					ComboBox(*setting);
				} else if (auto* vSetting = dynamic_cast<VideoSourceSetting*>(setting)) {
					ComboBox(*vSetting);
				} else {
					assert(false);
				}
			}
		});
	});
}

////// joystick stuff

// joystick is 0..3
[[nodiscard]] static std::string settingName(unsigned joystick)
{
	return (joystick < 2) ? strCat("msxjoystick", joystick + 1, "_config")
	     : (joystick < 4) ? strCat("joymega",     joystick - 1, "_config")
						  : strCat("joyhandle",   joystick - 3, "_config");
}

// joystick is 0..3
[[nodiscard]] static std::string joystickToGuiString(unsigned joystick)
{
	return (joystick < 2) ? strCat("MSX joystick ",       joystick + 1)
		 : (joystick < 4) ? strCat("JoyMega controller ", joystick - 1)
	                      : strCat("Panasonic FS-JH1 ",   joystick - 3);
}

[[nodiscard]] static std::string toGuiString(const BooleanInput& input, const JoystickManager& joystickManager)
{
	return std::visit(overloaded{
		[](const BooleanKeyboard& k) {
			return strCat("keyboard key ", SDLKey::toString(k.getKeyCode()));
		},
		[](const BooleanMouseButton& m) {
			return strCat("mouse button ", m.getButton());
		},
		[&](const BooleanJoystickButton& j) {
			return strCat(joystickManager.getDisplayName(j.getJoystick()), " button ", j.getButton());
		},
		[&](const BooleanJoystickHat& h) {
			return strCat(joystickManager.getDisplayName(h.getJoystick()), " D-pad ", h.getHat(), ' ', toString(h.getValue()));
		},
		[&](const BooleanJoystickAxis& a) {
			return strCat(joystickManager.getDisplayName(a.getJoystick()),
			              " stick axis ", a.getAxis(), ", ",
			              (a.getDirection() == BooleanJoystickAxis::Direction::POS ? "positive"sv : "negative"sv), " direction");
		}
	}, input);
}

[[nodiscard]] static bool insideCircle(gl::vec2 mouse, gl::vec2 center, float radius)
{
	auto delta = center - mouse;
	return gl::sum(delta * delta) <= (radius * radius);
}
[[nodiscard]] static bool between(float x, float min, float max)
{
	return (min <= x) && (x <= max);
}

namespace {
struct Rectangle {
	gl::vec2 topLeft;
	gl::vec2 bottomRight;
};
}
[[nodiscard]] static bool insideRectangle(gl::vec2 mouse, Rectangle r)
{
	return between(mouse.x, r.topLeft.x, r.bottomRight.x) &&
	       between(mouse.y, r.topLeft.y, r.bottomRight.y);
}


static constexpr auto fractionDPad = 1.0f / 3.0f;
static constexpr auto thickness = 3.0f;

static void drawDPad(gl::vec2 center, float size, std::span<const uint8_t, 4> hovered, int hoveredRow)
{
	const auto F = fractionDPad;
	std::array<std::array<ImVec2, 5 + 1>, 4> points = {
		std::array<ImVec2, 5 + 1>{ // UP
			center + size * gl::vec2{ 0,  0},
			center + size * gl::vec2{-F, -F},
			center + size * gl::vec2{-F, -1},
			center + size * gl::vec2{ F, -1},
			center + size * gl::vec2{ F, -F},
			center + size * gl::vec2{ 0,  0},
		},
		std::array<ImVec2, 5 + 1>{ // DOWN
			center + size * gl::vec2{ 0,  0},
			center + size * gl::vec2{ F,  F},
			center + size * gl::vec2{ F,  1},
			center + size * gl::vec2{-F,  1},
			center + size * gl::vec2{-F,  F},
			center + size * gl::vec2{ 0,  0},
		},
		std::array<ImVec2, 5 + 1>{ // LEFT
			center + size * gl::vec2{ 0,  0},
			center + size * gl::vec2{-F,  F},
			center + size * gl::vec2{-1,  F},
			center + size * gl::vec2{-1, -F},
			center + size * gl::vec2{-F, -F},
			center + size * gl::vec2{ 0,  0},
		},
		std::array<ImVec2, 5 + 1>{ // RIGHT
			center + size * gl::vec2{ 0,  0},
			center + size * gl::vec2{ F, -F},
			center + size * gl::vec2{ 1, -F},
			center + size * gl::vec2{ 1,  F},
			center + size * gl::vec2{ F,  F},
			center + size * gl::vec2{ 0,  0},
		},
	};

	auto* drawList = ImGui::GetWindowDrawList();
	auto hoverColor = ImGui::GetColorU32(ImGuiCol_ButtonHovered);

	auto color = getColor(imColor::TEXT);
	for (auto i : xrange(4)) {
		if (hovered[i] || (hoveredRow == i)) {
			drawList->AddConvexPolyFilled(points[i].data(), 5, hoverColor);
		}
		drawList->AddPolyline(points[i].data(), 5 + 1, color, thickness);
	}
}

static void drawFilledCircle(gl::vec2 center, float radius, bool fill)
{
	auto* drawList = ImGui::GetWindowDrawList();
	if (fill) {
		auto hoverColor = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
		drawList->AddCircleFilled(center, radius, hoverColor);
	}
	auto color = getColor(imColor::TEXT);
	drawList->AddCircle(center, radius, color, 0, thickness);
}
static void drawFilledRectangle(Rectangle r, float corner, bool fill)
{
	auto* drawList = ImGui::GetWindowDrawList();
	if (fill) {
		auto hoverColor = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
		drawList->AddRectFilled(r.topLeft, r.bottomRight, hoverColor, corner);
	}
	auto color = getColor(imColor::TEXT);
	drawList->AddRect(r.topLeft, r.bottomRight, color, corner, thickness);
}

static void drawLetterA(gl::vec2 center)
{
	auto* drawList = ImGui::GetWindowDrawList();
	auto tr = [&](gl::vec2 p) { return center + p; };
	const std::array<ImVec2, 3> lines = { tr({-6, 7}), tr({0, -7}), tr({6, 7}) };
	auto color = getColor(imColor::TEXT);
	drawList->AddPolyline(lines.data(), narrow<int>(lines.size()), color, thickness);
	drawList->AddLine(tr({-3, 1}), tr({3, 1}), color, thickness);
}
static void drawLetterB(gl::vec2 center)
{
	auto* drawList = ImGui::GetWindowDrawList();
	auto tr = [&](gl::vec2 p) { return center + p; };
	const std::array<ImVec2, 4> lines = { tr({1, -7}), tr({-4, -7}), tr({-4, 7}), tr({2, 7}) };
	auto color = getColor(imColor::TEXT);
	drawList->AddPolyline(lines.data(), narrow<int>(lines.size()), color, thickness);
	drawList->AddLine(tr({-4, -1}), tr({2, -1}), color, thickness);
	drawList->AddBezierQuadratic(tr({1, -7}), tr({4, -7}), tr({4, -4}), color, thickness);
	drawList->AddBezierQuadratic(tr({4, -4}), tr({4, -1}), tr({1, -1}), color, thickness);
	drawList->AddBezierQuadratic(tr({2, -1}), tr({6, -1}), tr({6,  3}), color, thickness);
	drawList->AddBezierQuadratic(tr({6,  3}), tr({6,  7}), tr({2,  7}), color, thickness);
}
static void drawLetterC(gl::vec2 center)
{
	auto* drawList = ImGui::GetWindowDrawList();
	auto tr = [&](gl::vec2 p) { return center + p; };
	auto color = getColor(imColor::TEXT);
	drawList->AddBezierCubic(tr({5, -5}), tr({-8, -16}), tr({-8, 16}), tr({5, 5}), color, thickness);
}
static void drawLetterX(gl::vec2 center)
{
	auto* drawList = ImGui::GetWindowDrawList();
	auto tr = [&](gl::vec2 p) { return center + p; };
	auto color = getColor(imColor::TEXT);
	drawList->AddLine(tr({-4, -6}), tr({4,  6}), color, thickness);
	drawList->AddLine(tr({-4,  6}), tr({4, -6}), color, thickness);
}
static void drawLetterY(gl::vec2 center)
{
	auto* drawList = ImGui::GetWindowDrawList();
	auto tr = [&](gl::vec2 p) { return center + p; };
	auto color = getColor(imColor::TEXT);
	drawList->AddLine(tr({-4, -6}), tr({0,  0}), color, thickness);
	drawList->AddLine(tr({-4,  6}), tr({4, -6}), color, thickness);
}
static void drawLetterZ(gl::vec2 center)
{
	auto* drawList = ImGui::GetWindowDrawList();
	auto tr = [&](gl::vec2 p) { return center + p; };
	const std::array<ImVec2, 4> linesZ2 = { tr({-4, -6}), tr({4, -6}), tr({-4, 6}), tr({4, 6}) };
	auto color = getColor(imColor::TEXT);
	drawList->AddPolyline(linesZ2.data(), 4, color, thickness);
}

namespace msxjoystick {

enum : uint8_t {UP, DOWN, LEFT, RIGHT, TRIG_A, TRIG_B, NUM_BUTTONS};

static constexpr std::array<zstring_view, NUM_BUTTONS> buttonNames = {
	"Up", "Down", "Left", "Right", "A", "B" // show in the GUI
};
static constexpr std::array<zstring_view, NUM_BUTTONS> keyNames = {
	"UP", "DOWN", "LEFT", "RIGHT", "A", "B" // keys in Tcl dict
};

// Customize joystick look
static constexpr auto boundingBox = gl::vec2{300.0f, 100.0f};
static constexpr auto radius = 20.0f;
static constexpr auto corner = 10.0f;
static constexpr auto centerA = gl::vec2{200.0f,  50.0f};
static constexpr auto centerB = gl::vec2{260.0f,  50.0f};
static constexpr auto centerDPad = gl::vec2{50.0f,  50.0f};
static constexpr auto sizeDPad = 30.0f;

[[nodiscard]] static std::vector<uint8_t> buttonsHovered(gl::vec2 mouse)
{
	std::vector<uint8_t> result(NUM_BUTTONS); // false
	auto mouseDPad = (mouse - centerDPad) * (1.0f / sizeDPad);
	if (insideRectangle(mouseDPad, Rectangle{{-1, -1}, {1, 1}}) &&
		(between(mouseDPad.x, -fractionDPad, fractionDPad) ||
		between(mouseDPad.y, -fractionDPad, fractionDPad))) { // mouse over d-pad
		bool t1 = mouseDPad.x <  mouseDPad.y;
		bool t2 = mouseDPad.x < -mouseDPad.y;
		result[UP]    = !t1 &&  t2;
		result[DOWN]  =  t1 && !t2;
		result[LEFT]  =  t1 &&  t2;
		result[RIGHT] = !t1 && !t2;
	}
	result[TRIG_A] = insideCircle(mouse, centerA, radius);
	result[TRIG_B] = insideCircle(mouse, centerB, radius);
	return result;
}

static void draw(gl::vec2 scrnPos, std::span<uint8_t> hovered, int hoveredRow)
{
	auto* drawList = ImGui::GetWindowDrawList();

	auto color = getColor(imColor::TEXT);
	drawList->AddRect(scrnPos, scrnPos + boundingBox, color, corner, thickness);

	drawDPad(scrnPos + centerDPad, sizeDPad, subspan<4>(hovered), hoveredRow);

	auto scrnCenterA = scrnPos + centerA;
	drawFilledCircle(scrnCenterA, radius, hovered[TRIG_A] || (hoveredRow == TRIG_A));
	drawLetterA(scrnCenterA);

	auto scrnCenterB = scrnPos + centerB;
	drawFilledCircle(scrnCenterB, radius, hovered[TRIG_B] || (hoveredRow == TRIG_B));
	drawLetterB(scrnCenterB);
}

} // namespace msxjoystick

namespace joymega {

enum : uint8_t {UP, DOWN, LEFT, RIGHT,
                TRIG_A, TRIG_B, TRIG_C,
                TRIG_X, TRIG_Y, TRIG_Z,
                TRIG_SELECT, TRIG_START,
                NUM_BUTTONS};

static constexpr std::array<zstring_view, NUM_BUTTONS> buttonNames = { // show in the GUI
	"Up", "Down", "Left", "Right",
	"A", "B", "C",
	"X", "Y", "Z",
	"Select", "Start",
};
static constexpr std::array<zstring_view, NUM_BUTTONS> keyNames = { // keys in Tcl dict
	"UP", "DOWN", "LEFT", "RIGHT",
	"A", "B", "C",
	"X", "Y", "Z",
	"SELECT", "START",
};

// Customize joystick look
static constexpr auto thickness = 3.0f;
static constexpr auto boundingBox = gl::vec2{300.0f, 158.0f};
static constexpr auto centerA = gl::vec2{205.0f, 109.9f};
static constexpr auto centerB = gl::vec2{235.9f,  93.5f};
static constexpr auto centerC = gl::vec2{269.7f,  83.9f};
static constexpr auto centerX = gl::vec2{194.8f,  75.2f};
static constexpr auto centerY = gl::vec2{223.0f,  61.3f};
static constexpr auto centerZ = gl::vec2{252.2f,  52.9f};
static constexpr auto selectBox = Rectangle{gl::vec2{130.0f,  60.0f}, gl::vec2{160.0f,  70.0f}};
static constexpr auto startBox  = Rectangle{gl::vec2{130.0f,  86.0f}, gl::vec2{160.0f,  96.0f}};
static constexpr auto radiusABC = 16.2f;
static constexpr auto radiusXYZ = 12.2f;
static constexpr auto centerDPad = gl::vec2{65.6f,  82.7f};
static constexpr auto sizeDPad = 34.0f;
static constexpr auto fractionDPad = 1.0f / 3.0f;

[[nodiscard]] static std::vector<uint8_t> buttonsHovered(gl::vec2 mouse)
{
	std::vector<uint8_t> result(NUM_BUTTONS); // false
	auto mouseDPad = (mouse - centerDPad) * (1.0f / sizeDPad);
	if (insideRectangle(mouseDPad, Rectangle{{-1, -1}, {1, 1}}) &&
		(between(mouseDPad.x, -fractionDPad, fractionDPad) ||
		between(mouseDPad.y, -fractionDPad, fractionDPad))) { // mouse over d-pad
		bool t1 = mouseDPad.x <  mouseDPad.y;
		bool t2 = mouseDPad.x < -mouseDPad.y;
		result[UP]    = !t1 &&  t2;
		result[DOWN]  =  t1 && !t2;
		result[LEFT]  =  t1 &&  t2;
		result[RIGHT] = !t1 && !t2;
	}
	result[TRIG_A] = insideCircle(mouse, centerA, radiusABC);
	result[TRIG_B] = insideCircle(mouse, centerB, radiusABC);
	result[TRIG_C] = insideCircle(mouse, centerC, radiusABC);
	result[TRIG_X] = insideCircle(mouse, centerX, radiusXYZ);
	result[TRIG_Y] = insideCircle(mouse, centerY, radiusXYZ);
	result[TRIG_Z] = insideCircle(mouse, centerZ, radiusXYZ);
	result[TRIG_START]  = insideRectangle(mouse, startBox);
	result[TRIG_SELECT] = insideRectangle(mouse, selectBox);
	return result;
}

static void draw(gl::vec2 scrnPos, std::span<uint8_t> hovered, int hoveredRow)
{
	auto* drawList = ImGui::GetWindowDrawList();
	auto tr = [&](gl::vec2 p) { return scrnPos + p; };
	auto color = getColor(imColor::TEXT);

	auto drawBezierCurve = [&](std::span<const gl::vec2> points, float thick = 1.0f) {
		assert((points.size() % 2) == 0);
		for (size_t i = 0; i < points.size() - 2; i += 2) {
			auto ap = points[i + 0];
			auto ad = points[i + 1];
			auto bp = points[i + 2];
			auto bd = points[i + 3];
			drawList->AddBezierCubic(tr(ap), tr(ap + ad), tr(bp - bd), tr(bp), color, thick);
		}
	};

	std::array outLine = {
		gl::vec2{150.0f,   0.0f}, gl::vec2{ 23.1f,   0.0f},
		gl::vec2{258.3f,  30.3f}, gl::vec2{ 36.3f,  26.4f},
		gl::vec2{300.0f, 107.0f}, gl::vec2{  0.0f,  13.2f},
		gl::vec2{285.2f, 145.1f}, gl::vec2{ -9.9f,   9.9f},
		gl::vec2{255.3f, 157.4f}, gl::vec2{ -9.0f,   0.0f},
		gl::vec2{206.0f, 141.8f}, gl::vec2{-16.2f,  -5.6f},
		gl::vec2{150.0f, 131.9f}, gl::vec2{-16.5f,   0.0f},
		gl::vec2{ 94.0f, 141.8f}, gl::vec2{-16.2f,   5.6f},
		gl::vec2{ 44.7f, 157.4f}, gl::vec2{ -9.0f,   0.0f},
		gl::vec2{ 14.8f, 145.1f}, gl::vec2{ -9.9f,  -9.9f},
		gl::vec2{  0.0f, 107.0f}, gl::vec2{  0.0f, -13.2f},
		gl::vec2{ 41.7f,  30.3f}, gl::vec2{ 36.3f, -26.4f},
		gl::vec2{150.0f,   0.0f}, gl::vec2{ 23.1f,   0.0f}, // closed loop
	};
	drawBezierCurve(outLine, thickness);

	drawDPad(tr(centerDPad), sizeDPad, subspan<4>(hovered), hoveredRow);
	drawList->AddCircle(tr(centerDPad), 43.0f, color);
	std::array dPadCurve = {
		gl::vec2{77.0f,  33.0f}, gl::vec2{ 69.2f,   0.0f},
		gl::vec2{54.8f, 135.2f}, gl::vec2{-66.9f,   0.0f},
		gl::vec2{77.0f,  33.0f}, gl::vec2{ 69.2f,   0.0f},
	};
	drawBezierCurve(dPadCurve);

	drawFilledCircle(tr(centerA), radiusABC, hovered[TRIG_A] || (hoveredRow == TRIG_A));
	drawLetterA(tr(centerA));
	drawFilledCircle(tr(centerB), radiusABC, hovered[TRIG_B] || (hoveredRow == TRIG_B));
	drawLetterB(tr(centerB));
	drawFilledCircle(tr(centerC), radiusABC, hovered[TRIG_C] || (hoveredRow == TRIG_C));
	drawLetterC(tr(centerC));
	drawFilledCircle(tr(centerX), radiusXYZ, hovered[TRIG_X] || (hoveredRow == TRIG_X));
	drawLetterX(tr(centerX));
	drawFilledCircle(tr(centerY), radiusXYZ, hovered[TRIG_Y] || (hoveredRow == TRIG_Y));
	drawLetterY(tr(centerY));
	drawFilledCircle(tr(centerZ), radiusXYZ, hovered[TRIG_Z] || (hoveredRow == TRIG_Z));
	drawLetterZ(tr(centerZ));
	std::array buttonCurve = {
		gl::vec2{221.1f,  28.9f}, gl::vec2{ 80.1f,   0.0f},
		gl::vec2{236.9f, 139.5f}, gl::vec2{-76.8f,   0.0f},
		gl::vec2{221.1f,  28.9f}, gl::vec2{ 80.1f,   0.0f},
	};
	drawBezierCurve(buttonCurve);

	auto corner = (selectBox.bottomRight.y - selectBox.topLeft.y) * 0.5f;
	auto trR = [&](Rectangle r) { return Rectangle{tr(r.topLeft), tr(r.bottomRight)}; };
	drawFilledRectangle(trR(selectBox), corner, hovered[TRIG_SELECT] || (hoveredRow == TRIG_SELECT));
	drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), tr({123.0f,  46.0f}), color, "Select");
	drawFilledRectangle(trR(startBox), corner, hovered[TRIG_START] || (hoveredRow == TRIG_START));
	drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), tr({128.0f,  97.0f}), color, "Start");
}

} // namespace joymega

namespace joyhandle {

enum : uint8_t {UP, DOWN, LEFT, RIGHT,
				TRIG_A, TRIG_B,
				WHEEL_LEFT, WHEEL_RIGHT,
				NUM_BUTTONS};

static constexpr std::array<zstring_view, NUM_BUTTONS> buttonNames = { // show in the GUI
	"Up", "Down", "Left", "Right",
	"A", "B",
	"Wheel left", "Wheel right",
};
static constexpr std::array<zstring_view, NUM_BUTTONS> keyNames = { // keys in Tcl dict
	"UP", "DOWN", "LEFT", "RIGHT",
	"A", "B",
	"WHEEL_LEFT", "WHEEL_RIGHT"
};

static constexpr auto boundingBox = gl::vec2{300.0f, 160.3f};
static constexpr float spanH = 0.50f;
// Each row is one filled rectangle: {x0, x1, yTop, yBottom} in local coords.
static constexpr std::array<std::array<float, 4>, 2239> fill = {{
	{{  94.52f, 103.44f,   0.00f,   0.50f }},
	{{ 236.86f, 245.72f,   0.00f,   0.50f }},
	{{  92.20f, 106.23f,   0.50f,   1.00f }},
	{{ 234.03f, 247.55f,   0.50f,   1.00f }},
	{{  90.97f,  96.04f,   1.00f,   1.50f }},
	{{ 102.16f, 107.85f,   1.00f,   1.50f }},
	{{ 232.44f, 237.52f,   1.00f,   1.50f }},
	{{ 244.23f, 248.88f,   1.00f,   1.50f }},
	{{  90.14f,  93.36f,   1.50f,   2.00f }},
	{{ 105.20f, 108.89f,   1.50f,   2.00f }},
	{{ 231.30f, 234.87f,   1.50f,   2.00f }},
	{{ 246.70f, 249.92f,   1.50f,   2.00f }},
	{{  89.50f,  91.72f,   2.00f,   2.50f }},
	{{ 106.75f, 109.72f,   2.00f,   2.50f }},
	{{ 230.50f, 233.10f,   2.00f,   2.50f }},
	{{ 248.13f, 250.62f,   2.00f,   2.50f }},
	{{  88.98f,  90.69f,   2.50f,   3.00f }},
	{{ 108.11f, 110.40f,   2.50f,   3.00f }},
	{{ 229.70f, 231.84f,   2.50f,   3.00f }},
	{{ 249.34f, 251.13f,   2.50f,   3.00f }},
	{{  88.58f,  89.99f,   3.00f,   3.50f }},
	{{ 108.86f, 110.88f,   3.00f,   3.50f }},
	{{ 229.20f, 230.96f,   3.00f,   3.50f }},
	{{ 249.89f, 251.51f,   3.00f,   3.50f }},
	{{  88.36f,  89.53f,   3.50f,   4.00f }},
	{{ 109.61f, 111.17f,   3.50f,   4.00f }},
	{{ 228.85f, 230.34f,   3.50f,   4.00f }},
	{{ 250.45f, 251.76f,   3.50f,   4.00f }},
	{{  88.25f,  89.24f,   4.00f,   4.50f }},
	{{  91.41f,  92.07f,   4.00f,   4.50f }},
	{{ 110.00f, 111.34f,   4.00f,   4.50f }},
	{{ 228.71f, 229.93f,   4.00f,   4.50f }},
	{{ 247.65f, 248.58f,   4.00f,   4.50f }},
	{{ 250.59f, 251.90f,   4.00f,   4.50f }},
	{{  88.17f,  89.17f,   4.50f,   5.00f }},
	{{  91.50f,  93.27f,   4.50f,   5.00f }},
	{{ 110.22f, 111.37f,   4.50f,   5.00f }},
	{{ 228.57f, 229.61f,   4.50f,   5.00f }},
	{{ 246.51f, 248.48f,   4.50f,   5.00f }},
	{{ 250.67f, 252.01f,   4.50f,   5.00f }},
	{{  88.09f,  89.28f,   5.00f,   5.50f }},
	{{  91.89f,  94.77f,   5.00f,   5.50f }},
	{{ 106.36f, 108.45f,   5.00f,   5.50f }},
	{{ 110.44f, 111.39f,   5.00f,   5.50f }},
	{{ 228.55f, 229.42f,   5.00f,   5.50f }},
	{{ 231.64f, 233.47f,   5.00f,   5.50f }},
	{{ 244.88f, 247.98f,   5.00f,   5.50f }},
	{{ 250.65f, 252.09f,   5.00f,   5.50f }},
	{{  88.01f,  89.55f,   5.50f,   6.00f }},
	{{  93.13f,  98.90f,   5.50f,   6.00f }},
	{{ 103.68f, 107.92f,   5.50f,   6.00f }},
	{{ 110.21f, 111.41f,   5.50f,   6.00f }},
	{{ 228.54f, 229.53f,   5.50f,   6.00f }},
	{{ 232.01f, 235.98f,   5.50f,   6.00f }},
	{{ 241.63f, 246.89f,   5.50f,   6.00f }},
	{{ 250.34f, 252.16f,   5.50f,   6.00f }},
	{{  87.95f,  90.08f,   6.00f,   6.50f }},
	{{  94.82f, 106.72f,   6.00f,   6.50f }},
	{{ 109.79f, 111.43f,   6.00f,   6.50f }},
	{{ 228.53f, 229.80f,   6.00f,   6.50f }},
	{{ 233.27f, 245.03f,   6.00f,   6.50f }},
	{{ 249.80f, 252.23f,   6.00f,   6.50f }},
	{{  87.88f,  90.84f,   6.50f,   7.00f }},
	{{  97.61f, 104.18f,   6.50f,   7.00f }},
	{{ 109.37f, 111.44f,   6.50f,   7.00f }},
	{{ 228.52f, 230.25f,   6.50f,   7.00f }},
	{{ 235.90f, 242.07f,   6.50f,   7.00f }},
	{{ 249.07f, 252.30f,   6.50f,   7.00f }},
	{{  87.82f,  91.84f,   7.00f,   7.50f }},
	{{ 108.94f, 111.43f,   7.00f,   7.50f }},
	{{ 228.51f, 231.00f,   7.00f,   7.50f }},
	{{ 248.10f, 250.66f,   7.00f,   7.50f }},
	{{ 250.89f, 252.36f,   7.00f,   7.50f }},
	{{  87.75f,  89.27f,   7.50f,   8.00f }},
	{{  90.39f,  93.33f,   7.50f,   8.00f }},
	{{ 107.33f, 111.42f,   7.50f,   8.00f }},
	{{ 228.52f, 232.20f,   7.50f,   8.00f }},
	{{ 246.34f, 249.59f,   7.50f,   8.00f }},
	{{ 251.01f, 252.42f,   7.50f,   8.00f }},
	{{  87.69f,  89.17f,   8.00f,   8.50f }},
	{{  91.39f,  96.57f,   8.00f,   8.50f }},
	{{ 105.48f, 111.41f,   8.00f,   8.50f }},
	{{ 228.54f, 229.99f,   8.00f,   8.50f }},
	{{ 230.40f, 234.57f,   8.00f,   8.50f }},
	{{ 244.34f, 248.55f,   8.00f,   8.50f }},
	{{ 251.08f, 252.49f,   8.00f,   9.00f }},
	{{  87.63f,  89.09f,   8.50f,   9.00f }},
	{{  92.85f, 108.59f,   8.50f,   9.00f }},
	{{ 109.99f, 111.40f,   8.50f,  11.50f }},
	{{ 228.55f, 229.98f,   8.50f,  12.00f }},
	{{ 231.81f, 247.38f,   8.50f,   9.00f }},
	{{  87.57f,  89.01f,   9.00f,   9.50f }},
	{{  94.79f, 106.99f,   9.00f,   9.50f }},
	{{ 233.50f, 245.28f,   9.00f,   9.50f }},
	{{ 251.20f, 252.61f,   9.00f,   9.50f }},
	{{  87.51f,  88.95f,   9.50f,  10.00f }},
	{{ 236.99f, 241.64f,   9.50f,  10.00f }},
	{{ 251.25f, 252.68f,   9.50f,  10.50f }},
	{{  87.45f,  88.88f,  10.00f,  10.50f }},
	{{  87.38f,  88.81f,  10.50f,  11.00f }},
	{{ 251.37f, 252.81f,  10.50f,  11.00f }},
	{{  87.33f,  88.75f,  11.00f,  12.00f }},
	{{ 251.43f, 252.87f,  11.00f,  11.50f }},
	{{ 109.99f, 111.35f,  11.50f,  13.50f }},
	{{ 251.49f, 252.93f,  11.50f,  12.00f }},
	{{  87.21f,  88.62f,  12.00f,  13.00f }},
	{{ 228.65f, 230.06f,  12.00f,  15.00f }},
	{{ 251.55f, 252.99f,  12.00f,  12.50f }},
	{{ 251.61f, 253.05f,  12.50f,  13.00f }},
	{{  87.10f,  88.49f,  13.00f,  13.50f }},
	{{ 251.67f, 253.11f,  13.00f,  13.50f }},
	{{  87.04f,  88.43f,  13.50f,  14.50f }},
	{{ 109.95f, 111.31f,  13.50f,  16.00f }},
	{{ 251.74f, 253.17f,  13.50f,  14.00f }},
	{{ 251.80f, 253.23f,  14.00f,  14.50f }},
	{{  86.93f,  88.30f,  14.50f,  15.00f }},
	{{ 251.86f, 253.29f,  14.50f,  15.50f }},
	{{  86.87f,  88.24f,  15.00f,  15.50f }},
	{{ 228.75f, 230.16f,  15.00f,  18.00f }},
	{{  86.80f,  88.18f,  15.50f,  16.00f }},
	{{ 251.99f, 253.40f,  15.50f,  16.00f }},
	{{  86.74f,  88.12f,  16.00f,  17.00f }},
	{{ 109.89f, 111.24f,  16.00f,  18.00f }},
	{{ 252.05f, 253.46f,  16.00f,  17.00f }},
	{{  86.61f,  88.01f,  17.00f,  17.50f }},
	{{ 252.18f, 253.57f,  17.00f,  18.00f }},
	{{  86.55f,  87.95f,  17.50f,  18.00f }},
	{{  86.49f,  87.89f,  18.00f,  18.50f }},
	{{ 109.84f, 111.19f,  18.00f,  19.50f }},
	{{ 228.85f, 230.26f,  18.00f,  20.50f }},
	{{ 252.31f, 253.69f,  18.00f,  18.50f }},
	{{  86.43f,  87.83f,  18.50f,  19.50f }},
	{{ 252.38f, 253.75f,  18.50f,  19.00f }},
	{{ 252.44f, 253.80f,  19.00f,  19.50f }},
	{{  86.31f,  87.72f,  19.50f,  20.00f }},
	{{ 109.80f, 111.15f,  19.50f,  22.00f }},
	{{ 252.51f, 253.86f,  19.50f,  20.00f }},
	{{  86.25f,  87.66f,  20.00f,  20.50f }},
	{{ 252.57f, 253.92f,  20.00f,  20.50f }},
	{{  86.19f,  87.61f,  20.50f,  21.00f }},
	{{ 228.94f, 230.36f,  20.50f,  21.00f }},
	{{ 252.64f, 253.98f,  20.50f,  21.00f }},
	{{  86.13f,  87.55f,  21.00f,  22.00f }},
	{{ 228.96f, 230.37f,  21.00f,  23.50f }},
	{{ 252.70f, 254.05f,  21.00f,  21.50f }},
	{{ 252.77f, 254.11f,  21.50f,  22.00f }},
	{{  86.01f,  87.43f,  22.00f,  22.50f }},
	{{ 109.74f, 111.08f,  22.00f,  23.50f }},
	{{ 252.83f, 254.17f,  22.00f,  22.50f }},
	{{  85.95f,  87.37f,  22.50f,  23.00f }},
	{{ 252.90f, 254.23f,  22.50f,  23.00f }},
	{{  85.89f,  87.31f,  23.00f,  23.50f }},
	{{ 252.96f, 254.29f,  23.00f,  24.00f }},
	{{  85.83f,  87.24f,  23.50f,  24.50f }},
	{{ 109.69f, 111.04f,  23.50f,  25.00f }},
	{{ 229.04f, 230.46f,  23.50f,  24.00f }},
	{{ 229.06f, 230.48f,  24.00f,  26.00f }},
	{{ 253.08f, 254.41f,  24.00f,  24.50f }},
	{{  85.71f,  87.11f,  24.50f,  25.00f }},
	{{ 253.14f, 254.47f,  24.50f,  25.00f }},
	{{  85.65f,  87.05f,  25.00f,  26.00f }},
	{{ 109.65f, 111.00f,  25.00f,  27.50f }},
	{{ 253.21f, 254.52f,  25.00f,  25.50f }},
	{{ 253.27f, 254.58f,  25.50f,  26.50f }},
	{{  85.53f,  86.91f,  26.00f,  26.50f }},
	{{ 229.13f, 230.55f,  26.00f,  27.00f }},
	{{  85.47f,  86.84f,  26.50f,  27.00f }},
	{{ 253.39f, 254.71f,  26.50f,  27.00f }},
	{{  85.41f,  86.77f,  27.00f,  27.50f }},
	{{ 229.16f, 230.59f,  27.00f,  29.00f }},
	{{ 253.45f, 254.77f,  27.00f,  27.50f }},
	{{  85.35f,  86.71f,  27.50f,  28.00f }},
	{{ 109.57f, 110.94f,  27.50f,  28.50f }},
	{{ 253.51f, 254.84f,  27.50f,  28.00f }},
	{{  85.29f,  86.64f,  28.00f,  28.50f }},
	{{ 253.57f, 254.90f,  28.00f,  28.50f }},
	{{  85.23f,  86.57f,  28.50f,  29.00f }},
	{{ 109.54f, 110.91f,  28.50f,  31.00f }},
	{{ 253.63f, 254.97f,  28.50f,  29.00f }},
	{{  85.17f,  86.50f,  29.00f,  29.50f }},
	{{ 229.23f, 230.66f,  29.00f,  29.50f }},
	{{ 253.69f, 255.03f,  29.00f,  29.50f }},
	{{  85.11f,  86.44f,  29.50f,  30.50f }},
	{{ 229.25f, 230.67f,  29.50f,  32.00f }},
	{{ 253.75f, 255.09f,  29.50f,  30.00f }},
	{{ 253.81f, 255.16f,  30.00f,  30.50f }},
	{{  84.99f,  86.32f,  30.50f,  31.00f }},
	{{ 253.87f, 255.22f,  30.50f,  31.00f }},
	{{  84.93f,  86.26f,  31.00f,  31.50f }},
	{{ 109.47f, 110.84f,  31.00f,  32.00f }},
	{{ 253.93f, 255.28f,  31.00f,  31.50f }},
	{{  84.87f,  86.20f,  31.50f,  32.00f }},
	{{ 253.99f, 255.34f,  31.50f,  32.00f }},
	{{  84.81f,  86.15f,  32.00f,  33.00f }},
	{{ 109.44f, 110.81f,  32.00f,  34.00f }},
	{{ 229.34f, 230.76f,  32.00f,  32.50f }},
	{{ 254.05f, 255.40f,  32.00f,  32.50f }},
	{{ 229.36f, 230.77f,  32.50f,  35.00f }},
	{{ 254.11f, 255.46f,  32.50f,  33.00f }},
	{{  84.69f,  86.03f,  33.00f,  33.50f }},
	{{ 254.17f, 255.52f,  33.00f,  33.50f }},
	{{  84.63f,  85.98f,  33.50f,  34.00f }},
	{{ 254.23f, 255.58f,  33.50f,  34.00f }},
	{{  84.57f,  85.92f,  34.00f,  34.50f }},
	{{ 109.37f, 110.75f,  34.00f,  34.50f }},
	{{ 254.29f, 255.64f,  34.00f,  34.50f }},
	{{  84.51f,  85.86f,  34.50f,  35.00f }},
	{{ 109.35f, 110.73f,  34.50f,  37.00f }},
	{{ 254.35f, 255.70f,  34.50f,  35.00f }},
	{{  84.45f,  85.80f,  35.00f,  35.50f }},
	{{ 229.46f, 230.86f,  35.00f,  37.00f }},
	{{ 254.41f, 255.76f,  35.00f,  35.50f }},
	{{  84.38f,  85.75f,  35.50f,  36.00f }},
	{{ 254.47f, 255.82f,  35.50f,  36.00f }},
	{{  84.31f,  85.69f,  36.00f,  36.50f }},
	{{ 254.53f, 255.88f,  36.00f,  36.50f }},
	{{  84.24f,  85.63f,  36.50f,  37.50f }},
	{{ 254.60f, 255.94f,  36.50f,  37.00f }},
	{{ 109.26f, 110.63f,  37.00f,  37.50f }},
	{{ 229.55f, 230.92f,  37.00f,  38.00f }},
	{{ 254.66f, 256.00f,  37.00f,  37.50f }},
	{{  84.07f,  85.51f,  37.50f,  38.00f }},
	{{ 109.24f, 110.62f,  37.50f,  38.50f }},
	{{ 254.72f, 256.06f,  37.50f,  38.00f }},
	{{  83.93f,  85.45f,  38.00f,  38.50f }},
	{{ 229.56f, 230.95f,  38.00f,  38.50f }},
	{{ 254.79f, 256.12f,  38.00f,  38.50f }},
	{{  27.12f,  85.38f,  38.50f,  39.00f }},
	{{ 109.21f, 230.97f,  38.50f,  40.00f }},
	{{ 254.85f, 274.89f,  38.50f,  39.00f }},
	{{  24.94f,  85.32f,  39.00f,  39.50f }},
	{{ 254.92f, 276.78f,  39.00f,  39.50f }},
	{{  23.84f,  85.25f,  39.50f,  40.00f }},
	{{ 254.99f, 277.61f,  39.50f,  40.00f }},
	{{  23.07f,  26.81f,  40.00f,  40.50f }},
	{{  83.61f,  85.19f,  40.00f,  40.50f }},
	{{ 109.15f, 110.65f,  40.00f,  40.50f }},
	{{ 229.63f, 231.02f,  40.00f,  40.50f }},
	{{ 255.05f, 256.73f,  40.00f,  40.50f }},
	{{ 275.29f, 278.20f,  40.00f,  40.50f }},
	{{  22.51f,  24.71f,  40.50f,  41.00f }},
	{{  83.61f,  85.13f,  40.50f,  41.50f }},
	{{ 109.13f, 110.59f,  40.50f,  41.50f }},
	{{ 229.65f, 231.03f,  40.50f,  41.50f }},
	{{ 255.12f, 256.43f,  40.50f,  41.00f }},
	{{ 276.67f, 278.69f,  40.50f,  41.00f }},
	{{  22.07f,  23.93f,  41.00f,  41.50f }},
	{{ 255.18f, 256.50f,  41.00f,  41.50f }},
	{{ 277.31f, 279.14f,  41.00f,  41.50f }},
	{{  21.67f,  23.44f,  41.50f,  42.00f }},
	{{  83.59f,  85.01f,  41.50f,  42.00f }},
	{{ 109.10f, 110.51f,  41.50f,  42.50f }},
	{{ 229.71f, 231.06f,  41.50f,  42.50f }},
	{{ 255.24f, 256.58f,  41.50f,  42.00f }},
	{{ 277.78f, 279.54f,  41.50f,  42.00f }},
	{{  21.27f,  23.05f,  42.00f,  42.50f }},
	{{  83.56f,  84.95f,  42.00f,  42.50f }},
	{{ 255.30f, 256.65f,  42.00f,  42.50f }},
	{{ 278.19f, 279.93f,  42.00f,  42.50f }},
	{{  20.86f,  22.66f,  42.50f,  43.00f }},
	{{  83.52f,  84.89f,  42.50f,  43.00f }},
	{{ 109.06f, 110.44f,  42.50f,  43.00f }},
	{{ 229.76f, 231.10f,  42.50f,  44.00f }},
	{{ 255.36f, 256.72f,  42.50f,  43.00f }},
	{{ 278.60f, 280.33f,  42.50f,  43.00f }},
	{{  20.42f,  22.28f,  43.00f,  43.50f }},
	{{  83.46f,  84.83f,  43.00f,  43.50f }},
	{{ 109.04f, 110.42f,  43.00f,  45.50f }},
	{{ 255.42f, 256.78f,  43.00f,  43.50f }},
	{{ 278.98f, 280.72f,  43.00f,  43.50f }},
	{{  19.99f,  21.88f,  43.50f,  44.00f }},
	{{  83.40f,  84.78f,  43.50f,  44.00f }},
	{{ 255.47f, 256.84f,  43.50f,  44.00f }},
	{{ 279.37f, 281.11f,  43.50f,  44.00f }},
	{{  19.55f,  21.47f,  44.00f,  44.50f }},
	{{  83.34f,  84.72f,  44.00f,  45.00f }},
	{{ 229.82f, 231.15f,  44.00f,  45.50f }},
	{{ 255.53f, 256.90f,  44.00f,  44.50f }},
	{{ 279.76f, 281.51f,  44.00f,  44.50f }},
	{{  19.12f,  21.02f,  44.50f,  45.00f }},
	{{ 255.59f, 256.96f,  44.50f,  45.50f }},
	{{ 280.16f, 281.91f,  44.50f,  45.00f }},
	{{  18.68f,  20.58f,  45.00f,  45.50f }},
	{{  83.21f,  84.61f,  45.00f,  46.00f }},
	{{ 280.56f, 282.32f,  45.00f,  45.50f }},
	{{  18.23f,  20.13f,  45.50f,  46.00f }},
	{{ 108.94f, 110.33f,  45.50f,  48.00f }},
	{{ 229.86f, 231.21f,  45.50f,  47.00f }},
	{{ 255.70f, 257.08f,  45.50f,  46.00f }},
	{{ 280.97f, 282.74f,  45.50f,  46.00f }},
	{{  17.78f,  19.67f,  46.00f,  46.50f }},
	{{  83.10f,  84.50f,  46.00f,  46.50f }},
	{{ 255.75f, 257.14f,  46.00f,  46.50f }},
	{{ 281.38f, 283.16f,  46.00f,  46.50f }},
	{{  17.33f,  19.22f,  46.50f,  47.00f }},
	{{  83.05f,  84.44f,  46.50f,  47.50f }},
	{{ 255.81f, 257.20f,  46.50f,  47.00f }},
	{{ 281.79f, 283.57f,  46.50f,  47.00f }},
	{{  16.88f,  18.77f,  47.00f,  47.50f }},
	{{ 229.89f, 231.26f,  47.00f,  49.50f }},
	{{ 255.86f, 257.26f,  47.00f,  48.00f }},
	{{ 282.20f, 283.98f,  47.00f,  47.50f }},
	{{  16.43f,  18.31f,  47.50f,  48.00f }},
	{{  82.94f,  84.34f,  47.50f,  48.50f }},
	{{ 282.61f, 284.39f,  47.50f,  48.00f }},
	{{  15.97f,  17.86f,  48.00f,  48.50f }},
	{{ 108.84f, 110.26f,  48.00f,  48.50f }},
	{{ 255.97f, 257.39f,  48.00f,  49.00f }},
	{{ 283.03f, 284.80f,  48.00f,  48.50f }},
	{{  15.51f,  17.40f,  48.50f,  49.00f }},
	{{  82.82f,  84.23f,  48.50f,  49.50f }},
	{{ 108.82f, 110.25f,  48.50f,  50.50f }},
	{{ 283.44f, 285.21f,  48.50f,  49.00f }},
	{{  15.05f,  16.94f,  49.00f,  49.50f }},
	{{ 256.08f, 257.51f,  49.00f,  49.50f }},
	{{ 283.86f, 285.61f,  49.00f,  49.50f }},
	{{  14.58f,  16.48f,  49.50f,  50.00f }},
	{{  82.71f,  84.11f,  49.50f,  50.00f }},
	{{ 229.94f, 231.37f,  49.50f,  51.00f }},
	{{ 256.14f, 257.57f,  49.50f,  50.00f }},
	{{ 284.27f, 286.02f,  49.50f,  50.00f }},
	{{  14.12f,  16.03f,  50.00f,  50.50f }},
	{{  82.65f,  84.05f,  50.00f,  50.50f }},
	{{ 256.19f, 257.64f,  50.00f,  50.50f }},
	{{ 284.69f, 286.42f,  50.00f,  50.50f }},
	{{  13.66f,  15.57f,  50.50f,  51.00f }},
	{{  82.59f,  83.99f,  50.50f,  51.00f }},
	{{ 108.74f, 110.20f,  50.50f,  53.50f }},
	{{ 256.24f, 257.70f,  50.50f,  51.00f }},
	{{ 285.09f, 286.83f,  50.50f,  51.00f }},
	{{  13.19f,  15.11f,  51.00f,  51.50f }},
	{{  82.53f,  83.93f,  51.00f,  52.00f }},
	{{ 229.96f, 231.44f,  51.00f,  51.50f }},
	{{ 256.29f, 257.76f,  51.00f,  51.50f }},
	{{ 285.50f, 287.23f,  51.00f,  51.50f }},
	{{  12.72f,  14.65f,  51.50f,  52.00f }},
	{{ 229.96f, 231.46f,  51.50f,  54.00f }},
	{{ 256.35f, 257.82f,  51.50f,  52.00f }},
	{{ 285.90f, 287.63f,  51.50f,  52.00f }},
	{{  12.27f,  14.19f,  52.00f,  52.50f }},
	{{  82.41f,  83.80f,  52.00f,  52.50f }},
	{{ 256.42f, 257.88f,  52.00f,  52.50f }},
	{{ 286.30f, 288.03f,  52.00f,  52.50f }},
	{{  11.81f,  13.74f,  52.50f,  53.00f }},
	{{  82.35f,  83.74f,  52.50f,  53.50f }},
	{{ 256.49f, 257.94f,  52.50f,  53.00f }},
	{{ 286.70f, 288.44f,  52.50f,  53.00f }},
	{{  11.36f,  13.28f,  53.00f,  53.50f }},
	{{ 256.57f, 257.99f,  53.00f,  53.50f }},
	{{ 287.11f, 288.84f,  53.00f,  53.50f }},
	{{  10.91f,  12.82f,  53.50f,  54.00f }},
	{{  82.23f,  83.61f,  53.50f,  54.50f }},
	{{ 108.64f, 110.21f,  53.50f,  54.50f }},
	{{ 256.64f, 258.05f,  53.50f,  54.00f }},
	{{ 287.51f, 289.25f,  53.50f,  54.00f }},
	{{  10.46f,  12.36f,  54.00f,  54.50f }},
	{{ 229.99f, 231.57f,  54.00f,  54.50f }},
	{{ 256.71f, 258.10f,  54.00f,  54.50f }},
	{{ 287.91f, 289.65f,  54.00f,  54.50f }},
	{{  10.02f,  11.90f,  54.50f,  55.00f }},
	{{  82.10f,  83.49f,  54.50f,  55.00f }},
	{{ 108.61f, 231.59f,  54.50f,  56.00f }},
	{{ 256.78f, 258.16f,  54.50f,  55.50f }},
	{{ 288.32f, 290.06f,  54.50f,  55.00f }},
	{{   9.56f,  11.45f,  55.00f,  55.50f }},
	{{  82.04f,  83.43f,  55.00f,  56.00f }},
	{{ 288.73f, 290.46f,  55.00f,  55.50f }},
	{{   9.11f,  10.99f,  55.50f,  56.00f }},
	{{ 256.90f, 258.27f,  55.50f,  56.00f }},
	{{ 289.13f, 290.86f,  55.50f,  56.00f }},
	{{   8.65f,  10.54f,  56.00f,  56.50f }},
	{{  81.91f,  83.30f,  56.00f,  56.50f }},
	{{ 108.56f, 109.91f,  56.00f,  56.50f }},
	{{ 230.19f, 231.65f,  56.00f,  56.50f }},
	{{ 256.97f, 258.33f,  56.00f,  56.50f }},
	{{ 289.54f, 291.27f,  56.00f,  56.50f }},
	{{   8.19f,  10.09f,  56.50f,  57.00f }},
	{{  81.83f,  83.24f,  56.50f,  57.00f }},
	{{ 108.54f, 109.89f,  56.50f,  57.00f }},
	{{ 230.23f, 231.67f,  56.50f,  57.00f }},
	{{ 257.03f, 258.40f,  56.50f,  57.00f }},
	{{ 289.95f, 291.67f,  56.50f,  57.00f }},
	{{   7.73f,   9.64f,  57.00f,  57.50f }},
	{{  81.73f,  83.18f,  57.00f,  57.50f }},
	{{ 108.53f, 152.90f,  57.00f,  57.50f }},
	{{ 230.27f, 231.69f,  57.00f,  59.00f }},
	{{ 257.09f, 258.46f,  57.00f,  57.50f }},
	{{ 290.35f, 292.07f,  57.00f,  57.50f }},
	{{   7.28f,   9.19f,  57.50f,  58.00f }},
	{{  81.63f,  83.12f,  57.50f,  58.00f }},
	{{ 108.51f, 229.13f,  57.50f,  58.00f }},
	{{ 257.15f, 258.53f,  57.50f,  58.00f }},
	{{ 290.75f, 292.46f,  57.50f,  58.00f }},
	{{   6.84f,   8.76f,  58.00f,  58.50f }},
	{{  81.46f,  83.06f,  58.00f,  58.50f }},
	{{ 108.49f, 192.27f,  58.00f,  58.50f }},
	{{ 257.21f, 258.58f,  58.00f,  58.50f }},
	{{ 291.15f, 292.86f,  58.00f,  58.50f }},
	{{   6.39f,   8.33f,  58.50f,  59.00f }},
	{{  81.29f,  83.01f,  58.50f,  59.00f }},
	{{ 108.48f, 109.94f,  58.50f,  59.50f }},
	{{ 257.27f, 258.61f,  58.50f,  59.50f }},
	{{ 291.53f, 293.25f,  58.50f,  59.00f }},
	{{   5.98f,   7.93f,  59.00f,  59.50f }},
	{{  81.01f,  82.95f,  59.00f,  59.50f }},
	{{ 230.36f, 231.76f,  59.00f,  61.50f }},
	{{ 291.91f, 293.64f,  59.00f,  59.50f }},
	{{   5.58f,   7.76f,  59.50f,  60.00f }},
	{{  80.59f,  82.89f,  59.50f,  60.00f }},
	{{ 108.45f, 109.89f,  59.50f,  60.50f }},
	{{ 257.40f, 258.71f,  59.50f,  60.00f }},
	{{ 292.24f, 294.02f,  59.50f,  60.00f }},
	{{   5.17f,  82.84f,  60.00f,  60.50f }},
	{{ 257.46f, 294.40f,  60.00f,  60.50f }},
	{{   4.82f,  82.78f,  60.50f,  61.00f }},
	{{ 108.41f, 109.84f,  60.50f,  62.50f }},
	{{ 257.52f, 294.77f,  60.50f,  61.00f }},
	{{   4.46f,   6.14f,  61.00f,  61.50f }},
	{{  72.85f,  82.72f,  61.00f,  61.50f }},
	{{ 257.58f, 295.12f,  61.00f,  61.50f }},
	{{   4.12f,   5.71f,  61.50f,  62.00f }},
	{{  79.52f,  82.66f,  61.50f,  62.00f }},
	{{ 230.46f, 231.85f,  61.50f,  64.00f }},
	{{ 257.64f, 259.02f,  61.50f,  62.00f }},
	{{ 293.70f, 295.47f,  61.50f,  62.00f }},
	{{   3.85f,   5.42f,  62.00f,  62.50f }},
	{{  79.17f,  80.94f,  62.00f,  62.50f }},
	{{  81.10f,  82.60f,  62.00f,  62.50f }},
	{{ 257.70f, 259.08f,  62.00f,  62.50f }},
	{{ 294.13f, 295.77f,  62.00f,  62.50f }},
	{{   3.57f,   5.12f,  62.50f,  63.00f }},
	{{  78.81f,  80.39f,  62.50f,  63.00f }},
	{{  81.13f,  82.54f,  62.50f,  63.50f }},
	{{ 108.35f, 109.75f,  62.50f,  63.00f }},
	{{ 257.76f, 259.14f,  62.50f,  63.00f }},
	{{ 294.45f, 296.02f,  62.50f,  63.00f }},
	{{   3.44f,   4.97f,  63.00f,  63.50f }},
	{{  78.47f,  80.01f,  63.00f,  63.50f }},
	{{ 108.33f, 109.73f,  63.00f,  65.50f }},
	{{ 257.82f, 259.21f,  63.00f,  63.50f }},
	{{ 294.72f, 296.17f,  63.00f,  63.50f }},
	{{   3.31f,   4.87f,  63.50f,  64.00f }},
	{{  78.13f,  79.68f,  63.50f,  64.00f }},
	{{  81.01f,  82.42f,  63.50f,  64.00f }},
	{{ 257.88f, 259.30f,  63.50f,  64.00f }},
	{{ 294.92f, 296.29f,  63.50f,  64.00f }},
	{{   3.24f,   4.76f,  64.00f,  64.50f }},
	{{  77.80f,  79.34f,  64.00f,  64.50f }},
	{{  80.94f,  82.36f,  64.00f,  64.50f }},
	{{ 230.57f, 231.95f,  64.00f,  64.50f }},
	{{ 257.94f, 259.47f,  64.00f,  64.50f }},
	{{ 295.08f, 296.37f,  64.00f,  64.50f }},
	{{   3.18f,   4.66f,  64.50f,  65.00f }},
	{{   7.93f,  79.16f,  64.50f,  65.00f }},
	{{  80.88f,  82.29f,  64.50f,  65.00f }},
	{{ 230.59f, 231.96f,  64.50f,  66.00f }},
	{{ 258.00f, 264.51f,  64.50f,  65.00f }},
	{{ 295.19f, 296.46f,  64.50f,  65.00f }},
	{{   3.12f,   4.59f,  65.00f,  66.00f }},
	{{   7.01f,  78.98f,  65.00f,  65.50f }},
	{{  80.82f,  82.23f,  65.00f,  66.00f }},
	{{ 258.06f, 291.77f,  65.00f,  65.50f }},
	{{ 295.25f, 296.51f,  65.00f,  65.50f }},
	{{   6.58f,  69.48f,  65.50f,  66.00f }},
	{{  76.05f,  78.85f,  65.50f,  66.00f }},
	{{ 108.25f, 109.64f,  65.50f,  66.00f }},
	{{ 258.12f, 292.29f,  65.50f,  66.00f }},
	{{ 295.29f, 296.56f,  65.50f,  67.00f }},
	{{   3.05f,   4.52f,  66.00f,  67.50f }},
	{{   6.20f,  68.44f,  66.00f,  66.50f }},
	{{  77.25f,  78.78f,  66.00f,  66.50f }},
	{{  80.69f,  82.11f,  66.00f,  66.50f }},
	{{ 108.24f, 109.62f,  66.00f,  68.50f }},
	{{ 230.66f, 232.02f,  66.00f,  67.00f }},
	{{ 258.19f, 259.84f,  66.00f,  67.00f }},
	{{ 276.08f, 277.72f,  66.00f,  66.50f }},
	{{ 291.04f, 292.58f,  66.00f,  66.50f }},
	{{   6.18f,  68.84f,  66.50f,  67.00f }},
	{{  77.25f,  78.71f,  66.50f,  67.00f }},
	{{  80.63f,  82.04f,  66.50f,  67.50f }},
	{{ 276.16f, 277.68f,  66.50f,  68.00f }},
	{{ 291.42f, 292.67f,  66.50f,  67.00f }},
	{{   6.18f,   8.58f,  67.00f,  67.50f }},
	{{  67.35f,  69.15f,  67.00f,  67.50f }},
	{{  77.24f,  78.64f,  67.00f,  68.50f }},
	{{ 230.70f, 232.06f,  67.00f,  68.50f }},
	{{ 258.31f, 259.80f,  67.00f,  67.50f }},
	{{ 291.47f, 292.73f,  67.00f,  67.50f }},
	{{ 295.37f, 296.68f,  67.00f,  68.50f }},
	{{   2.96f,   4.42f,  67.50f,  68.00f }},
	{{   6.18f,   7.93f,  67.50f,  68.00f }},
	{{  67.57f,  69.15f,  67.50f,  68.00f }},
	{{  80.51f,  81.91f,  67.50f,  68.00f }},
	{{ 258.37f, 259.83f,  67.50f,  68.00f }},
	{{ 291.50f, 292.77f,  67.50f,  69.00f }},
	{{   2.94f,   4.38f,  68.00f,  69.00f }},
	{{   6.18f,   7.51f,  68.00f,  76.50f }},
	{{  67.67f,  69.15f,  68.00f,  71.00f }},
	{{  80.44f,  81.85f,  68.00f,  69.00f }},
	{{ 258.43f, 259.88f,  68.00f,  68.50f }},
	{{ 276.27f, 277.68f,  68.00f,  70.00f }},
	{{  77.16f,  78.52f,  68.50f,  69.00f }},
	{{ 108.16f, 109.54f,  68.50f,  69.00f }},
	{{ 230.76f, 232.12f,  68.50f,  69.50f }},
	{{ 258.50f, 259.93f,  68.50f,  69.00f }},
	{{ 295.43f, 296.76f,  68.50f,  69.00f }},
	{{   2.91f,   4.32f,  69.00f,  71.00f }},
	{{  77.14f,  78.48f,  69.00f,  69.50f }},
	{{  80.32f,  81.72f,  69.00f,  70.00f }},
	{{ 108.14f, 109.52f,  69.00f,  71.50f }},
	{{ 258.56f, 259.98f,  69.00f,  70.00f }},
	{{ 291.56f, 292.85f,  69.00f,  71.50f }},
	{{ 295.45f, 296.78f,  69.00f,  71.00f }},
	{{  77.11f,  78.45f,  69.50f,  71.00f }},
	{{ 230.81f, 232.16f,  69.50f,  71.00f }},
	{{  80.20f,  81.59f,  70.00f,  70.50f }},
	{{ 258.68f, 260.11f,  70.00f,  70.50f }},
	{{ 276.35f, 277.73f,  70.00f,  71.00f }},
	{{  80.13f,  81.52f,  70.50f,  71.50f }},
	{{ 258.75f, 260.18f,  70.50f,  71.00f }},
	{{   2.84f,   4.26f,  71.00f,  71.50f }},
	{{  67.75f,  69.15f,  71.00f,  96.00f }},
	{{  77.02f,  78.35f,  71.00f,  72.50f }},
	{{ 230.87f, 232.23f,  71.00f,  72.00f }},
	{{ 258.81f, 260.25f,  71.00f,  71.50f }},
	{{ 276.38f, 277.76f,  71.00f,  73.50f }},
	{{ 295.52f, 296.86f,  71.00f,  72.00f }},
	{{   2.83f,   4.24f,  71.50f,  74.50f }},
	{{  80.01f,  81.39f,  71.50f,  72.00f }},
	{{ 108.05f, 109.44f,  71.50f,  74.50f }},
	{{ 258.87f, 260.31f,  71.50f,  72.00f }},
	{{ 291.64f, 292.95f,  71.50f,  72.00f }},
	{{  23.85f,  29.77f,  72.00f,  72.50f }},
	{{  79.95f,  81.33f,  72.00f,  72.50f }},
	{{ 149.67f, 189.99f,  72.00f,  72.50f }},
	{{ 230.91f, 232.27f,  72.00f,  73.50f }},
	{{ 258.94f, 260.38f,  72.00f,  72.50f }},
	{{ 291.66f, 292.97f,  72.00f,  74.50f }},
	{{ 295.56f, 296.90f,  72.00f,  73.50f }},
	{{  21.28f,  31.63f,  72.50f,  73.00f }},
	{{  76.93f,  78.25f,  72.50f,  74.00f }},
	{{  79.89f,  81.26f,  72.50f,  73.00f }},
	{{ 148.62f, 191.69f,  72.50f,  73.00f }},
	{{ 259.00f, 260.45f,  72.50f,  73.00f }},
	{{  19.80f,  26.19f,  73.00f,  73.50f }},
	{{  27.93f,  33.15f,  73.00f,  73.50f }},
	{{  79.83f,  81.20f,  73.00f,  73.50f }},
	{{ 147.73f, 157.33f,  73.00f,  73.50f }},
	{{ 186.23f, 192.76f,  73.00f,  73.50f }},
	{{ 259.07f, 260.51f,  73.00f,  73.50f }},
	{{  18.67f,  22.29f,  73.50f,  74.00f }},
	{{  30.81f,  34.46f,  73.50f,  74.00f }},
	{{  79.77f,  81.14f,  73.50f,  74.00f }},
	{{ 146.92f, 149.65f,  73.50f,  74.00f }},
	{{ 190.37f, 193.62f,  73.50f,  74.00f }},
	{{ 230.97f, 232.33f,  73.50f,  74.50f }},
	{{ 259.13f, 260.58f,  73.50f,  74.00f }},
	{{ 276.45f, 277.85f,  73.50f,  74.00f }},
	{{ 295.61f, 296.96f,  73.50f,  74.50f }},
	{{  17.65f,  20.55f,  74.00f,  74.50f }},
	{{  32.38f,  35.30f,  74.00f,  74.50f }},
	{{  76.83f,  78.13f,  74.00f,  75.50f }},
	{{  79.70f,  81.07f,  74.00f,  74.50f }},
	{{ 146.26f, 148.38f,  74.00f,  74.50f }},
	{{ 191.70f, 194.30f,  74.00f,  74.50f }},
	{{ 259.19f, 260.64f,  74.00f,  74.50f }},
	{{ 276.47f, 277.86f,  74.00f,  76.50f }},
	{{   2.76f,   4.14f,  74.50f,  75.00f }},
	{{  16.72f,  19.40f,  74.50f,  75.00f }},
	{{  33.57f,  36.14f,  74.50f,  75.00f }},
	{{  79.64f,  81.01f,  74.50f,  75.00f }},
	{{ 107.94f, 109.35f,  74.50f,  77.00f }},
	{{ 145.66f, 147.50f,  74.50f,  75.00f }},
	{{ 192.67f, 194.81f,  74.50f,  75.00f }},
	{{ 231.01f, 232.36f,  74.50f,  75.50f }},
	{{ 259.26f, 260.71f,  74.50f,  75.00f }},
	{{ 291.73f, 293.07f,  74.50f,  75.50f }},
	{{ 295.65f, 297.00f,  74.50f,  76.00f }},
	{{   2.75f,   4.13f,  75.00f,  78.00f }},
	{{  15.95f,  18.43f,  75.00f,  75.50f }},
	{{  34.59f,  36.99f,  75.00f,  75.50f }},
	{{  79.58f,  80.95f,  75.00f,  75.50f }},
	{{ 145.17f, 146.89f,  75.00f,  75.50f }},
	{{ 193.39f, 195.32f,  75.00f,  75.50f }},
	{{ 259.33f, 260.77f,  75.00f,  75.50f }},
	{{  15.36f,  17.58f,  75.50f,  76.00f }},
	{{  35.47f,  37.77f,  75.50f,  76.00f }},
	{{  76.73f,  78.02f,  75.50f,  76.50f }},
	{{  79.52f,  80.88f,  75.50f,  76.00f }},
	{{ 144.68f, 146.32f,  75.50f,  76.00f }},
	{{ 194.05f, 195.73f,  75.50f,  76.00f }},
	{{ 231.05f, 232.40f,  75.50f,  77.00f }},
	{{ 259.40f, 260.83f,  75.50f,  76.00f }},
	{{ 291.76f, 293.11f,  75.50f,  77.00f }},
	{{  14.77f,  16.78f,  76.00f,  76.50f }},
	{{  36.29f,  38.31f,  76.00f,  76.50f }},
	{{  79.46f,  80.82f,  76.00f,  76.50f }},
	{{ 144.29f, 145.87f,  76.00f,  76.50f }},
	{{ 194.52f, 196.15f,  76.00f,  76.50f }},
	{{ 259.47f, 260.90f,  76.00f,  76.50f }},
	{{ 295.71f, 297.06f,  76.00f,  77.50f }},
	{{   6.15f,   7.51f,  76.50f,  84.00f }},
	{{  14.19f,  16.09f,  76.50f,  77.00f }},
	{{  21.17f,  22.50f,  76.50f,  77.00f }},
	{{  36.94f,  38.84f,  76.50f,  77.00f }},
	{{  76.66f,  77.95f,  76.50f,  77.00f }},
	{{  79.40f,  80.76f,  76.50f,  77.00f }},
	{{ 143.92f, 145.42f,  76.50f,  77.00f }},
	{{ 194.98f, 196.50f,  76.50f,  77.00f }},
	{{ 259.53f, 260.96f,  76.50f,  77.00f }},
	{{ 276.55f, 277.95f,  76.50f,  77.00f }},
	{{  13.60f,  15.45f,  77.00f,  77.50f }},
	{{  18.11f,  25.30f,  77.00f,  77.50f }},
	{{  37.60f,  39.37f,  77.00f,  77.50f }},
	{{  76.63f,  77.91f,  77.00f,  78.00f }},
	{{  79.34f,  80.70f,  77.00f,  77.50f }},
	{{ 107.85f, 109.27f,  77.00f,  78.00f }},
	{{ 143.57f, 145.09f,  77.00f,  77.50f }},
	{{ 195.26f, 196.83f,  77.00f,  77.50f }},
	{{ 231.11f, 232.46f,  77.00f,  78.50f }},
	{{ 259.60f, 261.03f,  77.00f,  77.50f }},
	{{ 276.56f, 277.97f,  77.00f,  79.50f }},
	{{ 291.80f, 293.17f,  77.00f,  79.00f }},
	{{  13.14f,  14.90f,  77.50f,  78.00f }},
	{{  16.36f,  26.93f,  77.50f,  78.00f }},
	{{  38.13f,  39.91f,  77.50f,  78.00f }},
	{{  79.28f,  80.64f,  77.50f,  78.00f }},
	{{ 143.34f, 144.76f,  77.50f,  78.00f }},
	{{ 151.12f, 189.29f,  77.50f,  78.00f }},
	{{ 195.53f, 197.11f,  77.50f,  78.00f }},
	{{ 259.66f, 261.10f,  77.50f,  78.00f }},
	{{ 295.77f, 297.12f,  77.50f,  78.50f }},
	{{   2.67f,   4.04f,  78.00f,  79.00f }},
	{{  12.74f,  14.58f,  78.00f,  78.50f }},
	{{  14.83f,  19.53f,  78.00f,  78.50f }},
	{{  23.90f,  27.96f,  78.00f,  78.50f }},
	{{  38.59f,  40.44f,  78.00f,  78.50f }},
	{{  76.56f,  77.85f,  78.00f,  78.50f }},
	{{  79.21f,  80.58f,  78.00f,  78.50f }},
	{{ 107.81f, 109.24f,  78.00f,  79.50f }},
	{{ 143.10f, 144.46f,  78.00f,  78.50f }},
	{{ 150.08f, 190.20f,  78.00f,  78.50f }},
	{{ 195.81f, 197.32f,  78.00f,  78.50f }},
	{{ 259.72f, 261.17f,  78.00f,  78.50f }},
	{{  12.33f,  17.34f,  78.50f,  79.00f }},
	{{  25.90f,  28.93f,  78.50f,  79.00f }},
	{{  39.04f,  40.82f,  78.50f,  79.00f }},
	{{  76.52f,  77.81f,  78.50f,  80.00f }},
	{{  79.15f,  80.52f,  78.50f,  79.00f }},
	{{ 142.94f, 144.25f,  78.50f,  79.00f }},
	{{ 149.39f, 158.32f,  78.50f,  79.00f }},
	{{ 185.18f, 190.90f,  78.50f,  79.00f }},
	{{ 195.96f, 197.45f,  78.50f,  79.00f }},
	{{ 231.16f, 232.51f,  78.50f,  80.00f }},
	{{ 259.78f, 261.23f,  78.50f,  79.00f }},
	{{ 295.81f, 297.16f,  78.50f,  80.00f }},
	{{   2.64f,   4.01f,  79.00f,  81.50f }},
	{{  11.93f,  16.29f,  79.00f,  79.50f }},
	{{  27.10f,  29.69f,  79.00f,  79.50f }},
	{{  39.49f,  41.16f,  79.00f,  79.50f }},
	{{  79.09f,  80.46f,  79.00f,  79.50f }},
	{{ 142.79f, 144.11f,  79.00f,  79.50f }},
	{{ 148.90f, 150.94f,  79.00f,  79.50f }},
	{{ 189.42f, 191.41f,  79.00f,  79.50f }},
	{{ 196.08f, 197.57f,  79.00f,  79.50f }},
	{{ 259.84f, 261.30f,  79.00f,  79.50f }},
	{{ 291.87f, 293.25f,  79.00f,  79.50f }},
	{{  11.52f,  15.23f,  79.50f,  80.00f }},
	{{  28.04f,  30.32f,  79.50f,  80.00f }},
	{{  39.94f,  41.49f,  79.50f,  80.00f }},
	{{  79.03f,  80.41f,  79.50f,  80.00f }},
	{{ 107.75f, 109.19f,  79.50f,  81.00f }},
	{{ 142.63f, 143.98f,  79.50f,  80.00f }},
	{{ 148.58f, 150.33f,  79.50f,  80.00f }},
	{{ 190.06f, 191.76f,  79.50f,  80.00f }},
	{{ 196.20f, 197.69f,  79.50f,  80.00f }},
	{{ 259.90f, 261.37f,  79.50f,  80.00f }},
	{{ 276.65f, 278.06f,  79.50f,  82.00f }},
	{{ 291.88f, 293.27f,  79.50f,  82.00f }},
	{{  11.12f,  14.44f,  80.00f,  80.50f }},
	{{  28.86f,  30.95f,  80.00f,  80.50f }},
	{{  40.36f,  41.83f,  80.00f,  80.50f }},
	{{  76.42f,  77.72f,  80.00f,  81.00f }},
	{{  78.96f,  80.35f,  80.00f,  80.50f }},
	{{ 142.53f, 143.90f,  80.00f,  80.50f }},
	{{ 148.32f, 149.90f,  80.00f,  80.50f }},
	{{ 190.48f, 192.02f,  80.00f,  80.50f }},
	{{ 196.33f, 197.79f,  80.00f,  80.50f }},
	{{ 231.21f, 232.56f,  80.00f,  81.50f }},
	{{ 259.97f, 261.43f,  80.00f,  80.50f }},
	{{ 295.87f, 297.22f,  80.00f,  81.00f }},
	{{  10.82f,  13.82f,  80.50f,  81.00f }},
	{{  29.58f,  31.53f,  80.50f,  81.00f }},
	{{  40.64f,  42.16f,  80.50f,  81.00f }},
	{{  78.90f,  80.29f,  80.50f,  81.00f }},
	{{ 142.42f, 143.81f,  80.50f,  81.00f }},
	{{ 148.13f, 149.61f,  80.50f,  81.00f }},
	{{ 190.77f, 192.23f,  80.50f,  81.00f }},
	{{ 196.45f, 197.87f,  80.50f,  81.50f }},
	{{ 260.03f, 261.50f,  80.50f,  81.00f }},
	{{  10.54f,  13.20f,  81.00f,  81.50f }},
	{{  30.20f,  31.98f,  81.00f,  81.50f }},
	{{  40.92f,  42.50f,  81.00f,  81.50f }},
	{{  76.35f,  77.66f,  81.00f,  81.50f }},
	{{  78.84f,  80.22f,  81.00f,  82.00f }},
	{{ 107.68f, 109.13f,  81.00f,  82.00f }},
	{{ 142.32f, 143.75f,  81.00f,  81.50f }},
	{{ 148.02f, 149.42f,  81.00f,  81.50f }},
	{{ 190.94f, 192.32f,  81.00f,  81.50f }},
	{{ 260.10f, 261.57f,  81.00f,  81.50f }},
	{{ 295.91f, 297.26f,  81.00f,  82.00f }},
	{{   2.57f,   3.94f,  81.50f,  82.50f }},
	{{  10.25f,  12.58f,  81.50f,  82.00f }},
	{{  30.74f,  32.44f,  81.50f,  82.00f }},
	{{  41.21f,  42.81f,  81.50f,  82.00f }},
	{{  76.31f,  77.63f,  81.50f,  82.50f }},
	{{ 142.23f, 143.70f,  81.50f,  82.50f }},
	{{ 147.95f, 149.33f,  81.50f,  82.50f }},
	{{ 191.08f, 192.39f,  81.50f,  82.00f }},
	{{ 196.59f, 197.98f,  81.50f,  82.00f }},
	{{ 231.26f, 232.62f,  81.50f,  82.50f }},
	{{ 260.16f, 261.63f,  81.50f,  82.00f }},
	{{   9.97f,  12.06f,  82.00f,  82.50f }},
	{{  31.27f,  32.89f,  82.00f,  82.50f }},
	{{  41.49f,  43.01f,  82.00f,  82.50f }},
	{{  78.72f,  80.09f,  82.00f,  82.50f }},
	{{ 107.64f, 109.09f,  82.00f,  83.50f }},
	{{ 191.16f, 192.45f,  82.00f,  83.00f }},
	{{ 196.65f, 198.02f,  82.00f,  82.50f }},
	{{ 260.23f, 261.70f,  82.00f,  82.50f }},
	{{ 276.75f, 278.15f,  82.00f,  82.50f }},
	{{ 291.97f, 293.37f,  82.00f,  84.50f }},
	{{ 295.95f, 297.30f,  82.00f,  83.50f }},
	{{   2.54f,   3.91f,  82.50f,  85.00f }},
	{{   9.68f,  11.69f,  82.50f,  83.00f }},
	{{  31.69f,  33.30f,  82.50f,  83.00f }},
	{{  41.77f,  43.21f,  82.50f,  83.00f }},
	{{  76.24f,  77.57f,  82.50f,  83.00f }},
	{{  78.65f,  80.02f,  82.50f,  83.00f }},
	{{ 142.11f, 143.61f,  82.50f,  83.50f }},
	{{ 147.79f, 149.19f,  82.50f,  83.00f }},
	{{ 196.72f, 198.06f,  82.50f,  83.00f }},
	{{ 231.30f, 232.66f,  82.50f,  85.00f }},
	{{ 260.29f, 261.76f,  82.50f,  83.00f }},
	{{ 276.76f, 278.17f,  82.50f,  85.00f }},
	{{   9.40f,  11.32f,  83.00f,  83.50f }},
	{{  32.11f,  33.61f,  83.00f,  83.50f }},
	{{  42.04f,  43.41f,  83.00f,  83.50f }},
	{{  76.21f,  77.53f,  83.00f,  84.00f }},
	{{  78.59f,  79.95f,  83.00f,  83.50f }},
	{{ 147.74f, 149.14f,  83.00f,  84.50f }},
	{{ 191.27f, 192.56f,  83.00f,  84.00f }},
	{{ 196.78f, 198.09f,  83.00f,  84.00f }},
	{{ 260.36f, 261.82f,  83.00f,  83.50f }},
	{{   9.17f,  10.95f,  83.50f,  84.00f }},
	{{  32.47f,  33.92f,  83.50f,  84.00f }},
	{{  42.19f,  43.62f,  83.50f,  84.00f }},
	{{  78.53f,  79.88f,  83.50f,  84.00f }},
	{{ 107.57f, 109.03f,  83.50f,  84.00f }},
	{{ 142.00f, 143.52f,  83.50f,  84.50f }},
	{{ 260.42f, 261.89f,  83.50f,  84.00f }},
	{{ 296.01f, 297.36f,  83.50f,  85.00f }},
	{{   6.12f,   7.55f,  84.00f,  87.50f }},
	{{   8.99f,  10.68f,  84.00f,  84.50f }},
	{{  32.78f,  34.23f,  84.00f,  84.50f }},
	{{  42.34f,  43.75f,  84.00f,  84.50f }},
	{{  76.14f,  77.47f,  84.00f,  84.50f }},
	{{  78.48f,  79.81f,  84.00f,  84.50f }},
	{{ 107.55f, 109.01f,  84.00f,  86.00f }},
	{{ 191.36f, 192.65f,  84.00f,  84.50f }},
	{{ 196.90f, 198.18f,  84.00f,  84.50f }},
	{{ 260.49f, 261.95f,  84.00f,  84.50f }},
	{{   8.81f,  10.45f,  84.50f,  85.00f }},
	{{  33.09f,  34.53f,  84.50f,  85.00f }},
	{{  42.49f,  43.87f,  84.50f,  85.00f }},
	{{  49.89f,  52.41f,  84.50f,  85.00f }},
	{{  60.83f,  63.50f,  84.50f,  85.00f }},
	{{  76.11f,  77.43f,  84.50f,  85.50f }},
	{{  78.43f,  79.74f,  84.50f,  85.50f }},
	{{ 141.89f, 143.43f,  84.50f,  85.00f }},
	{{ 147.60f, 149.01f,  84.50f,  85.50f }},
	{{ 191.40f, 192.69f,  84.50f,  85.50f }},
	{{ 196.97f, 198.24f,  84.50f,  85.00f }},
	{{ 260.55f, 261.99f,  84.50f,  85.50f }},
	{{ 292.06f, 293.46f,  84.50f,  87.00f }},
	{{   2.48f,   3.84f,  85.00f,  86.50f }},
	{{   8.64f,  10.23f,  85.00f,  85.50f }},
	{{  33.33f,  34.70f,  85.00f,  85.50f }},
	{{  42.64f,  44.00f,  85.00f,  85.50f }},
	{{  47.99f,  54.16f,  85.00f,  85.50f }},
	{{  58.60f,  64.80f,  85.00f,  85.50f }},
	{{ 141.80f, 143.38f,  85.00f,  85.50f }},
	{{ 197.02f, 198.37f,  85.00f,  85.50f }},
	{{ 231.33f, 232.77f,  85.00f,  86.50f }},
	{{ 276.86f, 278.26f,  85.00f,  87.50f }},
	{{ 296.07f, 297.42f,  85.00f,  86.00f }},
	{{   8.46f,  10.01f,  85.50f,  86.00f }},
	{{  33.52f,  34.88f,  85.50f,  86.00f }},
	{{  42.79f,  44.13f,  85.50f,  86.00f }},
	{{  47.51f,  54.65f,  85.50f,  86.00f }},
	{{  58.09f,  65.39f,  85.50f,  86.00f }},
	{{  76.05f,  77.36f,  85.50f,  86.00f }},
	{{  78.34f,  79.66f,  85.50f,  86.50f }},
	{{ 141.33f, 143.33f,  85.50f,  86.00f }},
	{{ 147.51f, 148.93f,  85.50f,  86.50f }},
	{{ 191.48f, 192.78f,  85.50f,  86.50f }},
	{{ 197.06f, 199.35f,  85.50f,  86.00f }},
	{{ 260.66f, 262.04f,  85.50f,  86.00f }},
	{{   8.29f,   9.79f,  86.00f,  86.50f }},
	{{  33.70f,  35.05f,  86.00f,  86.50f }},
	{{  42.94f,  44.24f,  86.00f,  86.50f }},
	{{  47.29f,  48.58f,  86.00f,  86.50f }},
	{{  53.63f,  54.82f,  86.00f,  86.50f }},
	{{  57.98f,  59.14f,  86.00f,  86.50f }},
	{{  64.33f,  65.73f,  86.00f,  86.50f }},
	{{  76.02f,  77.33f,  86.00f,  87.00f }},
	{{ 107.42f, 108.96f,  86.00f,  87.00f }},
	{{ 139.57f, 143.27f,  86.00f,  86.50f }},
	{{ 197.11f, 200.96f,  86.00f,  86.50f }},
	{{ 260.64f, 262.07f,  86.00f,  86.50f }},
	{{ 296.11f, 297.46f,  86.00f,  87.50f }},
	{{   2.44f,   3.80f,  86.50f,  88.50f }},
	{{   8.11f,   9.67f,  86.50f,  87.00f }},
	{{  33.86f,  35.22f,  86.50f,  87.00f }},
	{{  43.03f,  44.30f,  86.50f,  87.00f }},
	{{  47.26f,  48.37f,  86.50f,  87.00f }},
	{{  53.80f,  54.93f,  86.50f,  87.00f }},
	{{  57.94f,  58.99f,  86.50f,  87.00f }},
	{{  64.47f,  65.73f,  86.50f,  88.00f }},
	{{  78.43f,  79.89f,  86.50f,  87.00f }},
	{{ 137.99f, 143.22f,  86.50f,  87.00f }},
	{{ 147.42f, 148.85f,  86.50f,  87.50f }},
	{{ 191.55f, 192.86f,  86.50f,  87.00f }},
	{{ 197.15f, 202.40f,  86.50f,  87.00f }},
	{{ 231.25f, 232.87f,  86.50f,  87.00f }},
	{{ 260.43f, 262.03f,  86.50f,  87.00f }},
	{{   7.98f,   9.55f,  87.00f,  87.50f }},
	{{  34.00f,  35.32f,  87.00f,  87.50f }},
	{{  43.06f,  44.37f,  87.00f,  88.00f }},
	{{  47.22f,  48.32f,  87.00f,  88.50f }},
	{{  53.85f,  54.96f,  87.00f,  87.50f }},
	{{  57.90f,  58.93f,  87.00f,  89.00f }},
	{{  75.95f,  77.24f,  87.00f,  87.50f }},
	{{  78.70f,  80.36f,  87.00f,  87.50f }},
	{{ 107.34f, 115.76f,  87.00f,  87.50f }},
	{{ 136.43f, 140.46f,  87.00f,  87.50f }},
	{{ 141.87f, 143.16f,  87.00f,  87.50f }},
	{{ 191.59f, 192.91f,  87.00f,  88.00f }},
	{{ 197.19f, 198.69f,  87.00f,  87.50f }},
	{{ 199.80f, 203.81f,  87.00f,  87.50f }},
	{{ 224.56f, 232.92f,  87.00f,  87.50f }},
	{{ 259.91f, 261.99f,  87.00f,  87.50f }},
	{{ 292.17f, 293.55f,  87.00f,  89.00f }},
	{{   6.11f,   7.68f,  87.50f,  88.00f }},
	{{   7.77f,   9.43f,  87.50f,  88.00f }},
	{{  34.13f,  35.40f,  87.50f,  88.00f }},
	{{  53.88f,  54.99f,  87.50f,  88.50f }},
	{{  75.92f,  77.21f,  87.50f,  88.50f }},
	{{  79.10f,  80.97f,  87.50f,  88.00f }},
	{{ 107.30f, 139.12f,  87.50f,  88.00f }},
	{{ 141.83f, 143.11f,  87.50f,  88.50f }},
	{{ 147.34f, 148.77f,  87.50f,  88.00f }},
	{{ 197.23f, 198.60f,  87.50f,  88.00f }},
	{{ 201.04f, 233.00f,  87.50f,  88.00f }},
	{{ 259.27f, 261.74f,  87.50f,  88.00f }},
	{{ 276.95f, 278.34f,  87.50f,  88.00f }},
	{{ 296.16f, 297.53f,  87.50f,  88.00f }},
	{{   6.11f,   9.31f,  88.00f,  88.50f }},
	{{  34.24f,  35.48f,  88.00f,  88.50f }},
	{{  43.12f,  44.50f,  88.00f,  89.00f }},
	{{  64.56f,  65.73f,  88.00f,  97.00f }},
	{{  79.63f,  81.84f,  88.00f,  88.50f }},
	{{ 106.93f, 138.03f,  88.00f,  88.50f }},
	{{ 147.30f, 148.73f,  88.00f,  89.00f }},
	{{ 191.66f, 192.99f,  88.00f,  89.00f }},
	{{ 197.27f, 198.62f,  88.00f,  89.00f }},
	{{ 202.22f, 210.07f,  88.00f,  88.50f }},
	{{ 218.81f, 233.48f,  88.00f,  88.50f }},
	{{ 258.23f, 260.92f,  88.00f,  88.50f }},
	{{ 276.97f, 278.36f,  88.00f,  90.50f }},
	{{ 296.18f, 297.55f,  88.00f,  90.00f }},
	{{   2.39f,   3.74f,  88.50f,  90.50f }},
	{{   6.11f,   9.19f,  88.50f,  90.00f }},
	{{  20.43f,  22.73f,  88.50f,  89.00f }},
	{{  34.34f,  35.56f,  88.50f,  89.00f }},
	{{  47.12f,  48.25f,  88.50f,  89.00f }},
	{{  53.91f,  55.05f,  88.50f,  97.00f }},
	{{  75.86f,  77.13f,  88.50f,  89.00f }},
	{{  80.28f,  83.17f,  88.50f,  89.00f }},
	{{ 106.18f, 108.40f,  88.50f,  89.00f }},
	{{ 134.05f, 136.94f,  88.50f,  89.00f }},
	{{ 141.77f, 143.01f,  88.50f,  89.00f }},
	{{ 203.36f, 206.31f,  88.50f,  89.00f }},
	{{ 232.00f, 234.19f,  88.50f,  89.00f }},
	{{ 257.02f, 260.14f,  88.50f,  89.00f }},
	{{  19.83f,  23.26f,  89.00f,  89.50f }},
	{{  34.43f,  35.64f,  89.00f,  89.50f }},
	{{  43.18f,  44.52f,  89.00f,  90.50f }},
	{{  47.11f,  48.24f,  89.00f,  96.50f }},
	{{  57.84f,  58.89f,  89.00f,  98.00f }},
	{{  75.83f,  77.09f,  89.00f,  90.00f }},
	{{  81.11f,  84.91f,  89.00f,  89.50f }},
	{{ 105.17f, 107.78f,  89.00f,  89.50f }},
	{{ 133.14f, 135.94f,  89.00f,  89.50f }},
	{{ 141.73f, 142.96f,  89.00f,  89.50f }},
	{{ 147.22f, 148.65f,  89.00f,  89.50f }},
	{{ 191.74f, 193.07f,  89.00f,  89.50f }},
	{{ 197.35f, 198.66f,  89.00f,  90.00f }},
	{{ 204.39f, 207.11f,  89.00f,  89.50f }},
	{{ 232.52f, 235.31f,  89.00f,  89.50f }},
	{{ 255.53f, 259.43f,  89.00f,  89.50f }},
	{{ 292.26f, 293.62f,  89.00f,  90.00f }},
	{{  19.52f,  23.63f,  89.50f,  90.00f }},
	{{  34.48f,  35.64f,  89.50f,  90.50f }},
	{{  81.65f,  86.95f,  89.50f,  90.00f }},
	{{ 103.64f, 107.03f,  89.50f,  90.00f }},
	{{ 132.27f, 134.98f,  89.50f,  90.00f }},
	{{ 141.68f, 142.91f,  89.50f,  90.00f }},
	{{ 147.18f, 148.62f,  89.50f,  90.00f }},
	{{ 191.79f, 193.11f,  89.50f,  90.00f }},
	{{ 205.41f, 208.00f,  89.50f,  90.00f }},
	{{ 233.19f, 236.90f,  89.50f,  90.00f }},
	{{ 253.48f, 258.90f,  89.50f,  90.00f }},
	{{   6.10f,   9.14f,  90.00f,  91.50f }},
	{{  19.40f,  23.81f,  90.00f,  91.00f }},
	{{  75.77f,  77.02f,  90.00f,  90.50f }},
	{{  81.63f,  89.70f,  90.00f,  90.50f }},
	{{ 101.13f, 106.12f,  90.00f,  90.50f }},
	{{ 131.41f, 134.02f,  90.00f,  90.50f }},
	{{ 141.63f, 142.87f,  90.00f,  90.50f }},
	{{ 147.14f, 148.58f,  90.00f,  90.50f }},
	{{ 191.83f, 193.15f,  90.00f,  90.50f }},
	{{ 197.44f, 198.80f,  90.00f,  90.50f }},
	{{ 206.33f, 208.90f,  90.00f,  90.50f }},
	{{ 234.02f, 239.14f,  90.00f,  90.50f }},
	{{ 250.47f, 258.97f,  90.00f,  90.50f }},
	{{ 292.30f, 293.65f,  90.00f,  91.50f }},
	{{ 296.26f, 297.64f,  90.00f,  90.50f }},
	{{   2.35f,   3.68f,  90.50f,  91.50f }},
	{{  34.48f,  35.65f,  90.50f,  91.00f }},
	{{  43.10f,  44.52f,  90.50f,  91.00f }},
	{{  49.74f,  52.63f,  90.50f,  91.00f }},
	{{  60.12f,  63.26f,  90.50f,  91.50f }},
	{{  75.74f,  76.98f,  90.50f,  91.00f }},
	{{  79.65f,  82.94f,  90.50f,  91.00f }},
	{{  85.21f, 105.14f,  90.50f,  91.00f }},
	{{ 130.56f, 133.12f,  90.50f,  91.00f }},
	{{ 141.56f, 142.82f,  90.50f,  91.00f }},
	{{ 147.10f, 148.54f,  90.50f,  91.50f }},
	{{ 191.88f, 193.20f,  90.50f,  91.50f }},
	{{ 197.48f, 198.87f,  90.50f,  91.00f }},
	{{ 207.25f, 209.76f,  90.50f,  91.00f }},
	{{ 235.32f, 255.11f,  90.50f,  91.00f }},
	{{ 257.58f, 262.07f,  90.50f,  91.00f }},
	{{ 277.07f, 278.44f,  90.50f,  91.00f }},
	{{ 296.28f, 297.66f,  90.50f,  93.00f }},
	{{  19.61f,  23.59f,  91.00f,  91.50f }},
	{{  34.43f,  35.66f,  91.00f,  91.50f }},
	{{  43.02f,  44.45f,  91.00f,  91.50f }},
	{{  49.54f,  52.76f,  91.00f,  91.50f }},
	{{  75.71f,  76.94f,  91.00f,  92.00f }},
	{{  78.75f,  82.91f,  91.00f,  91.50f }},
	{{  87.07f, 104.29f,  91.00f,  91.50f }},
	{{ 129.72f, 132.25f,  91.00f,  91.50f }},
	{{ 141.46f, 142.78f,  91.00f,  91.50f }},
	{{ 197.52f, 198.97f,  91.00f,  91.50f }},
	{{ 208.12f, 210.62f,  91.00f,  91.50f }},
	{{ 236.03f, 253.00f,  91.00f,  91.50f }},
	{{ 257.60f, 262.66f,  91.00f,  91.50f }},
	{{ 277.09f, 278.46f,  91.00f,  93.00f }},
	{{   2.32f,   3.65f,  91.50f,  95.00f }},
	{{   6.10f,   9.20f,  91.50f,  92.00f }},
	{{  20.03f,  23.17f,  91.50f,  92.00f }},
	{{  34.37f,  35.65f,  91.50f,  92.00f }},
	{{  42.94f,  44.39f,  91.50f,  92.00f }},
	{{  60.99f,  62.35f,  91.50f,  92.00f }},
	{{  78.98f,  82.89f,  91.50f,  92.00f }},
	{{  87.51f,  89.19f,  91.50f,  92.00f }},
	{{  91.69f, 100.15f,  91.50f,  92.00f }},
	{{ 102.89f, 104.24f,  91.50f,  92.00f }},
	{{ 128.88f, 131.38f,  91.50f,  92.00f }},
	{{ 141.24f, 142.74f,  91.50f,  92.00f }},
	{{ 147.03f, 148.47f,  91.50f,  92.00f }},
	{{ 191.97f, 193.28f,  91.50f,  92.50f }},
	{{ 197.56f, 199.17f,  91.50f,  92.00f }},
	{{ 208.96f, 211.46f,  91.50f,  92.00f }},
	{{ 236.15f, 237.62f,  91.50f,  92.00f }},
	{{ 240.38f, 249.05f,  91.50f,  92.00f }},
	{{ 251.30f, 252.82f,  91.50f,  92.00f }},
	{{ 257.62f, 259.23f,  91.50f,  92.00f }},
	{{ 261.17f, 262.85f,  91.50f,  92.00f }},
	{{ 292.35f, 293.71f,  91.50f,  93.00f }},
	{{   6.10f,   9.29f,  92.00f,  92.50f }},
	{{  20.50f,  22.72f,  92.00f,  92.50f }},
	{{  34.28f,  35.57f,  92.00f,  92.50f }},
	{{  42.86f,  44.32f,  92.00f,  92.50f }},
	{{  75.64f,  76.88f,  92.00f,  92.50f }},
	{{  81.24f,  82.88f,  92.00f,  92.50f }},
	{{  87.51f,  89.03f,  92.00f,  95.50f }},
	{{  96.57f,  98.74f,  92.00f,  92.50f }},
	{{ 102.97f, 104.26f,  92.00f,  93.00f }},
	{{ 128.07f, 130.53f,  92.00f,  92.50f }},
	{{ 140.51f, 142.70f,  92.00f,  92.50f }},
	{{ 146.99f, 148.43f,  92.00f,  92.50f }},
	{{ 197.60f, 199.79f,  92.00f,  92.50f }},
	{{ 209.80f, 212.29f,  92.00f,  92.50f }},
	{{ 236.13f, 237.37f,  92.00f,  92.50f }},
	{{ 241.79f, 243.30f,  92.00f,  92.50f }},
	{{ 251.31f, 252.73f,  92.00f,  93.50f }},
	{{ 257.63f, 259.06f,  92.00f,  93.00f }},
	{{ 261.69f, 262.97f,  92.00f,  92.50f }},
	{{   6.10f,   9.39f,  92.50f,  93.00f }},
	{{  34.20f,  35.49f,  92.50f,  93.00f }},
	{{  42.78f,  44.22f,  92.50f,  93.00f }},
	{{  60.19f,  63.17f,  92.50f,  93.00f }},
	{{  75.61f,  76.84f,  92.50f,  93.50f }},
	{{  80.97f,  82.87f,  92.50f,  93.00f }},
	{{  97.08f,  98.75f,  92.50f,  93.00f }},
	{{ 127.27f, 129.73f,  92.50f,  93.00f }},
	{{ 139.35f, 142.66f,  92.50f,  93.00f }},
	{{ 146.95f, 148.39f,  92.50f,  93.50f }},
	{{ 192.05f, 193.37f,  92.50f,  93.50f }},
	{{ 197.64f, 200.93f,  92.50f,  93.00f }},
	{{ 210.61f, 213.12f,  92.50f,  93.00f }},
	{{ 236.08f, 237.34f,  92.50f,  93.00f }},
	{{ 241.83f, 243.18f,  92.50f,  93.00f }},
	{{ 261.79f, 263.07f,  92.50f,  93.00f }},
	{{   6.10f,   7.63f,  93.00f,  96.50f }},
	{{   7.91f,   9.48f,  93.00f,  93.50f }},
	{{  34.11f,  35.41f,  93.00f,  93.50f }},
	{{  42.70f,  44.12f,  93.00f,  93.50f }},
	{{  49.38f,  52.98f,  93.00f,  93.50f }},
	{{  59.88f,  63.58f,  93.00f,  93.50f }},
	{{  80.82f,  82.85f,  93.00f,  93.50f }},
	{{  97.29f,  98.76f,  93.00f,  93.50f }},
	{{ 103.07f, 104.35f,  93.00f,  93.50f }},
	{{ 126.49f, 128.93f,  93.00f,  93.50f }},
	{{ 138.32f, 142.61f,  93.00f,  93.50f }},
	{{ 197.69f, 202.06f,  93.00f,  93.50f }},
	{{ 211.43f, 213.94f,  93.00f,  93.50f }},
	{{ 236.03f, 237.32f,  93.00f,  93.50f }},
	{{ 241.78f, 243.10f,  93.00f,  93.50f }},
	{{ 257.65f, 259.04f,  93.00f,  93.50f }},
	{{ 261.89f, 263.16f,  93.00f,  93.50f }},
	{{ 277.16f, 278.53f,  93.00f,  94.00f }},
	{{ 292.41f, 293.76f,  93.00f,  94.50f }},
	{{ 296.37f, 297.76f,  93.00f,  95.50f }},
	{{   8.07f,   9.57f,  93.50f,  94.00f }},
	{{  33.98f,  35.34f,  93.50f,  94.00f }},
	{{  42.60f,  44.01f,  93.50f,  94.00f }},
	{{  75.54f,  76.78f,  93.50f,  94.00f }},
	{{  80.73f,  82.85f,  93.50f,  94.00f }},
	{{  97.37f,  98.77f,  93.50f,  94.50f }},
	{{ 103.13f, 104.42f,  93.50f,  94.00f }},
	{{ 125.71f, 128.13f,  93.50f,  94.00f }},
	{{ 137.28f, 140.20f,  93.50f,  94.00f }},
	{{ 141.09f, 142.56f,  93.50f,  94.00f }},
	{{ 146.87f, 148.31f,  93.50f,  94.00f }},
	{{ 192.14f, 193.46f,  93.50f,  94.00f }},
	{{ 197.73f, 199.32f,  93.50f,  94.00f }},
	{{ 200.14f, 203.17f,  93.50f,  94.00f }},
	{{ 212.25f, 214.75f,  93.50f,  94.00f }},
	{{ 235.94f, 237.27f,  93.50f,  94.00f }},
	{{ 241.74f, 243.05f,  93.50f,  94.50f }},
	{{ 251.35f, 252.67f,  93.50f,  94.00f }},
	{{ 257.65f, 259.04f,  93.50f,  96.00f }},
	{{ 261.99f, 263.25f,  93.50f,  94.00f }},
	{{   8.24f,   9.74f,  94.00f,  94.50f }},
	{{  33.84f,  35.18f,  94.00f,  94.50f }},
	{{  42.40f,  43.88f,  94.00f,  94.50f }},
	{{  75.50f,  76.74f,  94.00f,  95.00f }},
	{{  80.65f,  82.84f,  94.00f,  95.00f }},
	{{ 103.20f, 104.50f,  94.00f,  94.50f }},
	{{ 124.93f, 127.34f,  94.00f,  94.50f }},
	{{ 136.30f, 138.87f,  94.00f,  94.50f }},
	{{ 141.19f, 142.51f,  94.00f,  95.00f }},
	{{ 146.83f, 148.27f,  94.00f,  94.50f }},
	{{ 192.18f, 193.50f,  94.00f,  95.00f }},
	{{ 197.77f, 199.27f,  94.00f,  95.00f }},
	{{ 201.36f, 204.21f,  94.00f,  94.50f }},
	{{ 213.07f, 215.56f,  94.00f,  94.50f }},
	{{ 235.86f, 237.22f,  94.00f,  94.50f }},
	{{ 251.37f, 252.64f,  94.00f,  97.50f }},
	{{ 262.07f, 263.34f,  94.00f,  94.50f }},
	{{ 277.20f, 278.56f,  94.00f,  95.50f }},
	{{   8.42f,   9.93f,  94.50f,  95.00f }},
	{{  33.70f,  34.97f,  94.50f,  95.00f }},
	{{  42.20f,  43.75f,  94.50f,  95.00f }},
	{{  97.50f,  98.87f,  94.50f,  95.00f }},
	{{ 103.27f, 104.58f,  94.50f,  95.00f }},
	{{ 124.15f, 126.55f,  94.50f,  95.00f }},
	{{ 135.32f, 137.81f,  94.50f,  95.00f }},
	{{ 146.79f, 148.22f,  94.50f,  95.50f }},
	{{ 202.40f, 205.21f,  94.50f,  95.00f }},
	{{ 213.90f, 216.37f,  94.50f,  95.00f }},
	{{ 235.77f, 237.16f,  94.50f,  95.00f }},
	{{ 241.61f, 242.93f,  94.50f,  95.00f }},
	{{ 262.15f, 263.43f,  94.50f,  95.00f }},
	{{ 292.46f, 293.81f,  94.50f,  96.00f }},
	{{   2.24f,   3.54f,  95.00f,  98.50f }},
	{{   8.61f,  10.13f,  95.00f,  95.50f }},
	{{  33.49f,  34.77f,  95.00f,  95.50f }},
	{{  42.00f,  43.56f,  95.00f,  95.50f }},
	{{  75.42f,  76.67f,  95.00f,  95.50f }},
	{{  80.52f,  82.82f,  95.00f,  96.00f }},
	{{  97.56f,  98.92f,  95.00f,  95.50f }},
	{{ 103.35f, 104.67f,  95.00f,  95.50f }},
	{{ 123.29f, 125.76f,  95.00f,  95.50f }},
	{{ 134.38f, 136.86f,  95.00f,  95.50f }},
	{{ 141.16f, 142.41f,  95.00f,  95.50f }},
	{{ 192.26f, 193.60f,  95.00f,  96.00f }},
	{{ 197.86f, 199.30f,  95.00f,  96.00f }},
	{{ 203.40f, 206.16f,  95.00f,  95.50f }},
	{{ 214.72f, 217.18f,  95.00f,  95.50f }},
	{{ 235.68f, 237.11f,  95.00f,  95.50f }},
	{{ 241.54f, 242.86f,  95.00f,  95.50f }},
	{{ 262.24f, 263.52f,  95.00f,  95.50f }},
	{{   8.80f,  10.33f,  95.50f,  96.00f }},
	{{  33.27f,  34.56f,  95.50f,  96.00f }},
	{{  41.80f,  43.33f,  95.50f,  96.00f }},
	{{  75.38f,  76.64f,  95.50f,  96.00f }},
	{{  87.52f,  88.94f,  95.50f, 100.00f }},
	{{  97.64f,  98.97f,  95.50f,  96.00f }},
	{{ 103.44f, 104.79f,  95.50f,  96.00f }},
	{{ 122.43f, 124.95f,  95.50f,  96.00f }},
	{{ 133.47f, 135.92f,  95.50f,  96.00f }},
	{{ 141.13f, 142.36f,  95.50f,  96.00f }},
	{{ 146.71f, 148.12f,  95.50f,  96.50f }},
	{{ 204.40f, 207.08f,  95.50f,  96.00f }},
	{{ 215.54f, 218.02f,  95.50f,  96.00f }},
	{{ 235.60f, 237.05f,  95.50f,  96.00f }},
	{{ 241.47f, 242.80f,  95.50f,  96.00f }},
	{{ 262.32f, 263.61f,  95.50f,  96.00f }},
	{{ 277.26f, 278.60f,  95.50f,  97.50f }},
	{{ 296.46f, 297.85f,  95.50f,  98.50f }},
	{{   8.99f,  10.53f,  96.00f,  96.50f }},
	{{  33.04f,  34.36f,  96.00f,  96.50f }},
	{{  41.60f,  43.18f,  96.00f,  96.50f }},
	{{  67.82f,  69.15f,  96.00f, 122.00f }},
	{{  75.34f,  76.60f,  96.00f,  97.00f }},
	{{  80.41f,  82.81f,  96.00f,  97.00f }},
	{{  97.72f,  99.02f,  96.00f,  96.50f }},
	{{ 103.53f, 104.90f,  96.00f,  96.50f }},
	{{ 121.56f, 124.10f,  96.00f,  96.50f }},
	{{ 132.60f, 134.97f,  96.00f,  96.50f }},
	{{ 141.08f, 142.31f,  96.00f,  96.50f }},
	{{ 192.34f, 193.69f,  96.00f,  96.50f }},
	{{ 197.94f, 199.37f,  96.00f,  96.50f }},
	{{ 205.32f, 207.94f,  96.00f,  96.50f }},
	{{ 216.36f, 218.88f,  96.00f,  96.50f }},
	{{ 235.51f, 236.90f,  96.00f,  96.50f }},
	{{ 241.38f, 242.74f,  96.00f,  96.50f }},
	{{ 257.68f, 259.05f,  96.00f,  99.00f }},
	{{ 262.40f, 263.70f,  96.00f,  96.50f }},
	{{ 292.51f, 293.86f,  96.00f,  97.50f }},
	{{   6.08f,   7.55f,  96.50f, 107.50f }},
	{{   9.25f,  10.86f,  96.50f,  97.00f }},
	{{  32.74f,  34.15f,  96.50f,  97.00f }},
	{{  41.40f,  42.98f,  96.50f,  97.00f }},
	{{  47.15f,  48.25f,  96.50f,  97.00f }},
	{{  97.81f,  99.07f,  96.50f,  97.00f }},
	{{ 103.65f, 105.06f,  96.50f,  97.00f }},
	{{ 120.69f, 123.25f,  96.50f,  97.00f }},
	{{ 131.77f, 134.07f,  96.50f,  97.00f }},
	{{ 141.03f, 142.26f,  96.50f,  97.00f }},
	{{ 146.63f, 148.04f,  96.50f,  98.00f }},
	{{ 192.38f, 193.74f,  96.50f,  97.00f }},
	{{ 197.99f, 199.41f,  96.50f,  97.50f }},
	{{ 206.24f, 208.78f,  96.50f,  97.00f }},
	{{ 217.17f, 219.75f,  96.50f,  97.00f }},
	{{ 235.31f, 236.73f,  96.50f,  97.00f }},
	{{ 241.28f, 242.68f,  96.50f,  97.00f }},
	{{ 262.47f, 263.79f,  96.50f,  97.00f }},
	{{   9.55f,  11.19f,  97.00f,  97.50f }},
	{{  32.45f,  33.95f,  97.00f,  97.50f }},
	{{  41.13f,  42.70f,  97.00f,  97.50f }},
	{{  47.16f,  48.26f,  97.00f,  98.50f }},
	{{  53.89f,  55.05f,  97.00f,  98.00f }},
	{{  64.54f,  65.73f,  97.00f,  99.00f }},
	{{  75.26f,  76.53f,  97.00f,  97.50f }},
	{{  80.30f,  82.80f,  97.00f,  97.50f }},
	{{  97.89f,  99.20f,  97.00f,  97.50f }},
	{{ 103.78f, 105.23f,  97.00f,  97.50f }},
	{{ 119.71f, 122.40f,  97.00f,  97.50f }},
	{{ 130.95f, 133.22f,  97.00f,  97.50f }},
	{{ 140.98f, 142.21f,  97.00f,  97.50f }},
	{{ 192.42f, 193.79f,  97.00f,  97.50f }},
	{{ 207.17f, 209.57f,  97.00f,  97.50f }},
	{{ 218.03f, 220.67f,  97.00f,  97.50f }},
	{{ 235.10f, 236.56f,  97.00f,  97.50f }},
	{{ 241.18f, 242.57f,  97.00f,  97.50f }},
	{{ 262.54f, 263.87f,  97.00f,  97.50f }},
	{{   9.85f,  11.52f,  97.50f,  98.00f }},
	{{  32.10f,  33.67f,  97.50f,  98.00f }},
	{{  40.78f,  42.42f,  97.50f,  98.00f }},
	{{  75.22f,  76.49f,  97.50f,  98.50f }},
	{{  80.25f,  82.80f,  97.50f,  98.50f }},
	{{  97.98f,  99.33f,  97.50f,  98.00f }},
	{{ 104.00f, 105.45f,  97.50f,  98.00f }},
	{{ 118.73f, 121.48f,  97.50f,  98.00f }},
	{{ 130.13f, 132.38f,  97.50f,  98.00f }},
	{{ 140.93f, 142.16f,  97.50f,  98.00f }},
	{{ 192.46f, 193.84f,  97.50f,  98.00f }},
	{{ 198.08f, 199.48f,  97.50f,  98.50f }},
	{{ 208.00f, 210.36f,  97.50f,  98.00f }},
	{{ 218.88f, 221.60f,  97.50f,  98.00f }},
	{{ 234.89f, 236.38f,  97.50f,  98.00f }},
	{{ 241.07f, 242.46f,  97.50f,  98.00f }},
	{{ 251.46f, 252.63f,  97.50f, 105.00f }},
	{{ 262.62f, 263.96f,  97.50f,  98.00f }},
	{{ 277.33f, 278.66f,  97.50f,  98.50f }},
	{{ 292.56f, 293.91f,  97.50f,  99.00f }},
	{{  10.15f,  11.85f,  98.00f,  98.50f }},
	{{  31.69f,  33.27f,  98.00f,  98.50f }},
	{{  40.43f,  42.13f,  98.00f,  98.50f }},
	{{  53.84f,  55.02f,  98.00f,  99.00f }},
	{{  57.93f,  58.97f,  98.00f,  98.50f }},
	{{  98.06f,  99.46f,  98.00f,  98.50f }},
	{{ 104.23f, 105.70f,  98.00f,  98.50f }},
	{{ 117.75f, 120.57f,  98.00f,  98.50f }},
	{{ 129.32f, 131.57f,  98.00f,  98.50f }},
	{{ 140.87f, 142.12f,  98.00f,  98.50f }},
	{{ 146.52f, 147.92f,  98.00f,  99.00f }},
	{{ 192.50f, 193.88f,  98.00f,  99.00f }},
	{{ 208.83f, 211.15f,  98.00f,  98.50f }},
	{{ 219.79f, 222.48f,  98.00f,  98.50f }},
	{{ 234.68f, 236.13f,  98.00f,  98.50f }},
	{{ 240.89f, 242.35f,  98.00f,  98.50f }},
	{{ 262.69f, 264.04f,  98.00f,  98.50f }},
	{{   2.16f,   3.44f,  98.50f,  99.00f }},
	{{  10.49f,  12.31f,  98.50f,  99.00f }},
	{{  31.29f,  32.87f,  98.50f,  99.00f }},
	{{  40.08f,  41.80f,  98.50f,  99.00f }},
	{{  47.28f,  48.37f,  98.50f,  99.00f }},
	{{  58.01f,  59.00f,  98.50f,  99.00f }},
	{{  75.14f,  76.42f,  98.50f,  99.50f }},
	{{  80.14f,  82.79f,  98.50f,  99.50f }},
	{{  98.22f,  99.59f,  98.50f,  99.00f }},
	{{ 104.56f, 106.11f,  98.50f,  99.00f }},
	{{ 116.56f, 119.62f,  98.50f,  99.00f }},
	{{ 128.52f, 130.77f,  98.50f,  99.00f }},
	{{ 140.82f, 142.07f,  98.50f,  99.00f }},
	{{ 198.16f, 199.56f,  98.50f,  99.50f }},
	{{ 209.65f, 211.99f,  98.50f,  99.00f }},
	{{ 220.73f, 223.54f,  98.50f,  99.00f }},
	{{ 234.19f, 235.79f,  98.50f,  99.00f }},
	{{ 240.70f, 242.25f,  98.50f,  99.00f }},
	{{ 262.73f, 264.12f,  98.50f,  99.00f }},
	{{ 277.36f, 278.69f,  98.50f, 100.50f }},
	{{ 296.56f, 297.96f,  98.50f, 101.00f }},
	{{   2.14f,   3.42f,  99.00f, 102.00f }},
	{{  10.92f,  12.81f,  99.00f,  99.50f }},
	{{  30.88f,  32.47f,  99.00f,  99.50f }},
	{{  39.72f,  41.48f,  99.00f,  99.50f }},
	{{  47.33f,  48.72f,  99.00f,  99.50f }},
	{{  53.60f,  54.90f,  99.00f,  99.50f }},
	{{  58.09f,  59.32f,  99.00f,  99.50f }},
	{{  64.14f,  65.72f,  99.00f,  99.50f }},
	{{  98.37f,  99.72f,  99.00f,  99.50f }},
	{{ 104.91f, 106.69f,  99.00f,  99.50f }},
	{{ 115.35f, 118.61f,  99.00f,  99.50f }},
	{{ 127.70f, 129.98f,  99.00f,  99.50f }},
	{{ 140.77f, 142.03f,  99.00f,  99.50f }},
	{{ 146.44f, 147.85f,  99.00f,  99.50f }},
	{{ 192.58f, 193.97f,  99.00f, 100.00f }},
	{{ 210.41f, 212.83f,  99.00f,  99.50f }},
	{{ 221.72f, 224.84f,  99.00f,  99.50f }},
	{{ 233.68f, 235.45f,  99.00f,  99.50f }},
	{{ 240.51f, 242.09f,  99.00f,  99.50f }},
	{{ 257.69f, 264.21f,  99.00f,  99.50f }},
	{{ 292.61f, 293.96f,  99.00f, 100.50f }},
	{{  11.36f,  13.31f,  99.50f, 100.00f }},
	{{  30.32f,  32.08f,  99.50f, 100.00f }},
	{{  39.37f,  41.14f,  99.50f, 100.00f }},
	{{  47.51f,  54.60f,  99.50f, 100.00f }},
	{{  58.21f,  65.27f,  99.50f, 100.00f }},
	{{  75.07f,  76.35f,  99.50f, 100.00f }},
	{{  80.03f,  81.22f,  99.50f, 100.00f }},
	{{  81.44f,  82.79f,  99.50f, 100.00f }},
	{{  98.52f,  99.91f,  99.50f, 100.00f }},
	{{ 105.40f, 107.69f,  99.50f, 100.00f }},
	{{ 113.64f, 117.56f,  99.50f, 100.00f }},
	{{ 126.83f, 129.19f,  99.50f, 100.00f }},
	{{ 140.72f, 141.99f,  99.50f, 100.00f }},
	{{ 146.40f, 147.82f,  99.50f, 100.50f }},
	{{ 198.25f, 199.65f,  99.50f, 100.00f }},
	{{ 211.23f, 213.66f,  99.50f, 100.00f }},
	{{ 222.78f, 226.30f,  99.50f, 100.00f }},
	{{ 232.74f, 235.10f,  99.50f, 100.00f }},
	{{ 240.32f, 241.91f,  99.50f, 100.00f }},
	{{ 257.69f, 264.29f,  99.50f, 100.00f }},
	{{  11.79f,  13.83f, 100.00f, 100.50f }},
	{{  29.70f,  31.68f, 100.00f, 100.50f }},
	{{  38.91f,  40.75f, 100.00f, 100.50f }},
	{{  48.10f,  53.75f, 100.00f, 100.50f }},
	{{  59.00f,  64.83f, 100.00f, 100.50f }},
	{{  75.03f,  76.31f, 100.00f, 101.00f }},
	{{  79.98f,  81.03f, 100.00f, 101.00f }},
	{{  81.51f,  82.79f, 100.00f, 101.00f }},
	{{  87.55f,  88.91f, 100.00f, 104.00f }},
	{{  98.67f, 100.14f, 100.00f, 100.50f }},
	{{ 105.95f, 109.91f, 100.00f, 100.50f }},
	{{ 110.78f, 116.34f, 100.00f, 100.50f }},
	{{ 125.97f, 128.40f, 100.00f, 100.50f }},
	{{ 140.67f, 141.95f, 100.00f, 100.50f }},
	{{ 192.65f, 194.06f, 100.00f, 101.50f }},
	{{ 198.30f, 199.69f, 100.00f, 101.00f }},
	{{ 212.07f, 214.50f, 100.00f, 100.50f }},
	{{ 223.95f, 234.44f, 100.00f, 100.50f }},
	{{ 240.13f, 241.73f, 100.00f, 100.50f }},
	{{ 257.69f, 260.79f, 100.00f, 100.50f }},
	{{ 262.91f, 264.38f, 100.00f, 100.50f }},
	{{  12.31f,  14.60f, 100.50f, 101.00f }},
	{{  29.07f,  31.15f, 100.50f, 101.00f }},
	{{  38.36f,  40.37f, 100.50f, 101.00f }},
	{{  98.91f, 100.37f, 100.50f, 101.00f }},
	{{ 106.69f, 114.91f, 100.50f, 101.00f }},
	{{ 125.11f, 127.61f, 100.50f, 101.00f }},
	{{ 140.62f, 141.91f, 100.50f, 101.50f }},
	{{ 146.32f, 147.74f, 100.50f, 101.50f }},
	{{ 212.90f, 215.34f, 100.50f, 101.00f }},
	{{ 225.35f, 233.77f, 100.50f, 101.00f }},
	{{ 239.95f, 241.54f, 100.50f, 101.00f }},
	{{ 257.69f, 259.15f, 100.50f, 102.50f }},
	{{ 263.08f, 264.46f, 100.50f, 101.00f }},
	{{ 277.43f, 278.76f, 100.50f, 101.50f }},
	{{ 292.66f, 294.01f, 100.50f, 101.50f }},
	{{  12.93f,  15.37f, 101.00f, 101.50f }},
	{{  28.18f,  30.44f, 101.00f, 101.50f }},
	{{  37.82f,  39.85f, 101.00f, 101.50f }},
	{{  74.96f,  76.23f, 101.00f, 101.50f }},
	{{  79.93f,  82.79f, 101.00f, 102.50f }},
	{{  99.18f, 100.60f, 101.00f, 101.50f }},
	{{ 107.70f, 112.69f, 101.00f, 101.50f }},
	{{ 124.25f, 126.79f, 101.00f, 101.50f }},
	{{ 198.39f, 199.78f, 101.00f, 102.00f }},
	{{ 213.74f, 216.18f, 101.00f, 101.50f }},
	{{ 227.31f, 232.37f, 101.00f, 101.50f }},
	{{ 239.76f, 241.27f, 101.00f, 101.50f }},
	{{ 263.19f, 264.55f, 101.00f, 101.50f }},
	{{ 296.65f, 298.05f, 101.00f, 101.50f }},
	{{  13.55f,  16.23f, 101.50f, 102.00f }},
	{{  27.17f,  29.74f, 101.50f, 102.00f }},
	{{  37.27f,  39.34f, 101.50f, 102.00f }},
	{{  74.92f,  76.19f, 101.50f, 102.50f }},
	{{  99.45f, 100.85f, 101.50f, 102.00f }},
	{{ 123.39f, 125.97f, 101.50f, 102.00f }},
	{{ 140.53f, 141.83f, 101.50f, 102.50f }},
	{{ 146.24f, 147.66f, 101.50f, 102.00f }},
	{{ 192.77f, 194.19f, 101.50f, 102.50f }},
	{{ 214.57f, 217.01f, 101.50f, 102.00f }},
	{{ 239.35f, 240.99f, 101.50f, 102.00f }},
	{{ 263.28f, 264.64f, 101.50f, 102.00f }},
	{{ 277.46f, 278.78f, 101.50f, 104.00f }},
	{{ 292.69f, 294.05f, 101.50f, 103.50f }},
	{{ 296.67f, 298.06f, 101.50f, 103.50f }},
	{{   2.07f,   3.34f, 102.00f, 103.50f }},
	{{  14.21f,  17.59f, 102.00f, 102.50f }},
	{{  25.80f,  29.03f, 102.00f, 102.50f }},
	{{  36.73f,  38.80f, 102.00f, 102.50f }},
	{{  99.76f, 101.25f, 102.00f, 102.50f }},
	{{ 122.50f, 125.10f, 102.00f, 102.50f }},
	{{ 146.20f, 147.61f, 102.00f, 103.00f }},
	{{ 198.48f, 199.87f, 102.00f, 103.00f }},
	{{ 215.48f, 218.05f, 102.00f, 102.50f }},
	{{ 238.92f, 240.71f, 102.00f, 102.50f }},
	{{ 263.36f, 264.72f, 102.00f, 102.50f }},
	{{  14.97f,  19.58f, 102.50f, 103.00f }},
	{{  24.02f,  28.29f, 102.50f, 103.00f }},
	{{  36.06f,  38.18f, 102.50f, 103.00f }},
	{{  74.85f,  76.11f, 102.50f, 103.50f }},
	{{  80.01f,  80.73f, 102.50f, 103.00f }},
	{{  81.32f,  82.79f, 102.50f, 103.00f }},
	{{ 100.18f, 101.66f, 102.50f, 103.00f }},
	{{ 121.41f, 124.21f, 102.50f, 103.00f }},
	{{ 140.44f, 141.75f, 102.50f, 103.50f }},
	{{ 192.85f, 194.27f, 102.50f, 103.50f }},
	{{ 216.39f, 219.10f, 102.50f, 103.00f }},
	{{ 238.48f, 240.29f, 102.50f, 103.00f }},
	{{ 257.68f, 261.20f, 102.50f, 103.00f }},
	{{ 263.43f, 264.81f, 102.50f, 103.00f }},
	{{  15.79f,  26.87f, 103.00f, 103.50f }},
	{{  35.19f,  37.57f, 103.00f, 103.50f }},
	{{  80.31f,  80.34f, 103.00f, 103.50f }},
	{{  81.43f,  82.79f, 103.00f, 103.50f }},
	{{ 100.60f, 102.07f, 103.00f, 103.50f }},
	{{ 120.31f, 123.26f, 103.00f, 103.50f }},
	{{ 146.12f, 147.52f, 103.00f, 104.00f }},
	{{ 198.57f, 199.96f, 103.00f, 104.00f }},
	{{ 217.30f, 220.15f, 103.00f, 103.50f }},
	{{ 238.04f, 239.86f, 103.00f, 103.50f }},
	{{ 257.67f, 262.09f, 103.00f, 104.00f }},
	{{ 263.51f, 264.89f, 103.00f, 103.50f }},
	{{   2.04f,   3.30f, 103.50f, 105.50f }},
	{{  16.69f,  25.44f, 103.50f, 104.00f }},
	{{  34.32f,  36.88f, 103.50f, 104.00f }},
	{{  74.78f,  76.03f, 103.50f, 104.00f }},
	{{  81.48f,  82.79f, 103.50f, 104.50f }},
	{{ 101.03f, 102.76f, 103.50f, 104.00f }},
	{{ 119.21f, 122.30f, 103.50f, 104.00f }},
	{{ 140.35f, 141.66f, 103.50f, 104.00f }},
	{{ 192.94f, 194.36f, 103.50f, 104.00f }},
	{{ 218.20f, 221.20f, 103.50f, 104.00f }},
	{{ 237.61f, 239.42f, 103.50f, 104.00f }},
	{{ 263.59f, 264.98f, 103.50f, 104.00f }},
	{{ 292.77f, 294.13f, 103.50f, 104.50f }},
	{{ 296.75f, 298.13f, 103.50f, 104.50f }},
	{{  17.51f,  23.36f, 104.00f, 104.50f }},
	{{  33.36f,  36.13f, 104.00f, 104.50f }},
	{{  74.74f,  75.99f, 104.00f, 105.00f }},
	{{  87.66f,  88.96f, 104.00f, 105.50f }},
	{{ 101.58f, 103.52f, 104.00f, 104.50f }},
	{{ 118.12f, 121.19f, 104.00f, 104.50f }},
	{{ 140.30f, 141.62f, 104.00f, 105.00f }},
	{{ 146.04f, 147.43f, 104.00f, 105.00f }},
	{{ 192.98f, 194.40f, 104.00f, 105.00f }},
	{{ 198.65f, 200.05f, 104.00f, 104.50f }},
	{{ 219.11f, 222.39f, 104.00f, 104.50f }},
	{{ 236.67f, 238.98f, 104.00f, 104.50f }},
	{{ 257.65f, 258.99f, 104.00f, 104.50f }},
	{{ 263.67f, 265.06f, 104.00f, 104.50f }},
	{{ 277.54f, 278.85f, 104.00f, 104.50f }},
	{{  18.35f,  21.87f, 104.50f, 105.00f }},
	{{  31.82f,  35.33f, 104.50f, 105.00f }},
	{{  81.57f,  82.82f, 104.50f, 105.50f }},
	{{ 102.13f, 104.63f, 104.50f, 105.00f }},
	{{ 116.48f, 120.02f, 104.50f, 105.00f }},
	{{ 198.70f, 200.09f, 104.50f, 105.50f }},
	{{ 220.31f, 224.06f, 104.50f, 105.00f }},
	{{ 235.62f, 238.29f, 104.50f, 105.00f }},
	{{ 257.63f, 258.96f, 104.50f, 105.00f }},
	{{ 263.75f, 265.15f, 104.50f, 105.00f }},
	{{ 277.56f, 278.87f, 104.50f, 107.50f }},
	{{ 292.80f, 294.16f, 104.50f, 106.00f }},
	{{ 296.79f, 298.16f, 104.50f, 106.50f }},
	{{  19.26f,  23.47f, 105.00f, 105.50f }},
	{{  30.13f,  34.39f, 105.00f, 105.50f }},
	{{  74.67f,  75.91f, 105.00f, 105.50f }},
	{{ 102.86f, 105.97f, 105.00f, 105.50f }},
	{{ 114.77f, 118.84f, 105.00f, 105.50f }},
	{{ 140.21f, 141.54f, 105.00f, 106.00f }},
	{{ 145.96f, 147.33f, 105.00f, 105.50f }},
	{{ 193.07f, 194.48f, 105.00f, 106.00f }},
	{{ 221.50f, 225.73f, 105.00f, 105.50f }},
	{{ 234.58f, 237.50f, 105.00f, 105.50f }},
	{{ 251.41f, 252.57f, 105.00f, 105.50f }},
	{{ 257.59f, 258.94f, 105.00f, 105.50f }},
	{{ 263.84f, 265.23f, 105.00f, 105.50f }},
	{{   1.99f,   3.25f, 105.50f, 107.50f }},
	{{  20.54f,  33.07f, 105.50f, 106.00f }},
	{{  74.63f,  75.88f, 105.50f, 106.00f }},
	{{  81.64f,  82.86f, 105.50f, 106.00f }},
	{{  87.75f,  89.08f, 105.50f, 106.00f }},
	{{ 103.75f, 108.29f, 105.50f, 106.00f }},
	{{ 111.54f, 117.59f, 105.50f, 106.00f }},
	{{ 145.91f, 147.29f, 105.50f, 106.00f }},
	{{ 198.78f, 200.18f, 105.50f, 106.50f }},
	{{ 222.70f, 229.76f, 105.50f, 106.00f }},
	{{ 230.86f, 236.72f, 105.50f, 106.00f }},
	{{ 251.34f, 252.53f, 105.50f, 106.50f }},
	{{ 257.56f, 258.97f, 105.50f, 106.00f }},
	{{ 263.93f, 265.32f, 105.50f, 106.00f }},
	{{  22.19f,  31.33f, 106.00f, 106.50f }},
	{{  74.60f,  75.84f, 106.00f, 107.00f }},
	{{  81.67f,  82.87f, 106.00f, 107.00f }},
	{{  87.84f,  89.17f, 106.00f, 106.50f }},
	{{ 104.96f, 115.69f, 106.00f, 106.50f }},
	{{ 140.13f, 141.46f, 106.00f, 106.50f }},
	{{ 145.87f, 147.24f, 106.00f, 106.50f }},
	{{ 193.16f, 194.55f, 106.00f, 107.00f }},
	{{ 224.51f, 235.30f, 106.00f, 106.50f }},
	{{ 257.50f, 260.09f, 106.00f, 106.50f }},
	{{ 264.02f, 265.40f, 106.00f, 106.50f }},
	{{ 292.86f, 294.21f, 106.00f, 107.50f }},
	{{  25.18f,  27.63f, 106.50f, 107.00f }},
	{{  87.93f,  89.25f, 106.50f, 107.00f }},
	{{ 107.12f, 113.78f, 106.50f, 107.00f }},
	{{ 140.09f, 141.42f, 106.50f, 107.00f }},
	{{ 145.82f, 147.19f, 106.50f, 107.00f }},
	{{ 198.87f, 200.26f, 106.50f, 107.50f }},
	{{ 226.53f, 233.69f, 106.50f, 107.00f }},
	{{ 251.18f, 252.43f, 106.50f, 107.00f }},
	{{ 257.41f, 262.72f, 106.50f, 107.00f }},
	{{ 264.11f, 265.49f, 106.50f, 107.00f }},
	{{ 296.86f, 298.23f, 106.50f, 107.50f }},
	{{  74.53f,  75.77f, 107.00f, 107.50f }},
	{{  81.73f,  82.99f, 107.00f, 107.50f }},
	{{  88.01f,  89.40f, 107.00f, 107.50f }},
	{{ 140.04f, 141.38f, 107.00f, 107.50f }},
	{{ 145.78f, 147.14f, 107.00f, 107.50f }},
	{{ 193.25f, 194.63f, 107.00f, 107.50f }},
	{{ 251.04f, 252.31f, 107.00f, 107.50f }},
	{{ 257.33f, 262.63f, 107.00f, 107.50f }},
	{{ 264.20f, 265.57f, 107.00f, 107.50f }},
	{{   1.94f,   3.19f, 107.50f, 109.50f }},
	{{   6.05f,   7.49f, 107.50f, 116.50f }},
	{{  74.49f,  75.74f, 107.50f, 108.50f }},
	{{  81.73f,  83.06f, 107.50f, 108.50f }},
	{{  88.10f,  89.56f, 107.50f, 108.00f }},
	{{ 140.00f, 141.34f, 107.50f, 108.50f }},
	{{ 145.74f, 147.09f, 107.50f, 108.00f }},
	{{ 193.29f, 194.67f, 107.50f, 108.50f }},
	{{ 198.95f, 200.35f, 107.50f, 108.00f }},
	{{ 250.89f, 252.18f, 107.50f, 108.00f }},
	{{ 257.18f, 258.62f, 107.50f, 108.00f }},
	{{ 264.29f, 265.66f, 107.50f, 108.00f }},
	{{ 277.66f, 278.95f, 107.50f, 108.00f }},
	{{ 292.92f, 294.27f, 107.50f, 108.50f }},
	{{ 296.90f, 298.27f, 107.50f, 109.00f }},
	{{  88.27f,  89.72f, 108.00f, 108.50f }},
	{{ 145.69f, 147.04f, 108.00f, 109.00f }},
	{{ 198.99f, 200.39f, 108.00f, 109.00f }},
	{{ 250.75f, 251.98f, 108.00f, 108.50f }},
	{{ 257.04f, 258.48f, 108.00f, 108.50f }},
	{{ 264.38f, 265.74f, 108.00f, 108.50f }},
	{{ 277.67f, 278.96f, 108.00f, 110.50f }},
	{{  74.42f,  75.68f, 108.50f, 109.00f }},
	{{  78.79f,  83.28f, 108.50f, 109.00f }},
	{{  88.44f,  89.92f, 108.50f, 109.00f }},
	{{ 139.91f, 141.26f, 108.50f, 109.00f }},
	{{ 193.37f, 194.74f, 108.50f, 109.00f }},
	{{ 250.54f, 251.77f, 108.50f, 109.00f }},
	{{ 256.90f, 258.41f, 108.50f, 109.00f }},
	{{ 264.47f, 265.83f, 108.50f, 109.00f }},
	{{ 292.96f, 294.30f, 108.50f, 110.00f }},
	{{  74.39f,  75.65f, 109.00f, 110.00f }},
	{{  78.68f,  83.42f, 109.00f, 109.50f }},
	{{  88.62f,  90.15f, 109.00f, 109.50f }},
	{{ 139.87f, 141.22f, 109.00f, 109.50f }},
	{{ 145.61f, 146.95f, 109.00f, 110.00f }},
	{{ 193.40f, 194.78f, 109.00f, 110.00f }},
	{{ 199.08f, 200.47f, 109.00f, 110.00f }},
	{{ 250.32f, 251.57f, 109.00f, 109.50f }},
	{{ 256.76f, 258.37f, 109.00f, 109.50f }},
	{{ 264.55f, 265.91f, 109.00f, 109.50f }},
	{{ 296.96f, 298.32f, 109.00f, 110.00f }},
	{{   1.90f,   3.14f, 109.50f, 111.50f }},
	{{  78.64f,  79.70f, 109.50f, 110.00f }},
	{{  82.22f,  83.57f, 109.50f, 110.00f }},
	{{  88.83f,  90.38f, 109.50f, 110.00f }},
	{{ 139.82f, 141.18f, 109.50f, 110.00f }},
	{{ 250.11f, 251.37f, 109.50f, 110.00f }},
	{{ 256.61f, 258.34f, 109.50f, 110.00f }},
	{{ 264.64f, 265.99f, 109.50f, 110.00f }},
	{{  45.30f,  45.65f, 110.00f, 110.50f }},
	{{  56.03f,  56.31f, 110.00f, 110.50f }},
	{{  74.32f,  75.61f, 110.00f, 111.00f }},
	{{  78.61f,  79.63f, 110.00f, 111.00f }},
	{{  82.49f,  83.75f, 110.00f, 110.50f }},
	{{  89.10f,  90.62f, 110.00f, 110.50f }},
	{{ 139.77f, 141.14f, 110.00f, 110.50f }},
	{{ 145.54f, 146.88f, 110.00f, 110.50f }},
	{{ 193.46f, 194.85f, 110.00f, 111.00f }},
	{{ 199.16f, 200.55f, 110.00f, 111.00f }},
	{{ 249.89f, 251.16f, 110.00f, 110.50f }},
	{{ 256.47f, 263.42f, 110.00f, 110.50f }},
	{{ 264.73f, 266.08f, 110.00f, 110.50f }},
	{{ 293.01f, 294.35f, 110.00f, 111.50f }},
	{{ 297.00f, 298.35f, 110.00f, 111.50f }},
	{{  45.23f,  45.79f, 110.50f, 111.00f }},
	{{  55.80f,  56.46f, 110.50f, 111.00f }},
	{{  82.72f,  83.95f, 110.50f, 111.00f }},
	{{  89.37f,  90.85f, 110.50f, 111.00f }},
	{{ 139.71f, 141.11f, 110.50f, 111.00f }},
	{{ 145.51f, 146.84f, 110.50f, 112.50f }},
	{{ 249.61f, 250.96f, 110.50f, 111.00f }},
	{{ 256.33f, 263.48f, 110.50f, 111.00f }},
	{{ 264.81f, 266.16f, 110.50f, 111.00f }},
	{{ 277.75f, 279.03f, 110.50f, 111.50f }},
	{{  45.20f,  45.85f, 111.00f, 114.00f }},
	{{  55.70f,  56.50f, 111.00f, 112.00f }},
	{{  74.24f,  75.58f, 111.00f, 112.00f }},
	{{  78.53f,  79.52f, 111.00f, 112.00f }},
	{{  82.84f,  84.15f, 111.00f, 111.50f }},
	{{  89.64f,  91.17f, 111.00f, 111.50f }},
	{{ 139.65f, 141.08f, 111.00f, 112.00f }},
	{{ 193.51f, 194.95f, 111.00f, 112.50f }},
	{{ 199.20f, 200.66f, 111.00f, 112.00f }},
	{{ 249.29f, 250.74f, 111.00f, 111.50f }},
	{{ 256.17f, 257.71f, 111.00f, 111.50f }},
	{{ 264.90f, 266.24f, 111.00f, 111.50f }},
	{{   1.85f,   3.10f, 111.50f, 114.00f }},
	{{  82.95f,  84.39f, 111.50f, 112.00f }},
	{{  89.91f,  91.54f, 111.50f, 112.00f }},
	{{ 248.96f, 250.39f, 111.50f, 112.00f }},
	{{ 255.94f, 257.44f, 111.50f, 112.00f }},
	{{ 264.98f, 266.33f, 111.50f, 112.00f }},
	{{ 277.78f, 279.06f, 111.50f, 114.50f }},
	{{ 293.06f, 294.40f, 111.50f, 113.50f }},
	{{ 297.05f, 298.40f, 111.50f, 113.00f }},
	{{  55.65f,  56.56f, 112.00f, 112.50f }},
	{{  74.17f,  75.52f, 112.00f, 112.50f }},
	{{  78.45f,  79.44f, 112.00f, 112.50f }},
	{{  83.06f,  84.62f, 112.00f, 112.50f }},
	{{  90.24f,  91.90f, 112.00f, 112.50f }},
	{{ 138.98f, 141.04f, 112.00f, 112.50f }},
	{{ 199.23f, 201.42f, 112.00f, 112.50f }},
	{{ 248.64f, 250.04f, 112.00f, 112.50f }},
	{{ 255.70f, 257.18f, 112.00f, 112.50f }},
	{{ 265.07f, 266.41f, 112.00f, 112.50f }},
	{{  55.66f,  56.55f, 112.50f, 113.50f }},
	{{  74.14f,  75.48f, 112.50f, 113.00f }},
	{{  78.41f,  79.41f, 112.50f, 113.50f }},
	{{  83.11f,  84.86f, 112.50f, 113.00f }},
	{{  90.64f,  92.26f, 112.50f, 113.00f }},
	{{ 137.71f, 141.02f, 112.50f, 113.00f }},
	{{ 145.50f, 146.85f, 112.50f, 113.00f }},
	{{ 193.53f, 194.87f, 112.50f, 113.00f }},
	{{ 199.25f, 202.62f, 112.50f, 113.00f }},
	{{ 248.25f, 249.69f, 112.50f, 113.00f }},
	{{ 255.47f, 256.94f, 112.50f, 113.00f }},
	{{ 265.15f, 266.50f, 112.50f, 113.00f }},
	{{  74.10f,  75.43f, 113.00f, 114.00f }},
	{{  83.15f,  85.12f, 113.00f, 113.50f }},
	{{  91.04f,  92.67f, 113.00f, 113.50f }},
	{{ 136.07f, 140.99f, 113.00f, 113.50f }},
	{{ 145.57f, 146.89f, 113.00f, 113.50f }},
	{{ 193.47f, 194.74f, 113.00f, 113.50f }},
	{{ 199.27f, 204.40f, 113.00f, 113.50f }},
	{{ 247.80f, 249.34f, 113.00f, 113.50f }},
	{{ 255.23f, 256.72f, 113.00f, 113.50f }},
	{{ 265.23f, 266.58f, 113.00f, 113.50f }},
	{{ 297.11f, 298.46f, 113.00f, 114.50f }},
	{{  55.78f,  56.47f, 113.50f, 114.00f }},
	{{  78.32f,  79.36f, 113.50f, 114.00f }},
	{{  83.20f,  85.39f, 113.50f, 114.00f }},
	{{  91.44f,  93.19f, 113.50f, 114.00f }},
	{{ 134.06f, 140.97f, 113.50f, 114.00f }},
	{{ 145.68f, 147.06f, 113.50f, 114.00f }},
	{{ 193.41f, 194.60f, 113.50f, 114.00f }},
	{{ 199.28f, 201.03f, 113.50f, 114.00f }},
	{{ 201.47f, 206.18f, 113.50f, 114.00f }},
	{{ 247.36f, 248.99f, 113.50f, 114.00f }},
	{{ 255.00f, 256.55f, 113.50f, 114.00f }},
	{{ 258.60f, 261.33f, 113.50f, 114.00f }},
	{{ 265.31f, 266.66f, 113.50f, 114.00f }},
	{{ 293.13f, 294.46f, 113.50f, 114.50f }},
	{{   1.79f,   3.04f, 114.00f, 116.00f }},
	{{  45.45f,  45.68f, 114.00f, 114.50f }},
	{{  55.96f,  56.34f, 114.00f, 114.50f }},
	{{  74.03f,  75.35f, 114.00f, 115.50f }},
	{{  78.28f,  79.33f, 114.00f, 114.50f }},
	{{  83.15f,  85.65f, 114.00f, 114.50f }},
	{{  91.85f,  93.71f, 114.00f, 114.50f }},
	{{ 132.01f, 136.86f, 114.00f, 114.50f }},
	{{ 139.49f, 140.95f, 114.00f, 114.50f }},
	{{ 145.89f, 147.26f, 114.00f, 114.50f }},
	{{ 193.18f, 194.41f, 114.00f, 114.50f }},
	{{ 199.30f, 200.82f, 114.00f, 114.50f }},
	{{ 203.43f, 208.26f, 114.00f, 114.50f }},
	{{ 246.90f, 248.61f, 114.00f, 114.50f }},
	{{ 254.76f, 256.44f, 114.00f, 114.50f }},
	{{ 258.11f, 264.01f, 114.00f, 114.50f }},
	{{ 265.40f, 266.75f, 114.00f, 114.50f }},
	{{  78.24f,  79.34f, 114.50f, 115.00f }},
	{{  83.08f,  84.06f, 114.50f, 115.00f }},
	{{  84.16f,  85.95f, 114.50f, 115.00f }},
	{{  92.41f,  94.23f, 114.50f, 115.00f }},
	{{ 129.77f, 134.90f, 114.50f, 115.00f }},
	{{ 139.61f, 140.98f, 114.50f, 115.00f }},
	{{ 146.13f, 147.61f, 114.50f, 115.00f }},
	{{ 192.85f, 194.19f, 114.50f, 115.00f }},
	{{ 199.31f, 200.74f, 114.50f, 115.00f }},
	{{ 205.31f, 210.39f, 114.50f, 115.00f }},
	{{ 246.28f, 248.05f, 114.50f, 115.00f }},
	{{ 254.48f, 256.37f, 114.50f, 115.00f }},
	{{ 259.62f, 263.57f, 114.50f, 115.00f }},
	{{ 265.48f, 266.83f, 114.50f, 115.00f }},
	{{ 277.86f, 279.15f, 114.50f, 117.50f }},
	{{ 293.16f, 294.49f, 114.50f, 116.50f }},
	{{ 297.16f, 298.51f, 114.50f, 116.00f }},
	{{  78.19f,  79.36f, 115.00f, 115.50f }},
	{{  82.83f,  83.90f, 115.00f, 115.50f }},
	{{  84.72f,  86.26f, 115.00f, 115.50f }},
	{{  92.97f,  94.91f, 115.00f, 115.50f }},
	{{ 127.52f, 132.96f, 115.00f, 115.50f }},
	{{ 139.69f, 141.04f, 115.00f, 115.50f }},
	{{ 146.48f, 148.09f, 115.00f, 115.50f }},
	{{ 192.40f, 193.90f, 115.00f, 115.50f }},
	{{ 199.22f, 200.67f, 115.00f, 115.50f }},
	{{ 207.20f, 212.69f, 115.00f, 115.50f }},
	{{ 245.66f, 247.48f, 115.00f, 115.50f }},
	{{ 254.12f, 256.35f, 115.00f, 115.50f }},
	{{ 265.56f, 266.91f, 115.00f, 115.50f }},
	{{  43.76f,  47.49f, 115.50f, 116.00f }},
	{{  54.53f,  57.80f, 115.50f, 116.00f }},
	{{  73.92f,  75.22f, 115.50f, 116.50f }},
	{{  78.15f,  83.83f, 115.50f, 116.50f }},
	{{  85.08f,  86.61f, 115.50f, 116.00f }},
	{{  93.53f,  95.69f, 115.50f, 116.00f }},
	{{ 125.00f, 131.00f, 115.50f, 116.00f }},
	{{ 139.80f, 141.09f, 115.50f, 116.00f }},
	{{ 146.87f, 148.76f, 115.50f, 116.00f }},
	{{ 191.82f, 193.50f, 115.50f, 116.00f }},
	{{ 199.13f, 200.58f, 115.50f, 116.00f }},
	{{ 209.24f, 215.20f, 115.50f, 116.00f }},
	{{ 244.95f, 246.92f, 115.50f, 116.00f }},
	{{ 253.76f, 256.37f, 115.50f, 116.00f }},
	{{ 265.64f, 267.00f, 115.50f, 116.00f }},
	{{   1.74f,   2.99f, 116.00f, 118.00f }},
	{{  42.93f,  48.23f, 116.00f, 116.50f }},
	{{  53.56f,  58.70f, 116.00f, 116.50f }},
	{{  85.46f,  87.04f, 116.00f, 116.50f }},
	{{  94.19f,  96.48f, 116.00f, 116.50f }},
	{{ 122.08f, 128.63f, 116.00f, 116.50f }},
	{{ 139.92f, 141.15f, 116.00f, 116.50f }},
	{{ 147.42f, 150.41f, 116.00f, 116.50f }},
	{{ 189.94f, 192.98f, 116.00f, 116.50f }},
	{{ 199.04f, 200.47f, 116.00f, 116.50f }},
	{{ 211.37f, 217.80f, 116.00f, 116.50f }},
	{{ 243.75f, 246.35f, 116.00f, 116.50f }},
	{{ 253.40f, 256.45f, 116.00f, 116.50f }},
	{{ 265.71f, 267.08f, 116.00f, 116.50f }},
	{{ 297.22f, 298.56f, 116.00f, 117.00f }},
	{{   6.05f,   7.45f, 116.50f, 117.00f }},
	{{  42.37f,  44.57f, 116.50f, 117.00f }},
	{{  46.86f,  48.81f, 116.50f, 117.00f }},
	{{  52.92f,  54.96f, 116.50f, 117.00f }},
	{{  57.55f,  59.39f, 116.50f, 117.00f }},
	{{  73.84f,  75.15f, 116.50f, 118.00f }},
	{{  78.06f,  79.11f, 116.50f, 117.00f }},
	{{  82.68f,  83.73f, 116.50f, 117.00f }},
	{{  85.85f,  87.47f, 116.50f, 117.00f }},
	{{  95.01f,  97.80f, 116.50f, 117.00f }},
	{{ 118.96f, 126.11f, 116.50f, 117.00f }},
	{{ 140.05f, 141.35f, 116.50f, 117.00f }},
	{{ 148.06f, 192.29f, 116.50f, 117.00f }},
	{{ 198.95f, 200.34f, 116.50f, 117.00f }},
	{{ 213.66f, 220.87f, 116.50f, 117.00f }},
	{{ 242.55f, 245.61f, 116.50f, 117.00f }},
	{{ 253.03f, 254.68f, 116.50f, 117.00f }},
	{{ 255.27f, 256.53f, 116.50f, 117.00f }},
	{{ 265.79f, 267.16f, 116.50f, 117.00f }},
	{{ 293.21f, 294.55f, 116.50f, 118.00f }},
	{{   6.06f,   7.45f, 117.00f, 123.00f }},
	{{  42.07f,  43.59f, 117.00f, 117.50f }},
	{{  47.78f,  49.29f, 117.00f, 117.50f }},
	{{  52.54f,  54.09f, 117.00f, 117.50f }},
	{{  58.38f,  59.84f, 117.00f, 117.50f }},
	{{  78.01f,  79.04f, 117.00f, 118.00f }},
	{{  82.65f,  83.69f, 117.00f, 117.50f }},
	{{  86.23f,  87.90f, 117.00f, 117.50f }},
	{{  95.83f,  99.22f, 117.00f, 117.50f }},
	{{ 115.57f, 123.61f, 117.00f, 117.50f }},
	{{ 140.19f, 141.55f, 117.00f, 117.50f }},
	{{ 148.91f, 191.37f, 117.00f, 117.50f }},
	{{ 198.84f, 200.21f, 117.00f, 117.50f }},
	{{ 216.11f, 224.44f, 117.00f, 117.50f }},
	{{ 241.35f, 244.63f, 117.00f, 117.50f }},
	{{ 252.67f, 254.25f, 117.00f, 117.50f }},
	{{ 255.40f, 256.61f, 117.00f, 117.50f }},
	{{ 265.87f, 267.25f, 117.00f, 117.50f }},
	{{ 297.25f, 298.59f, 117.00f, 119.00f }},
	{{  41.78f,  43.08f, 117.50f, 118.00f }},
	{{  48.18f,  49.62f, 117.50f, 118.00f }},
	{{  52.29f,  53.55f, 117.50f, 118.00f }},
	{{  58.80f,  60.09f, 117.50f, 118.00f }},
	{{  82.62f,  83.64f, 117.50f, 119.00f }},
	{{  86.61f,  88.33f, 117.50f, 118.00f }},
	{{  96.81f, 102.14f, 117.50f, 118.00f }},
	{{ 110.40f, 120.89f, 117.50f, 118.00f }},
	{{ 140.32f, 141.77f, 117.50f, 118.00f }},
	{{ 150.00f, 190.33f, 117.50f, 118.00f }},
	{{ 198.56f, 200.06f, 117.50f, 118.00f }},
	{{ 218.89f, 228.01f, 117.50f, 118.00f }},
	{{ 237.85f, 243.66f, 117.50f, 118.00f }},
	{{ 252.21f, 253.84f, 117.50f, 118.00f }},
	{{ 255.54f, 256.70f, 117.50f, 118.00f }},
	{{ 258.68f, 264.41f, 117.50f, 118.00f }},
	{{ 265.95f, 267.33f, 117.50f, 118.00f }},
	{{ 277.94f, 279.25f, 117.50f, 118.00f }},
	{{   1.69f,   2.94f, 118.00f, 120.00f }},
	{{  41.68f,  42.74f, 118.00f, 118.50f }},
	{{  48.43f,  49.73f, 118.00f, 118.50f }},
	{{  52.10f,  53.36f, 118.00f, 118.50f }},
	{{  59.11f,  60.17f, 118.00f, 118.50f }},
	{{  73.72f,  75.04f, 118.00f, 119.50f }},
	{{  77.92f,  78.93f, 118.00f, 119.00f }},
	{{  87.04f,  88.77f, 118.00f, 118.50f }},
	{{  98.09f, 117.57f, 118.00f, 118.50f }},
	{{ 140.43f, 142.06f, 118.00f, 118.50f }},
	{{ 198.28f, 199.93f, 118.00f, 118.50f }},
	{{ 221.95f, 242.51f, 118.00f, 118.50f }},
	{{ 251.67f, 253.39f, 118.00f, 118.50f }},
	{{ 255.63f, 256.78f, 118.00f, 118.50f }},
	{{ 258.93f, 264.21f, 118.00f, 118.50f }},
	{{ 266.02f, 267.41f, 118.00f, 118.50f }},
	{{ 277.96f, 279.27f, 118.00f, 120.50f }},
	{{ 293.26f, 294.60f, 118.00f, 119.50f }},
	{{  41.66f,  42.57f, 118.50f, 119.00f }},
	{{  48.46f,  49.68f, 118.50f, 119.00f }},
	{{  52.13f,  53.18f, 118.50f, 119.00f }},
	{{  59.17f,  60.17f, 118.50f, 119.00f }},
	{{  87.55f,  89.26f, 118.50f, 119.00f }},
	{{ 100.13f, 113.38f, 118.50f, 119.00f }},
	{{ 139.90f, 142.35f, 118.50f, 119.00f }},
	{{ 198.00f, 200.40f, 118.50f, 119.00f }},
	{{ 225.66f, 240.55f, 118.50f, 119.00f }},
	{{ 251.13f, 252.91f, 118.50f, 119.00f }},
	{{ 255.72f, 256.87f, 118.50f, 119.00f }},
	{{ 266.10f, 267.49f, 118.50f, 119.00f }},
	{{  41.65f,  42.66f, 119.00f, 119.50f }},
	{{  48.38f,  49.56f, 119.00f, 119.50f }},
	{{  52.16f,  53.29f, 119.00f, 119.50f }},
	{{  58.93f,  60.09f, 119.00f, 119.50f }},
	{{  77.83f,  78.85f, 119.00f, 119.50f }},
	{{  82.54f,  83.51f, 119.00f, 120.00f }},
	{{  88.05f,  89.89f, 119.00f, 119.50f }},
	{{ 138.22f, 142.67f, 119.00f, 119.50f }},
	{{ 197.72f, 202.12f, 119.00f, 119.50f }},
	{{ 232.72f, 235.16f, 119.00f, 119.50f }},
	{{ 250.58f, 252.44f, 119.00f, 119.50f }},
	{{ 255.81f, 256.95f, 119.00f, 119.50f }},
	{{ 266.18f, 267.58f, 119.00f, 119.50f }},
	{{ 297.32f, 298.66f, 119.00f, 120.00f }},
	{{  41.81f,  43.00f, 119.50f, 120.00f }},
	{{  48.12f,  49.43f, 119.50f, 120.00f }},
	{{  52.34f,  53.60f, 119.50f, 120.00f }},
	{{  58.57f,  59.81f, 119.50f, 120.00f }},
	{{  73.62f,  74.93f, 119.50f, 120.50f }},
	{{  77.78f,  78.82f, 119.50f, 120.00f }},
	{{  88.56f,  90.51f, 119.50f, 120.00f }},
	{{ 136.46f, 143.11f, 119.50f, 120.00f }},
	{{ 197.29f, 203.91f, 119.50f, 120.00f }},
	{{ 250.04f, 251.96f, 119.50f, 120.00f }},
	{{ 255.90f, 257.03f, 119.50f, 120.00f }},
	{{ 266.26f, 267.66f, 119.50f, 120.50f }},
	{{ 293.31f, 294.65f, 119.50f, 120.50f }},
	{{   1.65f,   2.89f, 120.00f, 122.00f }},
	{{  42.10f,  43.70f, 120.00f, 120.50f }},
	{{  47.45f,  49.05f, 120.00f, 120.50f }},
	{{  52.66f,  54.19f, 120.00f, 120.50f }},
	{{  57.95f,  59.54f, 120.00f, 120.50f }},
	{{  77.74f,  78.80f, 120.00f, 121.50f }},
	{{  82.47f,  83.41f, 120.00f, 120.50f }},
	{{  89.08f,  91.14f, 120.00f, 120.50f }},
	{{ 134.72f, 139.30f, 120.00f, 120.50f }},
	{{ 141.63f, 143.55f, 120.00f, 120.50f }},
	{{ 196.86f, 198.69f, 120.00f, 120.50f }},
	{{ 201.01f, 205.74f, 120.00f, 120.50f }},
	{{ 249.35f, 251.41f, 120.00f, 120.50f }},
	{{ 255.98f, 257.10f, 120.00f, 120.50f }},
	{{ 297.36f, 298.70f, 120.00f, 121.50f }},
	{{  42.40f,  45.19f, 120.50f, 121.00f }},
	{{  45.93f,  48.64f, 120.50f, 121.00f }},
	{{  53.03f,  55.37f, 120.50f, 121.00f }},
	{{  56.83f,  59.26f, 120.50f, 121.00f }},
	{{  73.55f,  74.86f, 120.50f, 121.00f }},
	{{  82.42f,  83.36f, 120.50f, 121.00f }},
	{{  89.75f,  91.77f, 120.50f, 121.00f }},
	{{ 132.85f, 137.75f, 120.50f, 121.00f }},
	{{ 142.15f, 144.15f, 120.50f, 121.00f }},
	{{ 196.27f, 198.17f, 120.50f, 121.00f }},
	{{ 202.69f, 207.58f, 120.50f, 121.00f }},
	{{ 248.58f, 250.77f, 120.50f, 121.00f }},
	{{ 256.05f, 257.18f, 120.50f, 121.00f }},
	{{ 266.42f, 267.82f, 120.50f, 121.00f }},
	{{ 278.02f, 279.37f, 120.50f, 122.00f }},
	{{ 293.35f, 294.69f, 120.50f, 122.50f }},
	{{  42.98f,  47.86f, 121.00f, 121.50f }},
	{{  53.97f,  58.34f, 121.00f, 121.50f }},
	{{  73.52f,  74.82f, 121.00f, 122.50f }},
	{{  82.37f,  83.31f, 121.00f, 121.50f }},
	{{  90.41f,  92.59f, 121.00f, 121.50f }},
	{{ 130.87f, 136.11f, 121.00f, 121.50f }},
	{{ 142.66f, 144.81f, 121.00f, 121.50f }},
	{{ 195.58f, 197.64f, 121.00f, 121.50f }},
	{{ 204.48f, 209.63f, 121.00f, 121.50f }},
	{{ 247.82f, 250.12f, 121.00f, 121.50f }},
	{{ 256.12f, 257.26f, 121.00f, 121.50f }},
	{{ 266.50f, 267.90f, 121.00f, 121.50f }},
	{{  44.24f,  46.34f, 121.50f, 122.00f }},
	{{  54.92f,  57.28f, 121.50f, 122.00f }},
	{{  77.63f,  78.80f, 121.50f, 122.00f }},
	{{  82.29f,  83.26f, 121.50f, 122.00f }},
	{{  91.07f,  93.54f, 121.50f, 122.00f }},
	{{ 128.80f, 134.18f, 121.50f, 122.00f }},
	{{ 143.32f, 145.70f, 121.50f, 122.00f }},
	{{ 194.67f, 197.09f, 121.50f, 122.00f }},
	{{ 206.37f, 211.96f, 121.50f, 122.00f }},
	{{ 246.89f, 249.47f, 121.50f, 122.00f }},
	{{ 256.20f, 257.34f, 121.50f, 122.00f }},
	{{ 259.14f, 265.17f, 121.50f, 122.00f }},
	{{ 266.58f, 267.98f, 121.50f, 122.00f }},
	{{ 297.42f, 298.76f, 121.50f, 122.50f }},
	{{   1.60f,   2.84f, 122.00f, 124.00f }},
	{{  67.82f,  69.05f, 122.00f, 125.00f }},
	{{  77.59f,  81.28f, 122.00f, 122.50f }},
	{{  81.47f,  83.20f, 122.00f, 122.50f }},
	{{  91.85f,  94.50f, 122.00f, 122.50f }},
	{{ 126.18f, 132.20f, 122.00f, 122.50f }},
	{{ 144.01f, 146.68f, 122.00f, 122.50f }},
	{{ 193.55f, 196.42f, 122.00f, 122.50f }},
	{{ 208.37f, 214.51f, 122.00f, 122.50f }},
	{{ 245.85f, 248.67f, 122.00f, 122.50f }},
	{{ 256.27f, 257.42f, 122.00f, 122.50f }},
	{{ 261.59f, 263.54f, 122.00f, 122.50f }},
	{{ 266.67f, 268.06f, 122.00f, 123.00f }},
	{{ 278.05f, 279.43f, 122.00f, 122.50f }},
	{{  73.42f,  74.75f, 122.50f, 123.00f }},
	{{  77.56f,  83.14f, 122.50f, 123.00f }},
	{{  92.76f,  95.48f, 122.50f, 123.00f }},
	{{ 122.90f, 130.15f, 122.50f, 123.00f }},
	{{ 144.91f, 164.49f, 122.50f, 123.00f }},
	{{ 180.58f, 195.57f, 122.50f, 123.00f }},
	{{ 210.46f, 217.68f, 122.50f, 123.00f }},
	{{ 244.76f, 247.79f, 122.50f, 123.00f }},
	{{ 256.34f, 257.49f, 122.50f, 123.00f }},
	{{ 278.06f, 279.45f, 122.50f, 125.00f }},
	{{ 293.43f, 294.76f, 122.50f, 123.00f }},
	{{ 297.45f, 298.80f, 122.50f, 124.00f }},
	{{   6.15f,   7.35f, 123.00f, 123.50f }},
	{{  73.39f,  74.73f, 123.00f, 124.00f }},
	{{  77.53f,  79.61f, 123.00f, 123.50f }},
	{{  81.11f,  82.87f, 123.00f, 123.50f }},
	{{  93.66f,  97.10f, 123.00f, 123.50f }},
	{{ 119.17f, 127.85f, 123.00f, 123.50f }},
	{{ 146.01f, 194.40f, 123.00f, 123.50f }},
	{{ 212.75f, 221.14f, 123.00f, 123.50f }},
	{{ 243.26f, 246.92f, 123.00f, 123.50f }},
	{{ 256.42f, 257.57f, 123.00f, 123.50f }},
	{{ 266.83f, 268.22f, 123.00f, 123.50f }},
	{{ 293.45f, 294.77f, 123.00f, 125.00f }},
	{{   6.20f,   7.34f, 123.50f, 124.50f }},
	{{  77.54f,  78.40f, 123.50f, 124.00f }},
	{{  94.65f,  98.72f, 123.50f, 124.00f }},
	{{ 114.91f, 125.27f, 123.50f, 124.00f }},
	{{ 148.53f, 192.55f, 123.50f, 124.00f }},
	{{ 215.07f, 225.11f, 123.50f, 124.00f }},
	{{ 241.48f, 245.75f, 123.50f, 124.00f }},
	{{ 256.50f, 257.65f, 123.50f, 124.00f }},
	{{ 266.92f, 268.30f, 123.50f, 124.00f }},
	{{   1.53f,   2.79f, 124.00f, 126.00f }},
	{{  73.33f,  74.68f, 124.00f, 125.00f }},
	{{  95.49f, 105.15f, 124.00f, 124.50f }},
	{{ 105.73f, 122.14f, 124.00f, 124.50f }},
	{{ 218.07f, 229.81f, 124.00f, 124.50f }},
	{{ 238.37f, 244.44f, 124.00f, 124.50f }},
	{{ 256.58f, 257.72f, 124.00f, 124.50f }},
	{{ 267.00f, 268.37f, 124.00f, 124.50f }},
	{{ 297.51f, 298.85f, 124.00f, 125.50f }},
	{{   6.30f,   7.32f, 124.50f, 125.50f }},
	{{  95.71f, 243.11f, 124.50f, 125.00f }},
	{{ 256.68f, 257.79f, 124.50f, 125.00f }},
	{{ 267.08f, 268.44f, 124.50f, 125.00f }},
	{{  67.37f,  68.85f, 125.00f, 125.50f }},
	{{  73.27f,  74.63f, 125.00f, 125.50f }},
	{{  95.51f,  96.98f, 125.00f, 125.50f }},
	{{  97.61f, 242.72f, 125.00f, 125.50f }},
	{{ 256.79f, 257.86f, 125.00f, 125.50f }},
	{{ 259.54f, 265.62f, 125.00f, 125.50f }},
	{{ 267.17f, 268.52f, 125.00f, 125.50f }},
	{{ 278.08f, 279.56f, 125.00f, 126.50f }},
	{{ 293.56f, 294.81f, 125.00f, 126.00f }},
	{{   6.39f,   8.40f, 125.50f, 126.00f }},
	{{  66.92f,  68.62f, 125.50f, 126.00f }},
	{{  73.24f,  74.61f, 125.50f, 127.00f }},
	{{  95.26f,  96.64f, 125.50f, 126.00f }},
	{{  97.59f,  99.27f, 125.50f, 126.00f }},
	{{ 101.07f, 239.30f, 125.50f, 126.00f }},
	{{ 241.39f, 242.82f, 125.50f, 126.00f }},
	{{ 256.90f, 257.93f, 125.50f, 126.00f }},
	{{ 259.61f, 265.42f, 125.50f, 126.00f }},
	{{ 267.25f, 268.60f, 125.50f, 126.00f }},
	{{ 297.57f, 298.91f, 125.50f, 127.00f }},
	{{   1.47f,   2.74f, 126.00f, 126.50f }},
	{{   6.73f,  68.27f, 126.00f, 126.50f }},
	{{  95.02f,  96.36f, 126.00f, 126.50f }},
	{{  97.53f,  98.99f, 126.00f, 126.50f }},
	{{ 101.26f, 102.85f, 126.00f, 126.50f }},
	{{ 237.45f, 238.94f, 126.00f, 126.50f }},
	{{ 241.63f, 242.95f, 126.00f, 126.50f }},
	{{ 257.04f, 258.00f, 126.00f, 126.50f }},
	{{ 267.33f, 268.69f, 126.00f, 126.50f }},
	{{ 293.06f, 294.81f, 126.00f, 126.50f }},
	{{   1.45f,   2.73f, 126.50f, 129.00f }},
	{{   7.16f,  67.70f, 126.50f, 127.00f }},
	{{  94.79f,  96.10f, 126.50f, 127.00f }},
	{{  97.46f,  98.81f, 126.50f, 127.00f }},
	{{ 101.41f, 102.87f, 126.50f, 127.00f }},
	{{ 237.47f, 238.78f, 126.50f, 127.00f }},
	{{ 241.82f, 243.10f, 126.50f, 127.00f }},
	{{ 257.19f, 258.01f, 126.50f, 127.00f }},
	{{ 267.42f, 294.80f, 126.50f, 127.00f }},
	{{  73.15f,  74.54f, 127.00f, 128.50f }},
	{{  94.55f,  95.85f, 127.00f, 127.50f }},
	{{  97.38f,  98.67f, 127.00f, 127.50f }},
	{{ 101.53f, 102.95f, 127.00f, 127.50f }},
	{{ 237.39f, 238.63f, 127.00f, 127.50f }},
	{{ 241.99f, 243.26f, 127.00f, 127.50f }},
	{{ 257.48f, 257.91f, 127.00f, 127.50f }},
	{{ 267.50f, 294.76f, 127.00f, 127.50f }},
	{{ 297.63f, 298.97f, 127.00f, 128.00f }},
	{{  76.17f,  76.95f, 127.50f, 128.00f }},
	{{  94.28f,  95.60f, 127.50f, 128.00f }},
	{{  97.30f,  98.55f, 127.50f, 128.00f }},
	{{ 101.66f, 103.04f, 127.50f, 128.00f }},
	{{ 237.32f, 238.50f, 127.50f, 128.00f }},
	{{ 242.14f, 243.43f, 127.50f, 128.00f }},
	{{ 267.58f, 290.93f, 127.50f, 128.00f }},
	{{  76.03f,  77.01f, 128.00f, 128.50f }},
	{{  93.96f,  95.33f, 128.00f, 128.50f }},
	{{  97.23f,  98.43f, 128.00f, 128.50f }},
	{{ 101.78f, 103.14f, 128.00f, 128.50f }},
	{{ 237.23f, 238.38f, 128.00f, 128.50f }},
	{{ 242.29f, 243.61f, 128.00f, 128.50f }},
	{{ 267.65f, 269.11f, 128.00f, 128.50f }},
	{{ 297.67f, 299.02f, 128.00f, 129.00f }},
	{{  73.05f,  74.47f, 128.50f, 130.00f }},
	{{  75.98f,  95.06f, 128.50f, 129.00f }},
	{{  97.15f,  98.30f, 128.50f, 129.00f }},
	{{ 101.89f, 103.25f, 128.50f, 129.00f }},
	{{ 237.13f, 238.26f, 128.50f, 129.00f }},
	{{ 242.44f, 243.82f, 128.50f, 129.00f }},
	{{ 267.70f, 269.15f, 128.50f, 129.00f }},
	{{   1.34f,   2.68f, 129.00f, 131.00f }},
	{{  75.98f,  94.77f, 129.00f, 129.50f }},
	{{  97.08f,  98.21f, 129.00f, 129.50f }},
	{{ 102.00f, 103.36f, 129.00f, 129.50f }},
	{{ 237.02f, 238.14f, 129.00f, 129.50f }},
	{{ 242.57f, 244.22f, 129.00f, 129.50f }},
	{{ 267.60f, 269.21f, 129.00f, 129.50f }},
	{{ 297.72f, 299.07f, 129.00f, 130.00f }},
	{{  75.99f,  93.86f, 129.50f, 130.00f }},
	{{  97.00f,  98.13f, 129.50f, 130.00f }},
	{{ 102.11f, 103.48f, 129.50f, 130.00f }},
	{{ 236.91f, 238.02f, 129.50f, 130.00f }},
	{{ 242.67f, 269.28f, 129.50f, 130.00f }},
	{{  72.91f,  74.45f, 130.00f, 130.50f }},
	{{  96.93f,  98.05f, 130.00f, 130.50f }},
	{{ 102.22f, 103.60f, 130.00f, 130.50f }},
	{{ 236.76f, 237.90f, 130.00f, 130.50f }},
	{{ 242.76f, 269.35f, 130.00f, 130.50f }},
	{{ 297.77f, 299.12f, 130.00f, 131.00f }},
	{{  72.83f,  74.45f, 130.50f, 131.00f }},
	{{  96.86f,  98.00f, 130.50f, 131.00f }},
	{{ 102.33f, 103.72f, 130.50f, 131.00f }},
	{{ 236.61f, 237.79f, 130.50f, 131.00f }},
	{{ 242.86f, 245.08f, 130.50f, 131.00f }},
	{{ 267.24f, 269.42f, 130.50f, 131.00f }},
	{{   1.23f,   2.70f, 131.00f, 131.50f }},
	{{  72.66f,  74.44f, 131.00f, 131.50f }},
	{{  96.79f,  97.95f, 131.00f, 131.50f }},
	{{ 102.44f, 106.34f, 131.00f, 131.50f }},
	{{ 236.42f, 237.67f, 131.00f, 131.50f }},
	{{ 242.92f, 244.21f, 131.00f, 131.50f }},
	{{ 268.07f, 269.50f, 131.00f, 131.50f }},
	{{ 297.82f, 299.18f, 131.00f, 132.00f }},
	{{   1.20f,  74.44f, 131.50f, 132.50f }},
	{{  96.73f,  97.89f, 131.50f, 132.00f }},
	{{ 102.51f, 237.55f, 131.50f, 132.00f }},
	{{ 242.96f, 244.23f, 131.50f, 132.00f }},
	{{ 268.11f, 277.68f, 131.50f, 132.00f }},
	{{  96.68f,  97.85f, 132.00f, 132.50f }},
	{{ 102.35f, 237.80f, 132.00f, 132.50f }},
	{{ 243.00f, 244.26f, 132.00f, 133.00f }},
	{{ 268.15f, 299.26f, 132.00f, 133.00f }},
	{{   1.13f,  74.43f, 132.50f, 133.00f }},
	{{  96.62f,  97.81f, 132.50f, 133.50f }},
	{{ 102.09f, 104.88f, 132.50f, 133.00f }},
	{{ 235.95f, 238.12f, 132.50f, 133.00f }},
	{{   1.08f,   2.83f, 133.00f, 133.50f }},
	{{  72.82f,  74.43f, 133.00f, 133.50f }},
	{{ 101.80f, 103.50f, 133.00f, 133.50f }},
	{{ 236.87f, 238.44f, 133.00f, 133.50f }},
	{{ 243.07f, 244.36f, 133.00f, 134.50f }},
	{{ 268.14f, 271.82f, 133.00f, 133.50f }},
	{{ 295.34f, 299.36f, 133.00f, 133.50f }},
	{{   1.03f,   2.66f, 133.50f, 134.00f }},
	{{  72.91f,  74.42f, 133.50f, 135.50f }},
	{{  96.52f,  97.73f, 133.50f, 134.50f }},
	{{ 101.54f, 102.99f, 133.50f, 134.00f }},
	{{ 237.32f, 238.72f, 133.50f, 134.00f }},
	{{ 268.14f, 269.62f, 133.50f, 134.50f }},
	{{ 297.53f, 299.41f, 133.50f, 134.00f }},
	{{   0.98f,   2.50f, 134.00f, 134.50f }},
	{{ 101.32f, 102.73f, 134.00f, 134.50f }},
	{{ 237.56f, 238.98f, 134.00f, 134.50f }},
	{{ 297.85f, 299.46f, 134.00f, 134.50f }},
	{{   0.93f,   2.35f, 134.50f, 135.00f }},
	{{  96.41f,  97.66f, 134.50f, 135.00f }},
	{{ 101.15f, 102.54f, 134.50f, 135.00f }},
	{{ 237.80f, 239.19f, 134.50f, 135.00f }},
	{{ 243.08f, 244.49f, 134.50f, 139.00f }},
	{{ 268.14f, 269.53f, 134.50f, 136.00f }},
	{{ 297.96f, 299.51f, 134.50f, 135.00f }},
	{{   0.88f,   2.27f, 135.00f, 135.50f }},
	{{  96.38f,  97.63f, 135.00f, 135.50f }},
	{{ 100.98f, 102.38f, 135.00f, 135.50f }},
	{{ 238.00f, 239.40f, 135.00f, 135.50f }},
	{{ 298.08f, 299.56f, 135.00f, 136.00f }},
	{{   0.82f,   2.20f, 135.50f, 136.00f }},
	{{  72.95f,  74.40f, 135.50f, 137.50f }},
	{{  96.34f,  97.60f, 135.50f, 136.50f }},
	{{ 100.82f, 102.21f, 135.50f, 136.00f }},
	{{ 238.17f, 239.58f, 135.50f, 136.00f }},
	{{   0.75f,   2.12f, 136.00f, 136.50f }},
	{{ 100.67f, 102.04f, 136.00f, 136.50f }},
	{{ 238.34f, 239.74f, 136.00f, 136.50f }},
	{{ 268.10f, 269.44f, 136.00f, 138.00f }},
	{{ 298.19f, 299.66f, 136.00f, 137.00f }},
	{{   0.69f,   2.04f, 136.50f, 137.00f }},
	{{  96.27f,  97.54f, 136.50f, 137.00f }},
	{{ 100.51f, 101.88f, 136.50f, 137.00f }},
	{{ 238.52f, 239.91f, 136.50f, 137.00f }},
	{{   0.63f,   1.96f, 137.00f, 137.50f }},
	{{  96.24f,  97.51f, 137.00f, 138.50f }},
	{{ 100.36f, 101.71f, 137.00f, 137.50f }},
	{{ 238.68f, 240.07f, 137.00f, 137.50f }},
	{{ 298.29f, 299.75f, 137.00f, 137.50f }},
	{{   0.56f,   1.88f, 137.50f, 138.00f }},
	{{  73.01f,  74.45f, 137.50f, 139.00f }},
	{{ 100.20f, 101.55f, 137.50f, 138.00f }},
	{{ 238.84f, 240.21f, 137.50f, 138.00f }},
	{{ 298.33f, 299.78f, 137.50f, 138.00f }},
	{{   0.50f,   1.84f, 138.00f, 138.50f }},
	{{ 100.05f, 101.39f, 138.00f, 138.50f }},
	{{ 239.00f, 240.35f, 138.00f, 138.50f }},
	{{ 268.04f, 269.34f, 138.00f, 139.50f }},
	{{ 298.36f, 299.81f, 138.00f, 139.00f }},
	{{   0.44f,   1.79f, 138.50f, 139.00f }},
	{{  96.16f,  97.44f, 138.50f, 139.00f }},
	{{  99.89f, 101.23f, 138.50f, 139.00f }},
	{{ 239.16f, 240.49f, 138.50f, 139.00f }},
	{{   0.38f,   1.74f, 139.00f, 139.50f }},
	{{  73.07f,  74.50f, 139.00f, 140.50f }},
	{{  96.15f,  97.42f, 139.00f, 141.00f }},
	{{  99.74f, 101.07f, 139.00f, 139.50f }},
	{{ 239.31f, 240.63f, 139.00f, 139.50f }},
	{{ 243.04f, 244.42f, 139.00f, 140.00f }},
	{{ 298.43f, 299.86f, 139.00f, 139.50f }},
	{{   0.32f,   1.69f, 139.50f, 140.00f }},
	{{  99.58f, 100.91f, 139.50f, 140.00f }},
	{{ 239.46f, 240.78f, 139.50f, 140.00f }},
	{{ 267.94f, 269.25f, 139.50f, 140.00f }},
	{{ 298.45f, 299.89f, 139.50f, 141.00f }},
	{{   0.26f,   1.65f, 140.00f, 140.50f }},
	{{  99.43f, 100.76f, 140.00f, 140.50f }},
	{{ 239.62f, 240.93f, 140.00f, 140.50f }},
	{{ 242.98f, 244.35f, 140.00f, 140.50f }},
	{{ 267.91f, 269.21f, 140.00f, 141.00f }},
	{{   0.22f,   1.60f, 140.50f, 141.50f }},
	{{  73.16f,  74.55f, 140.50f, 142.00f }},
	{{  99.27f, 100.60f, 140.50f, 141.00f }},
	{{ 239.77f, 241.09f, 140.50f, 141.00f }},
	{{ 242.95f, 244.31f, 140.50f, 141.50f }},
	{{  96.10f,  97.34f, 141.00f, 143.50f }},
	{{  99.12f, 100.44f, 141.00f, 141.50f }},
	{{ 239.91f, 241.24f, 141.00f, 141.50f }},
	{{ 267.84f, 269.12f, 141.00f, 142.00f }},
	{{ 298.52f, 299.96f, 141.00f, 142.00f }},
	{{   0.15f,   1.51f, 141.50f, 143.00f }},
	{{  98.95f, 100.28f, 141.50f, 142.00f }},
	{{ 240.05f, 241.41f, 141.50f, 142.00f }},
	{{ 242.88f, 244.24f, 141.50f, 142.00f }},
	{{  73.29f,  74.63f, 142.00f, 142.50f }},
	{{  98.79f, 100.13f, 142.00f, 142.50f }},
	{{ 240.19f, 241.57f, 142.00f, 142.50f }},
	{{ 242.83f, 244.20f, 142.00f, 143.00f }},
	{{ 267.72f, 269.00f, 142.00f, 142.50f }},
	{{ 298.56f, 299.98f, 142.00f, 148.50f }},
	{{  73.33f,  74.71f, 142.50f, 143.00f }},
	{{  98.63f,  99.97f, 142.50f, 143.00f }},
	{{ 240.32f, 241.74f, 142.50f, 143.00f }},
	{{ 267.66f, 268.93f, 142.50f, 143.00f }},
	{{   0.09f,   1.45f, 143.00f, 145.00f }},
	{{  73.38f,  74.79f, 143.00f, 143.50f }},
	{{  98.45f,  99.80f, 143.00f, 143.50f }},
	{{ 240.45f, 241.92f, 143.00f, 143.50f }},
	{{ 242.71f, 244.13f, 143.00f, 143.50f }},
	{{ 267.60f, 268.86f, 143.00f, 143.50f }},
	{{  73.43f,  74.87f, 143.50f, 144.00f }},
	{{  96.08f,  97.44f, 143.50f, 144.00f }},
	{{  98.26f,  99.63f, 143.50f, 144.00f }},
	{{ 240.60f, 242.09f, 143.50f, 144.00f }},
	{{ 242.60f, 244.10f, 143.50f, 144.00f }},
	{{ 267.50f, 268.79f, 143.50f, 144.00f }},
	{{  73.52f,  74.96f, 144.00f, 144.50f }},
	{{  96.07f,  97.81f, 144.00f, 144.50f }},
	{{  97.95f,  99.44f, 144.00f, 144.50f }},
	{{ 240.79f, 244.08f, 144.00f, 144.50f }},
	{{ 267.41f, 268.68f, 144.00f, 144.50f }},
	{{  73.61f,  75.04f, 144.50f, 145.00f }},
	{{  96.07f,  99.22f, 144.50f, 145.00f }},
	{{ 241.01f, 244.06f, 144.50f, 145.00f }},
	{{ 267.19f, 268.57f, 144.50f, 145.00f }},
	{{   0.05f,   1.36f, 145.00f, 145.50f }},
	{{  73.75f,  98.95f, 145.00f, 145.50f }},
	{{ 241.27f, 253.05f, 145.00f, 145.50f }},
	{{ 263.73f, 268.41f, 145.00f, 145.50f }},
	{{   0.04f,   1.35f, 145.50f, 149.00f }},
	{{  74.10f,  98.60f, 145.50f, 146.00f }},
	{{ 241.60f, 268.05f, 145.50f, 146.00f }},
	{{  75.83f,  97.58f, 146.00f, 146.50f }},
	{{ 242.73f, 267.12f, 146.00f, 146.50f }},
	{{ 298.54f, 300.00f, 148.50f, 149.00f }},
	{{   0.02f, 299.98f, 149.00f, 150.00f }},
	{{   0.01f, 299.95f, 150.00f, 150.50f }},
	{{   0.01f,   1.46f, 150.50f, 151.00f }},
	{{ 298.49f, 299.93f, 150.50f, 152.50f }},
	{{   0.01f,   1.40f, 151.00f, 153.50f }},
	{{ 298.55f, 299.82f, 152.50f, 153.50f }},
	{{   0.00f,   1.47f, 153.50f, 154.50f }},
	{{ 298.48f, 299.74f, 153.50f, 154.00f }},
	{{ 298.18f, 299.64f, 154.00f, 154.50f }},
	{{   0.17f, 119.16f, 154.50f, 155.00f }},
	{{ 297.69f, 299.42f, 154.50f, 155.00f }},
	{{   0.69f, 299.02f, 155.00f, 155.50f }},
	{{   1.56f, 298.18f, 155.50f, 156.00f }},
	{{   2.60f,   4.37f, 156.00f, 156.50f }},
	{{  28.36f,  30.02f, 156.00f, 156.50f }},
	{{ 269.67f, 271.33f, 156.00f, 156.50f }},
	{{ 295.34f, 297.17f, 156.00f, 156.50f }},
	{{   2.81f,   4.27f, 156.50f, 157.00f }},
	{{  28.38f,  29.92f, 156.50f, 157.00f }},
	{{ 269.83f, 271.24f, 156.50f, 157.00f }},
	{{ 295.43f, 296.95f, 156.50f, 157.00f }},
	{{   2.95f,   4.32f, 157.00f, 157.50f }},
	{{  28.41f,  29.83f, 157.00f, 157.50f }},
	{{ 269.94f, 271.25f, 157.00f, 157.50f }},
	{{ 295.40f, 296.80f, 157.00f, 157.50f }},
	{{   3.05f,   4.41f, 157.50f, 158.00f }},
	{{  28.43f,  29.73f, 157.50f, 158.00f }},
	{{ 270.02f, 271.32f, 157.50f, 158.00f }},
	{{ 295.35f, 296.68f, 157.50f, 158.00f }},
	{{   3.15f,   4.53f, 158.00f, 158.50f }},
	{{  28.45f,  29.62f, 158.00f, 158.50f }},
	{{ 270.10f, 271.41f, 158.00f, 158.50f }},
	{{ 295.29f, 296.56f, 158.00f, 158.50f }},
	{{   3.26f,   4.79f, 158.50f, 159.00f }},
	{{  27.58f,  29.50f, 158.50f, 159.00f }},
	{{ 270.21f, 271.70f, 158.50f, 159.00f }},
	{{ 294.41f, 296.43f, 158.50f, 159.00f }},
	{{   3.42f,  29.34f, 159.00f, 159.50f }},
	{{ 270.38f, 296.25f, 159.00f, 159.50f }},
	{{   3.68f,  29.00f, 159.50f, 160.00f }},
	{{ 270.73f, 295.92f, 159.50f, 160.00f }},
	{{   4.09f,  28.42f, 160.00f, 160.50f }},
	{{ 278.45f, 295.37f, 160.00f, 160.50f }},
}};

[[nodiscard]] static std::vector<uint8_t> buttonsHovered(gl::vec2 mouse)
{
	std::vector<uint8_t> result(NUM_BUTTONS); // false
	/*
	 * TODO
	auto mouseDPad = (mouse - centerDPad) * (1.0f / sizeDPad);
	if (insideRectangle(mouseDPad, Rectangle{{-1, -1}, {1, 1}}) &&
		(between(mouseDPad.x, -fractionDPad, fractionDPad) ||
		between(mouseDPad.y, -fractionDPad, fractionDPad))) { // mouse over d-pad
		bool t1 = mouseDPad.x <  mouseDPad.y;
		bool t2 = mouseDPad.x < -mouseDPad.y;
		result[UP]    = !t1 &&  t2;
		result[DOWN]  =  t1 && !t2;
		result[LEFT]  =  t1 &&  t2;
		result[RIGHT] = !t1 && !t2;
	}
	result[TRIG_A] = insideCircle(mouse, centerA, radius);
	result[TRIG_B] = insideCircle(mouse, centerB, radius);
	result[WHEEL_LEFT] = 0;
	result[WHEEL_RIGHT] = 0;
	 */
	return result;
}

static void draw(gl::vec2 scrnPos, std::span<uint8_t> hovered, int hoveredRow)
{
	auto* drawList = ImGui::GetWindowDrawList();
	auto color = getColor(imColor::TEXT);
	for (const auto& r : fill) {
		drawList->AddRectFilled(scrnPos + gl::vec2{r[0], r[2]}, scrnPos + gl::vec2{r[1], r[3]}, color);
	}
}

} // namespace joyhandle

void ImGuiSettings::paintJoystick(MSXMotherBoard& motherBoard)
{
	ImGui::SetNextWindowSize(gl::vec2{316, 323}, ImGuiCond_FirstUseEver);
	im::Window("Configure MSX joysticks", &showConfigureJoystick, [&]{
		ImGui::SetNextItemWidth(13.0f * ImGui::GetFontSize());
		im::Combo("Select joystick", joystickToGuiString(joystick).c_str(), [&]{
			for (const auto& j : xrange(6)) {
				if (ImGui::Selectable(joystickToGuiString(j).c_str())) {
					joystick = j;
				}
			}
		});

		const auto& joystickManager = manager.getReactor().getInputEventGenerator().getJoystickManager();
		const auto& controller = motherBoard.getMSXCommandController();
		auto* setting = dynamic_cast<StringSetting*>(controller.findSetting(settingName(joystick)));
		if (!setting) return;
		auto& interp = setting->getInterpreter();
		TclObject bindings = setting->getValue();

		gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
		gl::vec2 mouse = gl::vec2(ImGui::GetIO().MousePos) - scrnPos;

		// Check if buttons are hovered
		// bool msxOrMega = joystick < 2;
		auto hovered = joystick < 2 ? msxjoystick::buttonsHovered(mouse)
		             : joystick < 4 ? joymega    ::buttonsHovered(mouse)
									: joyhandle  ::buttonsHovered(mouse);
		const auto numButtons = hovered.size();
		using SP = std::span<const zstring_view>;
		auto keyNames = joystick < 2 ? SP{msxjoystick::keyNames}
		              : joystick < 4 ? SP{joymega    ::keyNames}
		                             : SP{joyhandle  ::keyNames};
		auto buttonNames = joystick < 2 ? SP{msxjoystick::buttonNames}
		                 : joystick < 4 ? SP{joymega    ::buttonNames}
		                                : SP{joyhandle  ::buttonNames};

		// Any joystick button clicked?
		std::optional<int> addAction;
		std::optional<int> removeAction;
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			for (auto i : xrange(numButtons)) {
				if (hovered[i]) addAction = narrow<int>(i);
			}
		}

		ImGui::Dummy(joystick < 2 ? msxjoystick::boundingBox // reserve space for joystick drawing
		           : joystick < 4 ? joymega    ::boundingBox
					              : joyhandle  ::boundingBox);

		// Draw table
		int hoveredRow = -1;
		const auto& style = ImGui::GetStyle();
		auto textHeight = ImGui::GetTextLineHeight();
		float rowHeight = 2.0f * style.FramePadding.y + textHeight;
		float bottomHeight = style.ItemSpacing.y + 2.0f * style.FramePadding.y + textHeight;
		im::Table("##joystick-table", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX, {0.0f, -bottomHeight}, [&]{
			im::ID_for_range(numButtons, [&](int i) {
				TclObject key(keyNames[i]);
				TclObject bindingList = bindings.getDictValue(interp, key);
				if (ImGui::TableNextColumn()) {
					auto pos = ImGui::GetCursorPos();
					ImGui::Selectable("##row", hovered[i], ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0, rowHeight));
					if (ImGui::IsItemHovered()) {
						hoveredRow = i;
					}

					ImGui::SetCursorPos(pos);
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted(buttonNames[i]);
				}
				if (ImGui::TableNextColumn()) {
					if (ImGui::Button("Add")) {
						addAction = i;
					}
					ImGui::SameLine();
					auto numBindings = bindingList.size();
					im::Disabled(numBindings == 0, [&]{
						if (ImGui::Button("Remove")) {
							if (numBindings == 1) {
								bindings.setDictValue(interp, key, TclObject{});
								setting->setValue(bindings);
							} else {
								removeAction = i;
							}
						}
					});
					ImGui::SameLine();
					if (numBindings == 0) {
						ImGui::TextDisabledUnformatted("no bindings");
					} else {
						size_t lastBindingIndex = numBindings - 1;
						size_t bindingIndex = 0;
						for (auto binding: bindingList) {
							ImGui::TextUnformatted(binding);
							simpleToolTip(toGuiString(*parseBooleanInput(binding), joystickManager));
							if (bindingIndex < lastBindingIndex) {
								ImGui::SameLine();
								ImGui::TextUnformatted("|"sv);
								ImGui::SameLine();
							}
							++bindingIndex;
						}
					}
				}
			});
		});
		  joystick < 2 ? msxjoystick::draw(scrnPos, hovered, hoveredRow)
		: joystick < 4 ? joymega    ::draw(scrnPos, hovered, hoveredRow)
		               : joyhandle  ::draw(scrnPos, hovered, hoveredRow);

		if (ImGui::Button("Default bindings...")) {
			ImGui::OpenPopup("bindings");
		}
		im::Popup("bindings", [&]{
			auto addOrSet = [&](auto getBindings) {
				if (ImGui::MenuItem("Add to current bindings")) {
					// merge 'newBindings' into 'bindings'
					auto newBindings = getBindings();
					for (auto k : xrange(int(numButtons))) {
						TclObject key(keyNames[k]);
						TclObject dstList = bindings.getDictValue(interp, key);
						TclObject srcList = newBindings.getDictValue(interp, key);
						// This is O(N^2), but that's fine (here).
						for (auto b : srcList) {
							if (!contains(dstList, b)) {
								dstList.addListElement(b);
							}
						}
						bindings.setDictValue(interp, key, dstList);
					}
					setting->setValue(bindings);
				}
				if (ImGui::MenuItem("Replace current bindings")) {
					setting->setValue(getBindings());
				}
			};
			im::Menu("Keyboard", [&]{
				addOrSet([] {
					return TclObject(TclObject::MakeDictTag{},
						"UP",    makeTclList("keyb Up"),
						"DOWN",  makeTclList("keyb Down"),
						"LEFT",  makeTclList("keyb Left"),
						"RIGHT", makeTclList("keyb Right"),
						"A",     makeTclList("keyb Space"),
						"B",     makeTclList("keyb M"));
				});
			});
			for (auto joyId : joystickManager.getConnectedJoysticks()) {
				im::Menu(joystickManager.getDisplayName(joyId).c_str(), [&]{
					addOrSet([&]{
						return joystick < 2 ? MSXJoystick::getDefaultConfig(joyId, joystickManager)
						     : joystick < 4 ? JoyMega::getDefaultConfig(joyId, joystickManager)
							                : JoyHandle::getDefaultConfig(joyId, joystickManager);
					});
				});
			}
		});

		// Popup for 'Add'
		static constexpr auto addTitle = "Waiting for input";
		if (addAction) {
			popupForKey = *addAction;
			popupTimeout = 5.0f;
			initListener();
			ImGui::OpenPopup(addTitle);
		}
		im::PopupModal(addTitle, nullptr, ImGuiWindowFlags_NoSavedSettings, [&]{
			auto close = [&]{
				ImGui::CloseCurrentPopup();
				popupForKey = unsigned(-1);
				deinitListener();
			};
			if (popupForKey >= numButtons) {
				close();
				return;
			}

			ImGui::Text("Enter event for joystick button '%s'", buttonNames[popupForKey].c_str());
			ImGui::Text("Or press ESC to cancel.  Timeout in %d seconds.", int(popupTimeout));

			popupTimeout -= ImGui::GetIO().DeltaTime;
			if (popupTimeout <= 0.0f) {
				close();
			}
		});

		// Popup for 'Remove'
		if (removeAction) {
			popupForKey = *removeAction;
			ImGui::OpenPopup("remove");
		}
		im::Popup("remove", [&]{
			auto close = [&]{
				ImGui::CloseCurrentPopup();
				popupForKey = unsigned(-1);
			};
			if (popupForKey >= numButtons) {
				close();
				return;
			}
			TclObject key(keyNames[popupForKey]);
			TclObject bindingList = bindings.getDictValue(interp, key);

			auto remove = size_t(-1);
			size_t counter = 0;
			for (const auto& b : bindingList) {
				if (ImGui::Selectable(b.c_str())) {
					remove = counter;
				}
				simpleToolTip(toGuiString(*parseBooleanInput(b), joystickManager));
				++counter;
			}
			if (remove != size_t(-1)) {
				bindingList.removeListIndex(interp, remove);
				bindings.setDictValue(interp, key, bindingList);
				setting->setValue(bindings);
				close();
			}

			if (ImGui::Selectable("all bindings")) {
				bindings.setDictValue(interp, key, TclObject{});
				setting->setValue(bindings);
				close();
			}
		});
	});
}

void ImGuiSettings::paintFont()
{
	im::Window("Select font", &showFont, [&]{
		auto selectFilename = [&](FilenameSetting& fileSetting, IntegerSetting& faceSetting, zstring_view name, float width) {
			auto currentFile = fileSetting.getString();
			auto currentFace = faceSetting.getInt();

			ImGui::SetNextItemWidth(width);
			im::Combo(tmpStrCat("##", fileSetting.getBaseName()).c_str(), name.c_str(), [&]{
				for (const auto& font : getAvailableFonts()) {
					bool isSelected = currentFile == font.filename &&
					                  currentFace == font.faceIndex;
					if (ImGui::Selectable(font.displayName.c_str(), isSelected)) {
						fileSetting.setString(font.filename);
						faceSetting.setInt(font.faceIndex);
					}
				}
			});
		};
		auto selectSize = [](IntegerSetting& setting) {
			auto display = [](int s) { return strCat(s); };
			auto current = setting.getInt();
			ImGui::SetNextItemWidth(4.0f * ImGui::GetFontSize());
			im::Combo(tmpStrCat("##", setting.getBaseName()).c_str(), display(current).c_str(), [&]{
				for (int size : {9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24, 26, 28, 30, 32}) {
					if (ImGui::Selectable(display(size).c_str(), current == size)) {
						setting.setInt(size);
					}
				}
			});
		};

		auto pos = ImGui::CalcTextSize("Monospace").x + 2.0f * ImGui::GetStyle().ItemSpacing.x;
		auto width = 12.0f * ImGui::GetFontSize(); // filename ComboBox (boxes are drawn with different font, but we want same width)

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Normal");
		ImGui::SameLine(pos);
		selectFilename(manager.fontPropFilename, manager.fontPropIndex, manager.fontPropName, width);
		ImGui::SameLine();
		selectSize(manager.fontPropSize);
		HelpMarker("You can install more fonts by copying .ttf file(s) to your \"<openmsx>/share/skins\" directory.");

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Monospace");
		ImGui::SameLine(pos);
		im::Font(manager.fontMono, [&]{
			selectFilename(manager.fontMonoFilename, manager.fontMonoIndex, manager.fontMonoName, width);
		});
		ImGui::SameLine();
		selectSize(manager.fontMonoSize);
		HelpMarker("Some GUI elements (e.g. the console) require a monospaced font.");
	});
}

[[nodiscard]] static std::string formatShortcutWithAnnotations(const Shortcuts::Shortcut& shortcut)
{
	auto result = getKeyChordName(shortcut.keyChord);
	// don't show the 'ALWAYS_xxx' values
	if (shortcut.type == Shortcuts::Type::GLOBAL) result += ", global";
	return result;
}

[[nodiscard]] static gl::vec2 buttonSize(std::string_view text, float defaultSize_)
{
	const auto& style = ImGui::GetStyle();
	auto textSize = ImGui::CalcTextSize(text).x + 2.0f * style.FramePadding.x;
	auto defaultSize = ImGui::GetFontSize() * defaultSize_;
	return {std::max(textSize, defaultSize), 0.0f};
}

void ImGuiSettings::paintEditShortcut()
{
	using enum Shortcuts::Type;

	bool editShortcutWindow = editShortcutId != Shortcuts::ID::INVALID;
	if (!editShortcutWindow) return;

	im::Window("Edit shortcut", &editShortcutWindow, ImGuiWindowFlags_AlwaysAutoResize, [&]{
		auto& shortcuts = manager.getShortcuts();
		auto shortcut = shortcuts.getShortcut(editShortcutId);

		im::Table("table", 2, [&]{
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

			if (ImGui::TableNextColumn()) {
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted("key");
			}
			static constexpr auto waitKeyTitle = "Waiting for key";
			if (ImGui::TableNextColumn()) {
				auto text = getKeyChordName(shortcut.keyChord);
				if (ImGui::Button(text.c_str(), buttonSize(text, 4.0f))) {
					popupTimeout = 10.0f;
					centerNextWindowOverCurrent();
					ImGui::OpenPopup(waitKeyTitle);
				}
			}
			bool isOpen = true;
			im::PopupModal(waitKeyTitle, &isOpen, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize, [&]{
				ImGui::Text("Enter key combination for shortcut '%s'",
					Shortcuts::getShortcutDescription(editShortcutId).c_str());
				ImGui::Text("Timeout in %d seconds.", int(popupTimeout));

				popupTimeout -= ImGui::GetIO().DeltaTime;
				if (!isOpen || popupTimeout <= 0.0f) {
					ImGui::CloseCurrentPopup();
				}
				if (auto keyChord = getCurrentlyPressedKeyChord(); keyChord != ImGuiKey_None) {
					shortcut.keyChord = keyChord;
					shortcuts.setShortcut(editShortcutId, shortcut);
					editShortcutWindow = false;
					ImGui::CloseCurrentPopup();
				}
			});

			if (shortcut.type == one_of(LOCAL, GLOBAL)) { // don't edit the 'ALWAYS_xxx' values
				if (ImGui::TableNextColumn()) {
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted("global");
				}
				if (ImGui::TableNextColumn()) {
					bool global = shortcut.type == GLOBAL;
					if (ImGui::Checkbox("##global", &global)) {
						shortcut.type = global ? GLOBAL : LOCAL;
						shortcuts.setShortcut(editShortcutId, shortcut);
					}
					simpleToolTip(
						"Global shortcuts react when any GUI window has focus.\n"
						"Local shortcuts only react when the specific GUI window has focus.\n"sv);
				}
			}
		});
		ImGui::Separator();
		const auto& defaultShortcut = Shortcuts::getDefaultShortcut(editShortcutId);
		im::Disabled(shortcut == defaultShortcut, [&]{
			if (ImGui::Button("Restore default")) {
				shortcuts.setShortcut(editShortcutId, defaultShortcut);
				editShortcutWindow = false;
			}
			simpleToolTip([&]{ return formatShortcutWithAnnotations(defaultShortcut); });
		});

		ImGui::SameLine();
		im::Disabled(shortcut == Shortcuts::Shortcut{}, [&]{
			if (ImGui::Button("Set None")) {
				shortcuts.setShortcut(editShortcutId, Shortcuts::Shortcut{});
				editShortcutWindow = false;
			}
			simpleToolTip("Set no binding for this shortcut"sv);
		});
	});
	if (!editShortcutWindow) editShortcutId = Shortcuts::ID::INVALID;
}

void ImGuiSettings::paintShortcut()
{
	im::Window("Edit shortcuts", &showShortcut, [&]{
		int flags = ImGuiTableFlags_Resizable
		          | ImGuiTableFlags_RowBg
		          | ImGuiTableFlags_NoBordersInBodyUntilResize
		          | ImGuiTableFlags_SizingStretchProp;
		im::Table("table", 2, flags, {-FLT_MIN, 0.0f}, [&]{
			ImGui::TableSetupColumn("description");
			ImGui::TableSetupColumn("key");

			const auto& shortcuts = manager.getShortcuts();
			im::ID_for_range(std::to_underlying(Shortcuts::ID::NUM), [&](int i) {
				auto id = static_cast<Shortcuts::ID>(i);
				auto shortcut = shortcuts.getShortcut(id);

				if (ImGui::TableNextColumn()) {
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted(Shortcuts::getShortcutDescription(id));
				}
				if (ImGui::TableNextColumn()) {
					auto text = formatShortcutWithAnnotations(shortcut);
					if (ImGui::Button(text.c_str(), buttonSize(text, 9.0f))) {
						editShortcutId = id;
						centerNextWindowOverCurrent();
					}
				}
			});
		});
	});
	paintEditShortcut();
}

void ImGuiSettings::paint(MSXMotherBoard* motherBoard)
{
	if (selectedStyle < 0) {
		// triggers when loading "imgui.ini" did not select a style
		selectedStyle = 0; // dark (also the default (recommended) Dear ImGui style)
		setStyle();
	}
	if (motherBoard && showConfigureJoystick) paintJoystick(*motherBoard);
	if (showFont) paintFont();
	if (showShortcut) paintShortcut();
}

std::span<const ImGuiSettings::FontInfo> ImGuiSettings::getAvailableFonts()
{
	if (availableFonts.empty()) {
		// collect font files
		std::vector<std::string> files;
		const auto& context = systemFileContext();
		for (const auto& path : context.getPaths()) {
			foreach_file(FileOperations::join(path, "skins"), [&](const std::string& /*fullName*/, std::string_view name) {
				auto nameGz = name.ends_with(".gz") ? name.substr(0, name.size() - 3) : name;
				if (nameGz.ends_with(".ttf") || nameGz.ends_with(".ttc") || nameGz.ends_with(".otf")) {
					files.emplace_back(name);
				}
			});
		}
		// sort and remove duplicates
		std::ranges::sort(files);
		auto u = std::ranges::unique(files);
		files.erase(u.begin(), u.end());

		// create FontInfo objects
		for (const auto& file : files) {
			try {
				auto resolved = context.resolve(FileOperations::join("skins", file));
				auto fontData = File(resolved).mmap<uint8_t>();
				for (auto i : xrange(getNumFaces(manager.freeTypeLibrary, fontData))) {
					FontInfo info;
					info.filename = file;
					info.displayName = manager.getFontDisplayName(fontData, i, file);
					info.faceIndex = narrow<int>(i);
					availableFonts.emplace_back(std::move(info));
				}
			} catch (MSXException&) {
				// ignore invalid font files
			}
		}
	}
	return availableFonts;
}

bool ImGuiSettings::signalEvent(const Event& event)
{
	// bool msxOrMega = joystick < 2;
	using SP = std::span<const zstring_view>;
	auto keyNames = joystick < 2 ? SP{msxjoystick::keyNames}
	              : joystick < 4 ? SP{joymega    ::keyNames}
				                 : SP{joyhandle  ::keyNames};
	if (const auto numButtons = keyNames.size(); popupForKey >= numButtons) {
		deinitListener();
		return false; // don't block
	}

	bool escape = false;
	if (const auto* keyDown = get_event_if<KeyDownEvent>(event)) {
		escape = keyDown->getKeyCode() == SDLK_ESCAPE;
	}
	if (!escape) {
		auto getJoyDeadZone = [&](JoystickId joyId) {
			const auto& joyMan = manager.getReactor().getInputEventGenerator().getJoystickManager();
			const auto* setting = joyMan.getJoyDeadZoneSetting(joyId);
			return setting ? setting->getInt() : 0;
		};
		auto b = captureBooleanInput(event, getJoyDeadZone);
		if (!b) return true; // keep popup active
		auto bs = toString(*b);

		auto* motherBoard = manager.getReactor().getMotherBoard();
		if (!motherBoard) return true;
		const auto& controller = motherBoard->getMSXCommandController();
		auto* setting = dynamic_cast<StringSetting*>(controller.findSetting(settingName(joystick)));
		if (!setting) return true;
		auto& interp = setting->getInterpreter();

		TclObject bindings = setting->getValue();
		TclObject key(keyNames[popupForKey]);
		TclObject bindingList = bindings.getDictValue(interp, key);

		if (!contains(bindingList, bs)) {
			bindingList.addListElement(bs);
			bindings.setDictValue(interp, key, bindingList);
			setting->setValue(bindings);
		}
	}

	popupForKey = unsigned(-1); // close popup
	return true; // block event
}

void ImGuiSettings::initListener()
{
	if (listening) return;
	listening = true;

	auto& distributor = manager.getReactor().getEventDistributor();
	// highest priority (higher than HOTKEY and IMGUI)
	using enum EventType;
	for (auto type : {KEY_DOWN, MOUSE_BUTTON_DOWN,
	                  JOY_BUTTON_DOWN, JOY_HAT, JOY_AXIS_MOTION}) {
		distributor.registerEventListener(type, *this);
	}
}

void ImGuiSettings::deinitListener()
{
	if (!listening) return;
	listening = false;

	auto& distributor = manager.getReactor().getEventDistributor();
	using enum EventType;
	for (auto type : {JOY_AXIS_MOTION, JOY_HAT, JOY_BUTTON_DOWN,
	                  MOUSE_BUTTON_DOWN, KEY_DOWN}) {
		distributor.unregisterEventListener(type, *this);
	}
}

} // namespace openmsx
