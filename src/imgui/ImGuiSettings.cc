#include "ImGuiSettings.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiMessages.hh"
#include "ImGuiOsdIcons.hh"
#include "ImGuiSoundChip.hh"
#include "ImGuiUtils.hh"
#include "VectorPath.hh"
#include "VectorPathDsl.hh"

#include "AnalogInput.hh"
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
#include <imgui_impl_opengl3.h>
#include <imgui_stdlib.h>

#include <SDL.h>

#include <algorithm>
#include <functional>
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
								if (ImGui::InputFloat(tmpStrCat("frequency (MHz)##", name).c_str(), &fval, 0.01f, 1.0f, "%.2f")) {
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

[[nodiscard]] static std::string toGuiString(const AnalogInput& input, const JoystickManager& joystickManager)
{
	return std::visit(overloaded{
		[](const AnalogMouseAxis& m) {
			return toString(m);
		},
		[&](const AnalogJoystickAxis& a) {
			return strCat(joystickManager.getDisplayName(a.getJoystick()),
			              " stick axis ", a.getAxis());
		}
	}, input);
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
[[nodiscard]] static std::string toGuiString(bool wheel, zstring_view str, const JoystickManager& joystickManager)
{
	return wheel ? toGuiString(*parseAnalogInput (str), joystickManager)
	             : toGuiString(*parseBooleanInput(str), joystickManager);
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
static constexpr auto centerA = gl::vec2{200.0f, 50.0f};
static constexpr auto centerB = gl::vec2{260.0f, 50.0f};
static constexpr auto centerDPad = gl::vec2{50.0f, 50.0f};
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
static constexpr auto selectBox = Rectangle{gl::vec2{130.0f, 60.0f}, gl::vec2{160.0f, 70.0f}};
static constexpr auto startBox  = Rectangle{gl::vec2{130.0f, 86.0f}, gl::vec2{160.0f, 96.0f}};
static constexpr auto radiusABC = 16.2f;
static constexpr auto radiusXYZ = 12.2f;
static constexpr auto centerDPad = gl::vec2{65.6f, 82.7f};
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
		gl::vec2{77.0f,  33.0f}, gl::vec2{ 69.2f, 0.0f},
		gl::vec2{54.8f, 135.2f}, gl::vec2{-66.9f, 0.0f},
		gl::vec2{77.0f,  33.0f}, gl::vec2{ 69.2f, 0.0f},
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
		gl::vec2{221.1f,  28.9f}, gl::vec2{ 80.1f, 0.0f},
		gl::vec2{236.9f, 139.5f}, gl::vec2{-76.8f, 0.0f},
		gl::vec2{221.1f,  28.9f}, gl::vec2{ 80.1f, 0.0f},
	};
	drawBezierCurve(buttonCurve);

	auto corner = (selectBox.bottomRight.y - selectBox.topLeft.y) * 0.5f;
	auto trR = [&](Rectangle r) { return Rectangle{tr(r.topLeft), tr(r.bottomRight)}; };
	drawFilledRectangle(trR(selectBox), corner, hovered[TRIG_SELECT] || (hoveredRow == TRIG_SELECT));
	drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), tr({123.0f, 46.0f}), color, "Select");
	drawFilledRectangle(trR(startBox), corner, hovered[TRIG_START] || (hoveredRow == TRIG_START));
	drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), tr({128.0f, 97.0f}), color, "Start");
}

} // namespace joymega

