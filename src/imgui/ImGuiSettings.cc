#include "ImGuiSettings.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "BooleanSetting.hh"
#include "CPUCore.hh"
#include "Display.hh"
#include "EventDistributor.hh"
#include "FilenameSetting.hh"
#include "FloatSetting.hh"
#include "GlobalCommandController.hh"
#include "GlobalSettings.hh"
#include "InputEventFactory.hh"
#include "IntegerSetting.hh"
#include "KeyCodeSetting.hh"
#include "KeyboardSettings.hh"
#include "Mixer.hh"
#include "MSXCPU.hh"
#include "MSXCommandController.hh"
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
#include "join.hh"
#include "narrow.hh"
#include "view.hh"
#include "StringOp.hh"
#include "zstring_view.hh"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <optional>

using namespace std::literals;

namespace openmsx {

ImGuiSettings::~ImGuiSettings()
{
	deinitListener();
}

void ImGuiSettings::showMenu(MSXMotherBoard* motherBoard)
{
	im::Menu("Settings", [&]{
		auto& reactor = manager.getReactor();
		auto& globalSettings = reactor.getGlobalSettings();
		auto& renderSettings = reactor.getDisplay().getRenderSettings();
		auto& settingsManager = reactor.getGlobalCommandController().getSettingsManager();

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
					static constexpr std::array algoEnables = {
						//                                        scanline / blur
						AlgoEnable{RenderSettings::SCALER_SIMPLE,     true,  true },
						AlgoEnable{RenderSettings::SCALER_SCALE,      false, false},
						AlgoEnable{RenderSettings::SCALER_HQ,         false, false},
						AlgoEnable{RenderSettings::SCALER_HQLITE,     false, false},
						AlgoEnable{RenderSettings::SCALER_RGBTRIPLET, true,  true },
						AlgoEnable{RenderSettings::SCALER_TV,         true,  false},
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
				Checkbox("Deinterlace", renderSettings.getDeinterlaceSetting());
				Checkbox("Deflicker", renderSettings.getDeflickerSetting());
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
				if (motherBoard) {
					ComboBox("Video source to display", motherBoard->getVideoSource());
				}
				Checkbox("VSync", renderSettings.getVSyncSetting());
				SliderInt("Minimum frame-skip", renderSettings.getMinFrameSkipSetting()); // TODO: either leave out this setting, or add a tooltip like, "Leave on 0 unless you use a very slow device and want regular frame skipping");
				SliderInt("Maximum frame-skip", renderSettings.getMaxFrameSkipSetting()); // TODO: either leave out this setting or add a tooltip like  "On slow devices, skip no more than this amount of frames to keep emulation on time.");
			});
			im::TreeNode("Advanced (for debugging)", [&]{ // default collapsed
				Checkbox("Enforce VDP sprites-per-line limit", renderSettings.getLimitSpritesSetting());
				Checkbox("Disable sprites", renderSettings.getDisableSpritesSetting());
				ComboBox("Way to handle too fast VDP access", renderSettings.getTooFastAccessSetting());
				ComboBox("Emulate VDP command timing", renderSettings.getCmdTimingSetting());
			});
		});
		im::Menu("Sound", [&]{
			auto& mixer = reactor.getMixer();
			auto& muteSetting = mixer.getMuteSetting();
			im::Disabled(muteSetting.getBoolean(), [&]{
				SliderInt("Master volume", mixer.getMasterVolume());
			});
			Checkbox("Mute", muteSetting);
			ImGui::Separator();
			static constexpr std::array resamplerToolTips = {
				EnumToolTip{"hq",   "best quality, uses more CPU"},
				EnumToolTip{"blip", "good speed/quality tradeoff"},
				EnumToolTip{"fast", "fast but low quality"},
			};
			ComboBox("Resampler", globalSettings.getResampleSetting(), resamplerToolTips);
			ImGui::Separator();

			ImGui::MenuItem("Show sound chip settings", nullptr, &manager.soundChip.showSoundChipSettings);
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
				auto fastForwardShortCut = getShortCutForCommand(reactor.getHotKey(), "toggle fastforward");
				if (!fastForwardShortCut.empty()) {
					HelpMarker(strCat("Use '", fastForwardShortCut ,"' to quickly toggle between these two"));
				}
				if (fwdChanged) {
					fwdSetting.setBoolean(fastForward != 0);
				}
				im::Indent([&]{
					im::Disabled(fastForward != 0, [&]{
						SliderInt("Speed (%)", speedManager.getSpeedSetting());
					});
					im::Disabled(fastForward != 1, [&]{
						SliderInt("Fast forward speed (%)", speedManager.getFastForwardSpeedSetting());
					});
				});
				Checkbox("Go full speed when loading", globalSettings.getThrottleManager().getFullSpeedLoadingSetting());
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
		im::Menu("Misc", [&]{
			static constexpr std::array kbdModeToolTips = {
				EnumToolTip{"CHARACTER",  "Tries to understand the character you are typing and then attempts to type that character using the current MSX keyboard. May not work very well when using a non-US host keyboard."},
				EnumToolTip{"KEY",        "Tries to map a key you press to the corresponding MSX key"},
				EnumToolTip{"POSITIONAL", "Tries to map the keyboard key positions to the MSX keyboard key positions"},
			};
			if (motherBoard) {
				auto& controller = motherBoard->getMSXCommandController();
				if (auto* mappingModeSetting = dynamic_cast<EnumSetting<KeyboardSettings::MappingMode>*>(controller.findSetting("kbd_mapping_mode"))) {
					ComboBox("Keyboard mapping mode", *mappingModeSetting, kbdModeToolTips);
				}
			};

			ImGui::MenuItem("Configure OSD icons...", nullptr, &manager.osdIcons.showConfigureIcons);
			ImGui::MenuItem("Fade out menu bar", nullptr, &manager.menuFade);
			ImGui::MenuItem("Configure messages ...", nullptr, &manager.messages.showConfigure);
			// TODO placeholder, move to appropriate location
			ImGui::MenuItem("Configure joystick ...", nullptr, &showConfigureJoystick);
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
					Checkbox(*bSetting);
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
			if (!Version::RELEASE) {
				ImGui::Separator();
				ImGui::Checkbox("ImGui Demo Window", &showDemoWindow);
				HelpMarker("Show the ImGui demo window.\n"
					"This is purely to demonstrate the ImGui capabilities.\n"
					"There is no connection with any openMSX functionality.");
			}
		});
	});
	if (showDemoWindow) {
		ImGui::ShowDemoWindow(&showDemoWindow);
	}
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
[[nodiscard]] static bool insideRectangle(gl::vec2 mouse, gl::vec2 topLeft, gl::vec2 bottomRight)
{
	return between(mouse[0], topLeft[0], bottomRight[0]) &&
	       between(mouse[1], topLeft[1], bottomRight[1]);
}

void ImGuiSettings::paintJoystick()
{
	// Customize joystick look
	static constexpr auto white = uint32_t(0xffffffff);
	static constexpr auto thickness = 3.0f;
	static constexpr auto joystickSize = gl::vec2{300.0f, 100.0f};
	static constexpr auto radius = 20.0f;
	static constexpr auto corner = 10.0f;
	static constexpr auto centerA = gl::vec2{200.0f, 50.0f};
	static constexpr auto centerB = gl::vec2{260.0f, 50.0f};
	static constexpr auto centerDPad = gl::vec2{50.0f, 50.0f};
	static constexpr auto sizeDPad = 30.0f;
	static constexpr auto fractionDPad = 1.0f / 3.0f;

	static constexpr std::array<zstring_view, NUM_BUTTONS> buttonNames = {
		"Up", "Down", "Left", "Right", "A", "B"
	};

	ImGui::SetNextWindowSize(gl::vec2{316, 278}, ImGuiCond_FirstUseEver);
	im::Window("Configure joystick", &showConfigureJoystick, [&]{
		auto* drawList = ImGui::GetWindowDrawList();
		gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
		gl::vec2 mouse = gl::vec2(ImGui::GetIO().MousePos) - scrnPos;

		// Check if buttons are hovered
		std::array<bool, NUM_BUTTONS> hovered = {}; // false
		auto mouseDPad = (mouse - centerDPad) * (1.0f / sizeDPad);
		if (insideRectangle(mouseDPad, {-1, -1}, {1, 1}) &&
		    (between(mouseDPad[0], -fractionDPad, fractionDPad) ||
		     between(mouseDPad[1], -fractionDPad, fractionDPad))) { // mouse over d-pad
			bool t1 = mouseDPad[0] <  mouseDPad[1];
			bool t2 = mouseDPad[0] < -mouseDPad[1];
			hovered[UP]    = !t1 &&  t2;
			hovered[DOWN]  =  t1 && !t2;
			hovered[LEFT]  =  t1 &&  t2;
			hovered[RIGHT] = !t1 && !t2;
		}
		hovered[TRIG_A] = insideCircle(mouse, centerA, radius);
		hovered[TRIG_B] = insideCircle(mouse, centerB, radius);

		// Any joystick button clicked?
		std::optional<int> addAction;
		std::optional<int> removeAction;
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			for (auto i : xrange(size_t(NUM_BUTTONS))) {
				if (hovered[i]) addAction = narrow<int>(i);
			}
		}

		ImGui::Dummy(joystickSize); // reserve space for joystick drawing

		// Draw table
		auto hoveredRow = size_t(-1);
		const auto& style = ImGui::GetStyle();
		auto textHeight = ImGui::GetTextLineHeight();
		float rowHeight = 2.0f * style.FramePadding.y + textHeight;
		im::Table("##joystick-table", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX, [&]{
			for (auto i : xrange(size_t(NUM_BUTTONS))) {
				im::ID(i, [&]{
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
						auto& bind = bindings[i];
						if (ImGui::Button("Add")) {
							addAction = i;
						}
						ImGui::SameLine();
						im::Disabled(bind.empty(), [&]{
							if (ImGui::Button("Remove")) {
								if (bind.size() == 1) {
									bind.clear();
								} else {
									removeAction = i;
								}
							}
						});
						ImGui::SameLine();
						if (bind.empty()) {
							ImGui::TextDisabled("no bindings");
						} else {
							ImGui::TextUnformatted(
								join(view::transform(bind, [](const auto& b) { return toString(b); }),
								     " | "));
						}
					}
				});
			}
		});

		// Draw joystick
		auto scrnCenterDPad = scrnPos + centerDPad;
		const auto F = fractionDPad;
		std::array<std::array<ImVec2, 5 + 1>, NUM_DIRECTIONS> dPadPoints = {
			std::array<ImVec2, 5 + 1>{ // UP
				scrnCenterDPad + sizeDPad * gl::vec2{ 0,  0},
				scrnCenterDPad + sizeDPad * gl::vec2{-F, -F},
				scrnCenterDPad + sizeDPad * gl::vec2{-F, -1},
				scrnCenterDPad + sizeDPad * gl::vec2{ F, -1},
				scrnCenterDPad + sizeDPad * gl::vec2{ F, -F},
				scrnCenterDPad + sizeDPad * gl::vec2{ 0,  0},
			},
			std::array<ImVec2, 5 + 1>{ // DOWN
				scrnCenterDPad + sizeDPad * gl::vec2{ 0,  0},
				scrnCenterDPad + sizeDPad * gl::vec2{ F,  F},
				scrnCenterDPad + sizeDPad * gl::vec2{ F,  1},
				scrnCenterDPad + sizeDPad * gl::vec2{-F,  1},
				scrnCenterDPad + sizeDPad * gl::vec2{-F,  F},
				scrnCenterDPad + sizeDPad * gl::vec2{ 0,  0},
			},
			std::array<ImVec2, 5 + 1>{ // LEFT
				scrnCenterDPad + sizeDPad * gl::vec2{ 0,  0},
				scrnCenterDPad + sizeDPad * gl::vec2{-F,  F},
				scrnCenterDPad + sizeDPad * gl::vec2{-1,  F},
				scrnCenterDPad + sizeDPad * gl::vec2{-1, -F},
				scrnCenterDPad + sizeDPad * gl::vec2{-F, -F},
				scrnCenterDPad + sizeDPad * gl::vec2{ 0,  0},
			},
			std::array<ImVec2, 5 + 1>{ // RIGHT
				scrnCenterDPad + sizeDPad * gl::vec2{ 0,  0},
				scrnCenterDPad + sizeDPad * gl::vec2{ F, -F},
				scrnCenterDPad + sizeDPad * gl::vec2{ 1, -F},
				scrnCenterDPad + sizeDPad * gl::vec2{ 1,  F},
				scrnCenterDPad + sizeDPad * gl::vec2{ F,  F},
				scrnCenterDPad + sizeDPad * gl::vec2{ 0,  0},
			},
		};
		auto hoverColor = ImGui::GetColorU32(ImGuiCol_ButtonHovered);

		drawList->AddRect(scrnPos, scrnPos + joystickSize, white, corner, 0, thickness);

		for (auto i : xrange(size_t(NUM_DIRECTIONS))) {
			if (hovered[i] || (hoveredRow == i)) {
				drawList->AddConvexPolyFilled(dPadPoints[i].data(), 5, hoverColor);
			}
			drawList->AddPolyline(dPadPoints[i].data(), 5 + 1, white, 0, thickness);
		}

		auto scrnCenterA = scrnPos + centerA;
		if (hovered[TRIG_A] || (hoveredRow == TRIG_A)) {
			drawList->AddCircleFilled(scrnCenterA, radius, hoverColor);
		}
		drawList->AddCircle(scrnCenterA, radius, white, 0, thickness);

		auto scrnCenterB = scrnPos + centerB;
		if (hovered[TRIG_B] || (hoveredRow == TRIG_B)) {
			drawList->AddCircleFilled(scrnCenterB, radius, hoverColor);
		}
		drawList->AddCircle(scrnCenterB, radius, white, 0, thickness);

		// Popup for 'Add'
		static constexpr auto addTitle = "Waiting for input";
		if (addAction) {
			popupForKey = *addAction;
			popupTimeout = 5.0f;
			initListener();
			ImGui::OpenPopup(addTitle);
		}
		im::PopupModal(addTitle, [&]{
			auto close = [&]{
				ImGui::CloseCurrentPopup();
				popupForKey = unsigned(-1);
				deinitListener();
			};
			if (popupForKey >= NUM_BUTTONS) {
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
			if (popupForKey >= NUM_BUTTONS) {
				close();
				return;
			}
			auto& bind = bindings[popupForKey];

			size_t remove = size_t(-1);
			size_t counter = 0;
			for (const auto& b : bind) {
				if (ImGui::Selectable(toString(b).c_str())) {
					remove = counter;
				}
				++counter;
			}
			if (remove != size_t(-1)) {
				bind.erase(bind.begin() + remove);
				close();
			}

			if (ImGui::Selectable("--all--")) {
				bind.clear();
				close();
			}
		});
	});
}

