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
#include "KeyCodeSetting.hh"
#include "KeyboardSettings.hh"
#include "Mixer.hh"
#include "MSXCPU.hh"
#include "MSXCommandController.hh"
#include "MSXJoystick.hh"
#include "MSXMotherBoard.hh"
#include "ProxySetting.hh"
#include "R800.hh"
#include "Reactor.hh"
#include "ReadOnlySetting.hh"
#include "SettingsManager.hh"
#include "StringSetting.hh"
#include "Version.hh"
#include "VideoSourceSetting.hh"
#include "Z80.hh"

#include "checked_cast.hh"
#include "foreach_file.hh"
#include "narrow.hh"
#include "StringOp.hh"
#include "unreachable.hh"
#include "zstring_view.hh"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <SDL.h>

#include <optional>

using namespace std::literals;

namespace openmsx {

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
	bool openConfirmPopup = false;

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
						//                 scanline / blur
						AlgoEnable{SIMPLE,     true,  true },
						AlgoEnable{SCALE,      false, false},
						AlgoEnable{HQ,         false, false},
						AlgoEnable{RGBTRIPLET, true,  true },
						AlgoEnable{TV,         true,  false},
					};
					auto it = ranges::find(algoEnables, scaler.getEnum(), &AlgoEnable::algo);
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
						ranges::replace(s, '_', ' ');
						return s;
					});
				}
			});
			im::TreeNode("Shape", ImGuiTreeNodeFlags_DefaultOpen, [&]{
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
				EnumToolTip{"hq",   "best quality, uses more CPU"},
				EnumToolTip{"blip", "good speed/quality tradeoff"},
				EnumToolTip{"fast", "fast but low quality"},
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
				EnumToolTip{"CHARACTER",  "Tries to understand the character you are typing and then attempts to type that character using the current MSX keyboard. May not work very well when using a non-US host keyboard."},
				EnumToolTip{"KEY",        "Tries to map a key you press to the corresponding MSX key"},
				EnumToolTip{"POSITIONAL", "Tries to map the keyboard key positions to the MSX keyboard key positions"},
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
			ImGui::MenuItem("Configure MSX joysticks...", nullptr, &showConfigureJoystick);
		});
		im::Menu("GUI", [&]{
			auto getExistingLayouts = [] {
				std::vector<std::string> names;
				for (auto context = userDataFileContext("layouts");
				     const auto& path : context.getPaths()) {
					foreach_file(path, [&](const std::string& fullName, std::string_view name) {
						if (name.ends_with(".ini")) {
							names.emplace_back(fullName);
						}
					});
				}
				ranges::sort(names, StringOp::caseless{});
				return names;
			};
			auto listExistingLayouts = [&](const std::vector<std::string>& names) {
				std::optional<std::pair<std::string, std::string>> selectedLayout;
				im::ListBox("##select-layout", [&]{
					for (const auto& name : names) {
						auto displayName = std::string(FileOperations::stripExtension(FileOperations::getFilename(name)));
						if (ImGui::Selectable(displayName.c_str())) {
							selectedLayout = std::pair{name, displayName};
						}
						im::PopupContextItem([&]{
							if (ImGui::MenuItem("delete")) {
								confirmText = strCat("Delete layout: ", displayName);
								confirmAction = [name]{ FileOperations::unlink(name); };
								openConfirmPopup = true;
							}
						});
					}
				});
				return selectedLayout;
			};
			im::Menu("Save layout", [&]{
				if (auto names = getExistingLayouts(); !names.empty()) {
					ImGui::TextUnformatted("Existing layouts"sv);
					if (auto selectedLayout = listExistingLayouts(names)) {
						const auto& [name, displayName] = *selectedLayout;
						saveLayoutName = displayName;
					}
				}
				ImGui::TextUnformatted("Enter name:"sv);
				ImGui::InputText("##save-layout-name", &saveLayoutName);
				ImGui::SameLine();
				im::Disabled(saveLayoutName.empty(), [&]{
					if (ImGui::Button("Save as")) {
						(void)reactor.getDisplay().getWindowPosition(); // to save up-to-date window position
						ImGui::CloseCurrentPopup();

						auto filename = FileOperations::parseCommandFileArgument(
							saveLayoutName, "layouts", "", ".ini");
						if (FileOperations::exists(filename)) {
							confirmText = strCat("Overwrite layout: ", saveLayoutName);
							confirmAction = [filename]{
								ImGui::SaveIniSettingsToDisk(filename.c_str());
							};
							openConfirmPopup = true;
						} else {
							ImGui::SaveIniSettingsToDisk(filename.c_str());
						}
					}
				});
			});
			im::Menu("Restore layout", [&]{
				ImGui::TextUnformatted("Select layout"sv);
				auto names = getExistingLayouts();
				if (auto selectedLayout = listExistingLayouts(names)) {
					const auto& [name, displayName] = *selectedLayout;
					manager.loadIniFile = name;
					ImGui::CloseCurrentPopup();
				}
			});
			im::Menu("Select style", [&]{
				std::optional<int> newStyle;
				static constexpr std::array names = {"Dark", "Light", "Classic"}; // must be in sync with setStyle()
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
			ImGui::MenuItem("Select font...", nullptr, &showFont);
			ImGui::MenuItem("Edit shortcuts...", nullptr, &showShortcut);
		});
		im::Menu("Misc", [&]{
			ImGui::MenuItem("Configure OSD icons...", nullptr, &manager.osdIcons->showConfigureIcons);
			ImGui::MenuItem("Fade out menu bar", nullptr, &manager.menuFade);
			ImGui::MenuItem("Show status bar", nullptr, &manager.statusBarVisible);
			ImGui::MenuItem("Configure messages...", nullptr, &manager.messages->configureWindow.open);
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
			ranges::sort(settings, StringOp::caseless{}, &Setting::getBaseName);
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

	const auto confirmTitle = "Confirm##settings";
	if (openConfirmPopup) {
		ImGui::OpenPopup(confirmTitle);
	}
	im::PopupModal(confirmTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize, [&]{
		ImGui::TextUnformatted(confirmText);

		bool close = false;
		if (ImGui::Button("Ok")) {
			confirmAction();
			close = true;
		}
		ImGui::SameLine();
		close |= ImGui::Button("Cancel");
		if (close) {
			ImGui::CloseCurrentPopup();
			confirmAction = {};
		}
	});
}

////// joystick stuff

// joystick is 0..3
[[nodiscard]] static std::string settingName(unsigned joystick)
{
	return (joystick < 2) ? strCat("msxjoystick", joystick + 1, "_config")
	                      : strCat("joymega", joystick - 1, "_config");
}

// joystick is 0..3
[[nodiscard]] static std::string joystickToGuiString(unsigned joystick)
{
	return (joystick < 2) ? strCat("MSX joystick ", joystick + 1)
	                      : strCat("JoyMega controller ", joystick - 1);
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
			              (a.getDirection() == BooleanJoystickAxis::Direction::POS ? "positive" : "negative"), " direction");
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

struct Rectangle {
	gl::vec2 topLeft;
	gl::vec2 bottomRight;
};
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
		drawList->AddPolyline(points[i].data(), 5 + 1, color, 0, thickness);
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
	drawList->AddRect(r.topLeft, r.bottomRight, color, corner, 0, thickness);
}

static void drawLetterA(gl::vec2 center)
{
	auto* drawList = ImGui::GetWindowDrawList();
	auto tr = [&](gl::vec2 p) { return center + p; };
	const std::array<ImVec2, 3> lines = { tr({-6, 7}), tr({0, -7}), tr({6, 7}) };
	auto color = getColor(imColor::TEXT);
	drawList->AddPolyline(lines.data(), lines.size(), color, 0, thickness);
	drawList->AddLine(tr({-3, 1}), tr({3, 1}), color, thickness);
}
static void drawLetterB(gl::vec2 center)
{
	auto* drawList = ImGui::GetWindowDrawList();
	auto tr = [&](gl::vec2 p) { return center + p; };
	const std::array<ImVec2, 4> lines = { tr({1, -7}), tr({-4, -7}), tr({-4, 7}), tr({2, 7}) };
	auto color = getColor(imColor::TEXT);
	drawList->AddPolyline(lines.data(), lines.size(), color, 0, thickness);
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
	drawList->AddPolyline(linesZ2.data(), 4, color, 0, thickness);
}

namespace msxjoystick {

enum {UP, DOWN, LEFT, RIGHT, TRIG_A, TRIG_B, NUM_BUTTONS};

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
	drawList->AddRect(scrnPos, scrnPos + boundingBox, color, corner, 0, thickness);

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

enum {UP, DOWN, LEFT, RIGHT,
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

void ImGuiSettings::paintJoystick(MSXMotherBoard& motherBoard)
{
	ImGui::SetNextWindowSize(gl::vec2{316, 323}, ImGuiCond_FirstUseEver);
	im::Window("Configure MSX joysticks", &showConfigureJoystick, [&]{
		ImGui::SetNextItemWidth(13.0f * ImGui::GetFontSize());
		im::Combo("Select joystick", joystickToGuiString(joystick).c_str(), [&]{
			for (const auto& j : xrange(4)) {
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
		bool msxOrMega = joystick < 2;
		auto hovered = msxOrMega ? msxjoystick::buttonsHovered(mouse)
		                         : joymega    ::buttonsHovered(mouse);
		const auto numButtons = hovered.size();
		using SP = std::span<const zstring_view>;
		auto keyNames = msxOrMega ? SP{msxjoystick::keyNames}
		                          : SP{joymega    ::keyNames};
		auto buttonNames = msxOrMega ? SP{msxjoystick::buttonNames}
		                             : SP{joymega    ::buttonNames};

		// Any joystick button clicked?
		std::optional<int> addAction;
		std::optional<int> removeAction;
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			for (auto i : xrange(numButtons)) {
				if (hovered[i]) addAction = narrow<int>(i);
			}
		}

		ImGui::Dummy(msxOrMega ? msxjoystick::boundingBox : joymega::boundingBox); // reserve space for joystick drawing

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
						ImGui::TextDisabled("no bindings");
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
		msxOrMega ? msxjoystick::draw(scrnPos, hovered, hoveredRow)
		          : joymega    ::draw(scrnPos, hovered, hoveredRow);

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
						return msxOrMega
							? MSXJoystick::getDefaultConfig(joyId, joystickManager)
							: JoyMega::getDefaultConfig(joyId, joystickManager);
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

			unsigned remove = -1u;
			unsigned counter = 0;
			for (const auto& b : bindingList) {
				if (ImGui::Selectable(b.c_str())) {
					remove = counter;
				}
				simpleToolTip(toGuiString(*parseBooleanInput(b), joystickManager));
				++counter;
			}
			if (remove != unsigned(-1)) {
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
		auto selectFilename = [&](FilenameSetting& setting, float width) {
			auto display = [](std::string_view name) {
				if (name.ends_with(".gz" )) name.remove_suffix(3);
				if (name.ends_with(".ttf")) name.remove_suffix(4);
				return std::string(name);
			};
			auto current = setting.getString();
			ImGui::SetNextItemWidth(width);
			im::Combo(tmpStrCat("##", setting.getBaseName()).c_str(), display(current).c_str(), [&]{
				for (const auto& font : getAvailableFonts()) {
					if (ImGui::Selectable(display(font).c_str(), current == font)) {
						setting.setString(font);
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
		selectFilename(manager.fontPropFilename, width);
		ImGui::SameLine();
		selectSize(manager.fontPropSize);
		HelpMarker("You can install more fonts by copying .ttf file(s) to your \"<openmsx>/share/skins\" directory.");

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Monospace");
		ImGui::SameLine(pos);
		im::Font(manager.fontMono, [&]{
			selectFilename(manager.fontMonoFilename, width);
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
			im::ID_for_range(to_underlying(Shortcuts::ID::NUM), [&](int i) {
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

std::span<const std::string> ImGuiSettings::getAvailableFonts()
{
	if (availableFonts.empty()) {
		for (const auto& context = systemFileContext();
		     const auto& path : context.getPaths()) {
			foreach_file(FileOperations::join(path, "skins"), [&](const std::string& /*fullName*/, std::string_view name) {
				if (name.ends_with(".ttf.gz") || name.ends_with(".ttf")) {
					availableFonts.emplace_back(name);
				}
			});
		}
		// sort and remove duplicates
		ranges::sort(availableFonts);
		availableFonts.erase(ranges::unique(availableFonts), end(availableFonts));
	}
	return availableFonts;
}

bool ImGuiSettings::signalEvent(const Event& event)
{
	bool msxOrMega = joystick < 2;
	using SP = std::span<const zstring_view>;
	auto keyNames = msxOrMega ? SP{msxjoystick::keyNames}
	                          : SP{joymega    ::keyNames};
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