namespace joyhandle {

enum : uint8_t {
	UP, DOWN, LEFT, RIGHT,
	TRIG_A, TRIG_B, WHEEL,
	NUM_BUTTONS
};
static constexpr std::array<zstring_view, NUM_BUTTONS> buttonNames = { // show in the GUI
	"Up", "Down", "Left", "Right",
	"A", "B", "Wheel (analog)",
};
static constexpr std::array<zstring_view, NUM_BUTTONS> keyNames = { // keys in Tcl dict
	"UP", "DOWN", "LEFT", "RIGHT",
	"A", "B", "WHEEL",
};

[[nodiscard]] static std::vector<uint8_t> buttonsHovered(gl::vec2 mouse)
{
	std::vector<uint8_t> result(NUM_BUTTONS); // false
	auto mouse1 = mouse - gl::vec2{39, 149};
	if (insideCircle(mouse1, {}, 24)) {
		auto ax = std::abs(mouse1.x);
		auto ay = std::abs(mouse1.y);
		if (mouse1.y < 0 && ax < ay) result[UP   ] = true;
		if (mouse1.y > 0 && ax < ay) result[DOWN ] = true;
		if (mouse1.x < 0 && ay < ax) result[LEFT ] = true;
		if (mouse1.x > 0 && ay < ax) result[RIGHT] = true;

	}
	result[TRIG_A] = insideCircle(mouse, {77, 186}, 5)
	              || insideCircle(mouse, {319, 36}, 15);
	result[TRIG_B] = insideCircle(mouse, {63, 186}, 5)
	              || insideCircle(mouse, {134, 36}, 15);
	if (insideRectangle(mouse, {{110,80}, {343,190}})) result[WHEEL] = true;
	return result;
}

using namespace VectorPath::dsl;
static constexpr auto boundingBox = gl::vec2{385.0f, 242.0f};
static constexpr auto body = strokeCollection<int16_t>(
	openPath(from(698,220), line(712,220), curve(719,220, 722,223, 722,230), line(727,408),
	         line(731,454), line(731,463), curve(731,466, 728,468, 726,468), line(14,468),
	         curve(11,468, 9,465, 9,463), line(9,454), line(13,408), line(18,230),
	         curve(18,223, 25,220, 28,220), line(212,220)),
	openPath(from(9,454), line(731,454)),
	openPath(from(13,408), line(201,408)),
	openPath(from(270,408), line(640,408)),
	openPath(from(709,408), line(727,408)),

	openPath(from(20,224), line(69,168), curve(73,164, 76,164, 80,164), line(660,164),
	         curve(664,164, 667,164, 671,168), line(720,223)),

	openPath(from(84,468), line(84,477), curve(84,479, 83,480, 81,480), line(21,480),
	         curve(19,480, 18,479, 18,477), line(18,468)),
	openPath(from(488,468), line(488,477), curve(488,479, 487,480, 485,480), line(425,480),
	         curve(423,480, 422,479, 422,477), line(422,468)),
	openPath(from(722,468), line(722,477), curve(722,479, 721,480, 719,480), line(659,480),
	         curve(657,480, 656,479, 656,477), line(656,468)),

	closedPath(from(183,233), curve(186,233, 188,235, 188,238), line(188,387),
	           curve(188,390, 186,392, 183,392), line(31,392), curve(28,392, 26,390, 26,387),
	           line(26,238), curve(26,235, 28,233, 31,233)),

	closedPath(from(133,285), line(151,285), line(151,323), line(133,323)),
	openPath(from(138,299), line(146,299)),
	openPath(from(138,305), line(146,305)),

	closedPath(from(162,285), line(180,285), line(180,323), line(162,323)),
	openPath(from(167,299), line(175,299)),
	openPath(from(167,305), line(175,305)),

	closedPath(from(217,212), curve(214,212, 212,214, 212,217), line(201,392), line(201,444),
	           line(263,444), line(263,217), curve(263,214, 261,212, 258,212)),
	openPath(from(201,392), line(250,392), curve(253,392, 255,390, 255,387), line(255,212)),
	openPath(from(263,444), line(270,408), line(270,210), curve(270,207, 268,205, 265,205)),

	closedPath(from(693,212), curve(696,212, 698,214, 698,217), line(709,392), line(709,444),
	           line(647,444), line(647,217), curve(647,214, 649,212, 652,212)),
	openPath(from(709,392), line(660,392), curve(657,392, 655,390, 655,387), line(655,212)),
	openPath(from(647,444), line(640,408), line(640,210), curve(640,207, 642,205, 645,205)),

	openPath(from(212,217), line(213,210), curve(213,208, 215,205, 218,205), line(692,205),
	         curve(695,205, 697,208, 697,210), line(698,217)));
static constexpr auto wheelOutline = path<int16_t>(
	from(78,56),
	curve(75,64, 67,68, 57,68), line(-57,68), curve(-67,68, -75,64, -78,56), line(-156,72),
	curve(-186,78, -231,58, -231,19), line(-231,-21), curve(-236,-24, -240,-26, -240,-29),
	line(-214,-245), curve(-214,-257, -155,-255, -155,-243), line(-163,-27),
	curve(-163,-23, -168,-21, -175,-19), curve(-175,5, -167,17, -147,7), line(-72,-32),
	line(-71,-47), curve(-69,-60, -62,-65, -50,-65), line(50,-65),
	curve(62,-65, 69,-60, 71,-47), line(72,-32), line(147,7),
	curve(167,17, 175,5, 175,-19), curve(168,-21, 163,-23, 163,-27), line(155,-243),
	curve(155,-255, 214,-257, 214,-245), line(240,-29), curve(240,-26, 237,-24, 231,-21),
	line(231,22), curve(231,61, 186,78, 156,72));
static constexpr auto wheelInside = strokeCollection<int16_t>(
	openPath(from(-163,-27), curve(-163,-13, -240,-15, -240,-29)),
	openPath(from(163,-27), curve(163,-13, 240,-15, 240,-29)),
	closedPath(from(47,-51), curve(53,-51, 56,-49, 57,-43), line(63,40),
		curve(63,47, 61,51, 52,51), line(-52,51), curve(-61,51, -63,47, -63,40),
		line(-57,-43), curve(-56,-49, -53,-51, -47,-51)),
	openPath(from(-215,-18), line(-215,19), curve(-215,49, -185,60, -161,55), line(-79,40)),
	openPath(from(-74,-15), line(-148,23), curve(-175,33, -190,13, -190,-17)),
	openPath(from(-57,68), curve(-71,68, -80,61, -79,46), line(-72,-32)),
	openPath(from(215,-18), line(215,19), curve(215,49, 185,60, 161,55), line(79,40)),
	openPath(from(74,-15), line(148,23), curve(175,33, 190,13, 190,-17)),
	openPath(from(57,68), curve(71,68, 80,61, 79,46), line(72,-32)));
static constexpr auto wheelButtonA = path<int16_t>(
	from(155,-243),
	curve(155,-255, 214,-257, 214,-245),
	curve(214,-233, 155,-231, 155,-243));
static constexpr auto wheelButtonB = path<int16_t>(
	from(-155,-243),
	curve(-155,-231, -214,-233, -214,-245),
	curve(-214,-257, -155,-255, -155,-243));
static constexpr auto wheelButtonInside = strokeCollection<int16_t>(
	openPath(from(-164,-243), curve(-164,-236, -207,-238, -207,-245)),
	openPath(from(164,-243), curve(164,-236, 207,-238, 207,-245)));

static void draw(gl::vec2 scrnPos, std::span<uint8_t> hovered, int hoveredRow, int analogValue)
{
	static constexpr ImU32 eraseColor = IM_COL32(0, 0, 0, 1); // alpha=0 is optimized away by ImGui, instead use alpha=1
	auto color = getColor(imColor::TEXT);
	auto hoverColor = ImGui::GetColorU32(ImGuiCol_ButtonHovered);

	bool up      = hovered[UP    ] || hoveredRow == UP;
	bool down    = hovered[DOWN  ] || hoveredRow == DOWN;
	bool left    = hovered[LEFT  ] || hoveredRow == LEFT;
	bool right   = hovered[RIGHT ] || hoveredRow == RIGHT;
	bool buttonA = hovered[TRIG_A] || hoveredRow == TRIG_A;
	bool buttonB = hovered[TRIG_B] || hoveredRow == TRIG_B;
	bool wheel   = hovered[WHEEL ] || hoveredRow == WHEEL;

	gl::vec2 stickOffset{0};
	gl::vec2 stickRadius{1};
	if (up   ) { stickOffset.y = -17; stickRadius.y = 0.85f; }
	if (down ) { stickOffset.y =  17; stickRadius.y = 0.85f; }
	if (left ) { stickOffset.x = -17; stickRadius.x = 0.85f; }
	if (right) { stickOffset.x =  17; stickRadius.x = 0.85f; }
	auto stickColor = (up || down || left || right) ? hoverColor : eraseColor;

	auto wheelColor = wheel ? hoverColor : eraseColor;

	using namespace VectorPath;
	static constexpr float scale = 0.5f;
	static constexpr float thickness = 1.0f;

	// Create private draw list (not the window's)
	ImDrawList dl(ImGui::GetDrawListSharedData());
	dl._ResetForNewFrame();
	dl.PushTexture(ImGui::GetIO().Fonts->TexRef);
	gl::vec2 logicalSize = boundingBox;
	dl.PushClipRect({0, 0}, logicalSize);

	// populate draw list with steering wheel shapes
	ScaleTransform transform{scale};

	float angle = float(analogValue) * (30.0f / 180.0f * float(Math::pi) / 32768);
	auto wheelTransform = makeAffineTransform(angle, scale, gl::vec2{453, 317} * scale);

	drawPathStrokes<int16_t>(&dl, body, transform, color, thickness);
	static constexpr gl::vec2 stickCenter1{78, 298};
	dl.AddCircle(transform(stickCenter1), 44 * scale, color);

	// buttons in side panel
	if (buttonA) {
		dl.AddCircleFilled(transform(155, 372), 10 * scale, hoverColor);
	}
	if (buttonB) {
		dl.AddCircleFilled(transform(127, 372), 10 * scale, hoverColor);
	}
	dl.AddCircle(transform(155, 372), 10 * scale, color, 0, thickness);
	dl.AddCircle(transform(127, 372), 10 * scale, color, 0, thickness);

	auto stickCenter = transform(stickCenter1 + stickOffset);

	// draw outline of moving elements erasing the underlying stuff
	{
		dl.AddCallback([](const ImDrawList*, const ImDrawCmd*) {
				//glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				glBlendFunc(GL_ONE, GL_ZERO);
			}, nullptr);
		dl.AddEllipseFilled(stickCenter, 35 * scale * stickRadius, stickColor);
		drawPathFilled<int16_t>(&dl, wheelOutline, wheelTransform, wheelColor);
		dl.AddCallback([](const ImDrawList*, const ImDrawCmd*) {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}, nullptr);
	}

	dl.AddEllipse      (stickCenter, 35 * scale * stickRadius, color);
	dl.AddEllipseFilled(stickCenter,  5 * scale * stickRadius, color);

	drawPathStroke<int16_t>(&dl, wheelOutline, wheelTransform, color, thickness, ImDrawFlags_Closed);
	drawPathStrokes<int16_t>(&dl, wheelInside, wheelTransform, color, thickness);

	if (buttonA) {
		drawPathFilled<int16_t>(&dl, wheelButtonA, wheelTransform, hoverColor);
	}
	drawPathStroke<int16_t>(&dl, wheelButtonA, wheelTransform, color, thickness);
	if (buttonB) {
		drawPathFilled<int16_t>(&dl, wheelButtonB, wheelTransform, hoverColor);
	}
	drawPathStroke<int16_t>(&dl, wheelButtonB, wheelTransform, color, thickness);
	drawPathStrokes<int16_t>(&dl, wheelButtonInside, wheelTransform, color, thickness);

	dl.PopClipRect();

	// render draw list to texture
	auto fbScale = ImGui::GetIO().DisplayFramebufferScale;
	auto fbSize = logicalSize * fbScale;

	struct ShapeLayerRT {
		gl::ColorTexture tex{0, 0}; // start empty, resized on demand
		gl::FrameBufferObject fbo;

		void ensure(gl::ivec2 newSize) {
			if (tex.size() == newSize) return;

			tex.resize(newSize.x, newSize.y);
			tex.setInterpolation(true);
			fbo = gl::FrameBufferObject(tex);
		}
	};
	static ShapeLayerRT rt;
	rt.ensure(gl::ivec2(fbSize));

	ImDrawData dd;
	dd.Valid = true;
	dd.DisplayPos = {0, 0};
	dd.DisplaySize = logicalSize;
	dd.FramebufferScale = fbScale;
	dd.Textures = &ImGui::GetPlatformIO().Textures;
	dd.AddDrawList(&dl);

	rt.fbo.push();
	glViewport(0, 0, int(fbSize.x), int(fbSize.y));
	glClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(&dd); // fills texture NOW
	rt.fbo.pop();

	// Queue textured quad on the window draw list (drawn later with the rest of the UI)
	auto pos = ImGui::GetCursorPos();
	ImGui::SetCursorScreenPos(scrnPos);
	ImGui::GetWindowDrawList()->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
			// GL_ONE handles the color lines, GL_ONE_MINUS_SRC_ALPHA preserves transparency holes
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		}, nullptr);
	ImGui::Image(rt.tex.getImGui(), logicalSize, {0, 1}, {1, 0});
	ImGui::GetWindowDrawList()->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, nullptr);
	ImGui::SetCursorPos(pos);
}

} // namespace joyhandle