void ImGuiSettings::paint(MSXMotherBoard* /*motherBoard*/)
{
	if (showConfigureJoystick) paintJoystick();
}

int ImGuiSettings::signalEvent(const Event& event_)
{
	if (popupForKey >= NUM_BUTTONS) {
		deinitListener();
		return 0;
	}

	auto event = event_; // copy
	bool escape = false;
	if (auto* keyDown = get_event_if<KeyDownEvent>(event)) {
		keyDown->clearUnicode();
		escape = keyDown->getKeyCode() == SDLK_ESCAPE;
	}
	if (!escape) {
		auto& bind = bindings[popupForKey];
		if (!contains(bind, event)) {
			bind.push_back(std::move(event));
		}
	}

	popupForKey = unsigned(-1);
	return EventDistributor::HOTKEY; // block event
}

void ImGuiSettings::initListener()
{
	if (listening) return;
	listening = true;

	auto& distributor = manager.getReactor().getEventDistributor();
	// highest priority (higher than HOTKEY and IMGUI)
	distributor.registerEventListener(EventType::KEY_DOWN, *this);
	distributor.registerEventListener(EventType::MOUSE_BUTTON_DOWN, *this);
	distributor.registerEventListener(EventType::MOUSE_WHEEL, *this);
	distributor.registerEventListener(EventType::JOY_HAT, *this);
	distributor.registerEventListener(EventType::JOY_BUTTON_DOWN, *this);
	distributor.registerEventListener(EventType::JOY_AXIS_MOTION, *this);
}

void ImGuiSettings::deinitListener()
{
	if (!listening) return;
	listening = false;

	auto& distributor = manager.getReactor().getEventDistributor();
	distributor.unregisterEventListener(EventType::JOY_AXIS_MOTION, *this);
	distributor.unregisterEventListener(EventType::JOY_BUTTON_DOWN, *this);
	distributor.unregisterEventListener(EventType::JOY_HAT, *this);
	distributor.unregisterEventListener(EventType::MOUSE_WHEEL, *this);
	distributor.unregisterEventListener(EventType::MOUSE_BUTTON_DOWN, *this);
	distributor.unregisterEventListener(EventType::KEY_DOWN, *this);
}

} // namespace openmsx