void ImGuiSettings::paintJoystick(MSXMotherBoard& motherBoard, JoystickManager& joystickManager)
{
	ImGui::SetNextWindowSize(gl::vec2{316, 323}, ImGuiCond_FirstUseEver);
	im::Window("Configure MSX joysticks", &showConfigureJoystick, [&]{
		ImGui::SetNextItemWidth(13.0f * ImGui::GetFontSize());
		bool justChanged = im::Combo("Select joystick", joystickToGuiString(joystick).c_str(), [&]{
			for (const auto& j : xrange(6)) {
				if (ImGui::Selectable(joystickToGuiString(j).c_str())) {
					joystick = j;
				}
			}
		});

		const auto& controller = motherBoard.getMSXCommandController();
		auto* setting = dynamic_cast<StringSetting*>(controller.findSetting(settingName(joystick)));
		if (!setting) return;
		auto& interp = setting->getInterpreter();
		TclObject bindings = setting->getValue();

		gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
		gl::vec2 mouse = gl::vec2(ImGui::GetIO().MousePos) - scrnPos;

		// Check if buttons are hovered
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
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !justChanged) {
			for (auto i : xrange(numButtons)) {
				if (hovered[i]) addAction = narrow<int>(i);
			}
		}

		ImGui::Dummy(joystick < 2 ? msxjoystick::boundingBox // reserve space for joystick drawing
		           : joystick < 4 ? joymega    ::boundingBox
					  : joyhandle  ::boundingBox);

		// Draw table
		bool anyWheel = false;
		int hoveredRow = -1;
		const auto& style = ImGui::GetStyle();
		auto textHeight = ImGui::GetTextLineHeight();
		float rowHeight = 2.0f * style.FramePadding.y + textHeight;
		float bottomHeight = style.ItemSpacing.y + 2.0f * style.FramePadding.y + textHeight;
		im::Table("##joystick-table", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX, {0.0f, -bottomHeight}, [&]{
			im::ID_for_range(numButtons, [&](int i) {
				TclObject key(keyNames[i]);
				bool wheel = key == "WHEEL";
				anyWheel |= wheel;
				TclObject bindingList = bindings.getDictValue(interp, key);
				if (wheel && analogBindings.empty()) {
					for (auto str : bindingList) {
						if (auto a = parseAnalogInput(str)) {
							analogBindings.emplace_back(*a, 0);
						}
					}
				}

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
						for (auto binding : bindingList) {
							ImGui::TextUnformatted(binding);
							simpleToolTip(toGuiString(wheel, binding, joystickManager));
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
		int analogValue = 0;
		if (anyWheel) {
			for (auto& [b, v] : analogBindings) {
				if (std::abs(v) > std::abs(analogValue)) {
					analogValue = v;
				}
			}
		} else {
			analogBindings.clear();
		}
		if (!analogBindings.empty()) {
			initListener();
		}

		  joystick < 2 ? msxjoystick::draw(scrnPos, hovered, hoveredRow)
		: joystick < 4 ? joymega    ::draw(scrnPos, hovered, hoveredRow)
		               : joyhandle  ::draw(scrnPos, hovered, hoveredRow, analogValue);

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
						     : joystick < 4 ? JoyMega    ::getDefaultConfig(joyId, joystickManager)
						                    : JoyHandle  ::getDefaultConfig(joyId, joystickManager);
					});
				});
			}
		});

		// Popup for 'Add'
		static constexpr auto addTitle = "Waiting for input";
		if (addAction) {
			addPopupForKey = *addAction;
			popupTimeout = 5.0f;
			initListener();
			ImGui::OpenPopup(addTitle);
		}
		im::PopupModal(addTitle, nullptr, ImGuiWindowFlags_NoSavedSettings, [&]{
			auto close = [&]{
				ImGui::CloseCurrentPopup();
				addPopupForKey = unsigned(-1);
				deinitListener();
			};
			if (addPopupForKey >= numButtons) {
				close();
				return;
			}

			ImGui::Text("Enter event for joystick button '%s'", buttonNames[addPopupForKey].c_str());
			ImGui::Text("Or press ESC to cancel.  Timeout in %d seconds.", int(popupTimeout));

			popupTimeout -= ImGui::GetIO().DeltaTime;
			if (popupTimeout <= 0.0f) {
				close();
			}
		});

		// Popup for 'Remove'
		if (removeAction) {
			removePopupForKey = *removeAction;
			ImGui::OpenPopup("remove");
		}
		im::Popup("remove", [&]{
			auto close = [&]{
				ImGui::CloseCurrentPopup();
				removePopupForKey = unsigned(-1);
			};
			if (removePopupForKey >= numButtons) {
				close();
				return;
			}
			TclObject key(keyNames[removePopupForKey]);
			TclObject bindingList = bindings.getDictValue(interp, key);
			bool wheel = key == "WHEEL";

			auto remove = size_t(-1);
			size_t counter = 0;
			for (const auto& b : bindingList) {
				if (ImGui::Selectable(b.c_str())) {
					remove = counter;
				}
				simpleToolTip(toGuiString(wheel, b, joystickManager));
				++counter;
			}
			if (remove != size_t(-1)) {
				bindingList.removeListIndex(interp, remove);
				bindings.setDictValue(interp, key, bindingList);
				setting->setValue(bindings);
				close();
				if (wheel) analogBindings.clear();
			}

			if (ImGui::Selectable("all bindings")) {
				bindings.setDictValue(interp, key, TclObject{});
				setting->setValue(bindings);
				close();
				analogBindings.clear();
			}
		});
		if (ImGui::Button("Mockup calibrate joystick axis")) showCalibrateJoystick = true;
	});
}

static void verticalText(ImDrawList* drawList, gl::vec2 pos, ImU32 color, std::string_view text)
{
	auto vtxIdxStart = drawList->_VtxCurrentIdx;
	drawList->AddText(pos, color, text.data(), text.data() + text.size());
	auto vtxIdxEnd = drawList->_VtxCurrentIdx;

	auto* verts = drawList->VtxBuffer.Data;
	for (auto i = vtxIdxStart; i < vtxIdxEnd; ++i) {
		auto offset = gl::vec2(verts[i].pos) - pos;
		verts[i].pos = pos + gl::vec2{offset.y, -offset.x};
	}
}

static float drawControlEdge(const char* id, gl::vec2 pos, float height, float dotY, float min, float max)
{
	ImGui::SetCursorScreenPos(pos - gl::vec2{2, 0});
	ImGui::InvisibleButton(id, gl::vec2(5, height));
	bool hovered = ImGui::IsItemHovered();
	bool active = ImGui::IsItemActive();
	auto current = pos.x;
	if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
		current = std::clamp(current + ImGui::GetIO().MouseDelta.x, min, max);
	}
	if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

	auto colSep    = ImGui::GetColorU32(ImGuiCol_Separator);
	auto colActive = ImGui::GetColorU32(ImGuiCol_SeparatorActive);
	auto colHover  = ImGui::GetColorU32(ImGuiCol_SeparatorHovered);
	auto* drawList = ImGui::GetWindowDrawList();
	auto color = active ? colActive : (hovered ? colHover : colSep);
	drawList->AddLine(pos, pos + gl::vec2{0, height}, color, hovered ? 3.0f : 1.0f);
	drawList->AddCircleFilled(pos + gl::vec2{0, dotY}, 10, color);
	return current;
}

static void generateCurvePoints(
	std::function<float(float)> func,
	std::function<gl::vec2(gl::vec2)> transform,
	float xStart, float xEnd,
	std::vector<ImVec2>& points)
{
	auto evalScreen = [&](float x) { return transform(gl::vec2{x, func(x)}); };

	std::function<void(float, gl::vec2, float, gl::vec2)> subDivide =
		[&](float x0, gl::vec2 p0, float x1, gl::vec2 p1) {
			float xm = (x0 + x1) * 0.5f;
			auto pMidCurve = evalScreen(xm);
			auto pMidLineY = (p0.y + p1.y) * 0.5f;
			float dy = pMidCurve.y - pMidLineY;

			if (std::abs(dy) > 2.0f) {
				// too large error, subdivide further
				subDivide(x0, p0, xm, pMidCurve); // left
				subDivide(xm, pMidCurve, x1, p1); // right
			} else {
				// flat enough, stop recursion
				points.emplace_back(pMidCurve);
				points.emplace_back(p1);
			}
		};
	auto pStart = evalScreen(xStart);
	auto pEnd   = evalScreen(xEnd);
	points.emplace_back(pStart);
	subDivide(xStart, pStart, xEnd, pEnd);
}

void ImGuiSettings::paintCalibrate(JoystickManager& joystickManager)
{
	static unsigned selectedJoystick = 0;
	static unsigned selectedAxis = 0;
	static float innerSetting  =  5000;
	static float middleSetting = 25000;
	static float outerSetting  = 30000;

	auto inner  = innerSetting  * (1.0f / 32767);
	auto middle = middleSetting * (1.0f / 32767);
	auto outer  = outerSetting  * (1.0f / 32767);
	auto f = std::log(0.5f) / std::log((middle - inner) / (outer - inner));

	auto s = ImGui::GetFontSize();
	ImGui::SetNextWindowSize(gl::vec2{32, 25} * s, ImGuiCond_FirstUseEver);
	im::Window("[Mockup] Calibrate joysticks", &showCalibrateJoystick, [&]{
		auto joysticks = joystickManager.getConnectedJoysticks();
		bool disabled = joysticks.empty();
		if (selectedJoystick >= joysticks.size()) selectedJoystick = 0;
		auto numAxes = disabled ? 0 : joystickManager.getNumAxes(joysticks[selectedJoystick]).value_or(0);
		if (selectedAxis > numAxes) selectedAxis = 0;
		auto axisValue = disabled ? 0 : joystickManager.getAxis(joysticks[selectedJoystick], int(selectedAxis)).value_or(0);

		im::Disabled(disabled, [&]{
			im::Table("##table", 2, [&]{
				ImGui::TableSetupColumn("joystick", ImGuiTableColumnFlags_WidthFixed, 18.0f * s);
				ImGui::TableSetupColumn("values");
				if (ImGui::TableNextColumn()) {
					auto selectedName = disabled
					                  ? "none available"
					                  : joystickManager.getDisplayName(joysticks[selectedJoystick]);
					im::Combo("Joystick", selectedName.c_str(), [&]{
						for (auto i : xrange(joysticks.size())) {
							auto name = joystickManager.getDisplayName(joysticks[selectedJoystick]);
							if (ImGui::Selectable(name.c_str()), selectedJoystick == i) {
								selectedJoystick = unsigned(i);
							}
						}
					});
					im::Indent([&]{
						auto axisName = [](unsigned i) {
							return i == unsigned(-1) ? "ALL" : strCat(i);
						};
						im::Combo("Axis", axisName(selectedAxis).c_str(), [&]{
							if (ImGui::Selectable("ALL", selectedAxis == unsigned(-1))) {
								selectedAxis = unsigned(-1);
							}
							for (auto i : xrange(numAxes)) {
								if (ImGui::Selectable(tmpStrCat(i).c_str(), selectedAxis == i)) {
									selectedAxis = i;
								}
							}
						});
					});
				}
				if (ImGui::TableNextColumn()) {
					ImGui::SetNextItemWidth(4.0f * s);
					if (ImGui::InputFloat("inner dead zone", &innerSetting, {}, {}, "%.0f")) {
						innerSetting = std::clamp(innerSetting, 0.0f, middleSetting - 1.0f);
					}
					ImGui::SetNextItemWidth(4.0f * s);
					if (ImGui::InputFloat("halfway value", &middleSetting, {}, {}, "%.0f")) {
						middleSetting = std::clamp(middleSetting, innerSetting + 1.0f, outerSetting - 1.0f);
					}
					ImGui::SetNextItemWidth(4.0f * s);
					if (ImGui::InputFloat("outer dead zone", &outerSetting, {}, {}, "%.0f")) {
						outerSetting = std::clamp(outerSetting, middleSetting + 1.0f, 32767.0f);
					}
				}
			});
		});

		//auto mouse = ImGui::GetIO().MousePos;
		gl::vec2 pos = ImGui::GetCursorScreenPos();
		gl::vec2 size = ImGui::GetContentRegionAvail();
		auto* drawList = ImGui::GetWindowDrawList();
		auto white = getColor(imColor::TEXT);
		auto gray  = getColor(imColor::TEXT_DISABLED);
		auto red   = getColor(imColor::ERROR);

		std::string_view xLabel = "raw input";
		std::string_view yLabel = "output";
		auto xTextSize = ImGui::CalcTextSize(xLabel);
		auto yTextSize = ImGui::CalcTextSize(yLabel);
		auto unit = xTextSize.y * 0.5f;
		auto origin = pos + gl::vec2{3 * unit, size.y - 3 * unit};
		drawList->AddText(pos + gl::vec2{(size.x - xTextSize.x) * 0.5f, size.y - xTextSize.y}, white, xLabel.data(), xLabel.data() + xLabel.size());
		verticalText(drawList, pos + gl::vec2{0, (size.y + yTextSize.x) * 0.5f}, white, yLabel);

		auto arrowRight = gl::vec2{pos.x + size.x, origin.y};
		auto arrowTop = gl::vec2{origin.x, pos.y};

		auto graphMin = origin.x + 4 * unit;
		auto graphMax = arrowRight.x - 4 * unit;
		auto fullWidth = graphMax - graphMin;

		auto graphLeft  = graphMin + inner  * fullWidth;
		auto graphMidX  = graphMin + middle * fullWidth;
		auto graphRight = graphMin + outer  * fullWidth;

		auto graphTop = arrowTop.y + 2 * unit;
		auto graphBottom = origin.y;
		auto graphMidY = (graphTop + graphBottom) * 0.5f;
		auto graphWidth = graphRight - graphLeft;
		auto graphHeight = graphBottom - graphTop;

		drawList->AddLine({origin.x, graphTop}, {arrowRight.x, graphTop}, gray);
		drawList->AddLine({origin.x, graphMidY}, {arrowRight.x, graphMidY}, gray);
		drawList->AddLine({graphMin, origin.y}, {graphMin, origin.y + unit}, gray);
		drawList->AddLine({graphMax, origin.y}, {graphMax, origin.y + unit}, gray);

		auto newLeft  = drawControlEdge("##inner",  {graphLeft,  graphTop}, graphHeight, graphHeight,        graphMin, graphMidX - 1);
		auto newMid   = drawControlEdge("##middle", {graphMidX,  graphTop}, graphHeight, graphHeight * 0.5f, graphLeft + 1, graphRight - 1);
		auto newRight = drawControlEdge("##outer",  {graphRight, graphTop}, graphHeight, 0,                  graphMidX + 1, graphMax);
		if (newLeft != graphLeft) {
			auto old = innerSetting;
			innerSetting  = std::clamp(((newLeft - graphMin) / fullWidth) * 32767.0f, 0.0f, 32767.0f);
			middleSetting = outerSetting - ((outerSetting - middleSetting) * (outerSetting - innerSetting) / (outerSetting - old));
		}
		if (newMid != graphMidX) {
			middleSetting = std::clamp(((newMid  - graphMin) / fullWidth) * 32767.0f, 0.0f, 32767.0f);
		}
		if (newRight != graphRight) {
			auto old = outerSetting;
			outerSetting  = std::clamp(((newRight - graphMin) / fullWidth) * 32767.0f, 0.0f, 32767.0f);
			middleSetting = innerSetting + ((middleSetting - innerSetting) * (outerSetting - innerSetting) / (old - innerSetting));
		}

		drawList->AddLine(arrowRight, origin - unit * gl::vec2{2, 0}, white, 2.0f);
		drawList->AddLine(arrowRight, arrowRight + unit * gl::vec2{-1, -1}, white, 2.0f);
		drawList->AddLine(arrowRight, arrowRight + unit * gl::vec2{-1,  1}, white, 2.0f);

		drawList->AddLine(arrowTop, origin + unit * gl::vec2{0, 2}, white, 2.0f);
		drawList->AddLine(arrowTop, arrowTop + unit * gl::vec2{-1, 1}, white, 2.0f);
		drawList->AddLine(arrowTop, arrowTop + unit * gl::vec2{ 1, 1}, white, 2.0f);

		//static constexpr int subDiv = 40;
		std::vector<ImVec2> points;
		points.reserve(40);
		points.emplace_back(origin);
		generateCurvePoints(
			[&](float x) { return std::pow(x, f); },
			[&](gl::vec2 p) { return p * gl::vec2{graphWidth, -graphHeight} + gl::vec2{graphLeft, origin.y}; },
			0.0f, 1.0f, points);
		points.emplace_back(arrowRight.x, graphTop);
		std::cerr << "points=" << points.size() << '\n';
		drawList->AddPolyline(points.data(), points.size(), white, 4.0f);

		auto input = float(axisValue) / 32768.0f; //std::clamp((mouse.x - graphMin) / fullWidth, 0.0f, 1.0f);
		auto output = pow(std::clamp((input - inner) / (outer - inner), 0.0f, 1.0f), f);
		auto cross = gl::vec2{graphMin + input * fullWidth, origin.y - output * graphHeight};
		drawList->AddLine({origin.x, cross.y}, gl::vec2{origin.x, cross.y} + unit * gl::vec2{1, -1}, red, 2.0f);
		drawList->AddLine({origin.x, cross.y}, gl::vec2{origin.x, cross.y} + unit * gl::vec2{1,  1}, red, 2.0f);
		drawList->AddLine({cross.x, origin.y}, gl::vec2{cross.x, origin.y} + unit * gl::vec2{-1, -1}, red, 2.0f);
		drawList->AddLine({cross.x, origin.y}, gl::vec2{cross.x, origin.y} + unit * gl::vec2{ 1, -1}, red, 2.0f);
		drawList->AddLine(cross + unit * gl::vec2{-1, -1}, cross + unit * gl::vec2{1,  1}, red, 2.0f);
		drawList->AddLine(cross + unit * gl::vec2{-1,  1}, cross + unit * gl::vec2{1, -1}, red, 2.0f);
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
	auto& joystickManager = manager.getReactor().getInputEventGenerator().getJoystickManager();
	if (motherBoard && showConfigureJoystick) paintJoystick(*motherBoard, joystickManager);
	if (showCalibrateJoystick) paintCalibrate(joystickManager);
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
	auto getJoyDeadZone = [&](JoystickId joyId) {
		const auto& joyMan = manager.getReactor().getInputEventGenerator().getJoystickManager();
		const auto* setting = joyMan.getJoyDeadZoneSetting(joyId);
		return setting ? setting->getInt() : 0;
	};

	for (auto& [binding, value] : analogBindings) {
		if (auto v = match(binding, event, getJoyDeadZone)) {
			value = *v;
		}
	}

	using SP = std::span<const zstring_view>;
	auto keyNames = joystick < 2 ? SP{msxjoystick::keyNames}
	              : joystick < 4 ? SP{joymega    ::keyNames}
	                             : SP{joyhandle  ::keyNames};
	if (const auto numButtons = keyNames.size(); addPopupForKey >= numButtons) {
		deinitListener();
		return false; // don't block
	}

	bool escape = false;
	if (const auto* keyDown = get_event_if<KeyDownEvent>(event)) {
		escape = keyDown->getKeyCode() == SDLK_ESCAPE;
	}
	if (!escape) {
		TclObject key(keyNames[addPopupForKey]);
		bool wheel = key == "WHEEL";

		std::string bs;
		if (wheel) {
			auto b = captureAnalogInput(event, getJoyDeadZone);
			if (!b) return true; // keep popup active
			bs = toString(*b);
		} else {
			auto b = captureBooleanInput(event, getJoyDeadZone);
			if (!b) return true; // keep popup active
			bs = toString(*b);
		}

		auto* motherBoard = manager.getReactor().getMotherBoard();
		if (!motherBoard) return true;
		const auto& controller = motherBoard->getMSXCommandController();
		auto* setting = dynamic_cast<StringSetting*>(controller.findSetting(settingName(joystick)));
		if (!setting) return true;
		auto& interp = setting->getInterpreter();

		TclObject bindings = setting->getValue();
		TclObject bindingList = bindings.getDictValue(interp, key);

		if (!contains(bindingList, bs)) {
			bindingList.addListElement(bs);
			bindings.setDictValue(interp, key, bindingList);
			setting->setValue(bindings);
			if (wheel) analogBindings.clear();
		}
	}

	addPopupForKey = unsigned(-1); // close popup
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
