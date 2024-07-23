#include "ImGuiReverseBar.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "FileContext.hh"
#include "FileOperations.hh"
#include "GLImage.hh"
#include "MSXMotherBoard.hh"
#include "ReverseManager.hh"
#include "GlobalCommandController.hh"

#include "foreach_file.hh"

#include <imgui.h>
#include <imgui_stdlib.h>

using namespace std::literals;

namespace openmsx {

void ImGuiReverseBar::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiReverseBar::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

void ImGuiReverseBar::showMenu(MSXMotherBoard* motherBoard)
{
	bool openConfirmPopup = false;

	auto stem = [&](std::string_view fullName) {
		return FileOperations::stripExtension(FileOperations::getFilename(fullName));
	};

	im::Menu("Save state", motherBoard != nullptr, [&]{
		const auto& hotKey = manager.getReactor().getHotKey();

		std::string_view loadCmd = "loadstate";
		auto loadShortCut = getShortCutForCommand(hotKey, loadCmd);
		if (ImGui::MenuItem("Quick load state", loadShortCut.c_str())) {
			manager.executeDelayed(makeTclList(loadCmd));
		}
		std::string_view saveCmd = "savestate";
		auto saveShortCut = getShortCutForCommand(hotKey, saveCmd);
		if (ImGui::MenuItem("Quick save state", saveShortCut.c_str())) {
			manager.executeDelayed(makeTclList(saveCmd));
		}
		ImGui::Separator();

		auto existingStates = manager.execute(TclObject("list_savestates"));
		im::Menu("Load state ...", existingStates && !existingStates->empty(), [&]{
			im::Table("table", 2, ImGuiTableFlags_BordersInnerV, [&]{
				if (ImGui::TableNextColumn()) {
					ImGui::TextUnformatted("Select save state"sv);
					im::ListBox("##list", ImVec2(ImGui::GetFontSize() * 20.0f, 240.0f), [&]{
						for (const auto& name : *existingStates) {
							if (ImGui::Selectable(name.c_str())) {
								manager.executeDelayed(makeTclList("loadstate", name));
							}
							if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) &&
							    (previewImage.name != name)) {
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
							im::PopupContextItem([&]{
								if (ImGui::MenuItem("delete")) {
									confirmCmd = makeTclList("delete_savestate", name);
									confirmText = strCat("Delete savestate '", name, "'?");
									openConfirmPopup = true;
								}
							});
						}
					});
				}
				if (ImGui::TableNextColumn()) {
					ImGui::TextUnformatted("Preview"sv);
					ImVec2 size(320, 240);
					if (previewImage.texture.get()) {
						ImGui::Image(previewImage.texture.getImGui(), size);
					} else {
						ImGui::Dummy(size);
					}
				}
			});
		});
		saveStateOpen = im::Menu("Save state ...", [&]{
			auto exists = [&]{
				auto filename = FileOperations::parseCommandFileArgument(
					saveStateName, "savestates", "", ".oms");
				return FileOperations::exists(filename);
			};
			if (!saveStateOpen) {
				// on each re-open of this menu, create a suggestion for a name
				if (auto result = manager.execute(makeTclList("guess_title", "savestate"))) {
					saveStateName = result->getString();
					if (exists()) {
						saveStateName = stem(FileOperations::getNextNumberedFileName(
							"savestates", result->getString(), ".oms", true));
					}
				}
			}
			ImGui::TextUnformatted("Enter name:"sv);
			ImGui::InputText("##save-state-name", &saveStateName);
			ImGui::SameLine();
			if (ImGui::Button("Create")) {
				ImGui::CloseCurrentPopup();
				confirmCmd = makeTclList("savestate", saveStateName);
				if (exists()) {
					openConfirmPopup = true;
					confirmText = strCat("Overwrite save state with name '", saveStateName, "'?");
				} else {
					manager.executeDelayed(confirmCmd);
				}
			}
		});
		if (ImGui::MenuItem("Open savestates folder...")) {
			SDL_OpenURL(strCat("file://", FileOperations::getUserOpenMSXDir(), "/savestates").c_str());
		}

		ImGui::Separator();

		const auto& reverseManager = motherBoard->getReverseManager();
		bool reverseEnabled = reverseManager.isCollecting();

		im::Menu("Load replay ...", reverseEnabled, [&]{
			ImGui::TextUnformatted("Select replay"sv);
			im::ListBox("##select-replay", [&]{
				struct Names {
					Names(std::string f, std::string d) // workaround, needed for clang, not gcc or msvc
						: fullName(std::move(f)), displayName(std::move(d)) {} // fixed in clang-16
					std::string fullName;
					std::string displayName;
				};
				std::vector<Names> names;
				for (auto context = userDataFileContext(ReverseManager::REPLAY_DIR);
				     const auto& path : context.getPaths()) {
					foreach_file(path, [&](const std::string& fullName, std::string_view name) {
						if (name.ends_with(ReverseManager::REPLAY_EXTENSION)) {
							name.remove_suffix(ReverseManager::REPLAY_EXTENSION.size());
							names.emplace_back(fullName, std::string(name));
						}
					});
				}
				ranges::sort(names, StringOp::caseless{}, &Names::displayName);
				for (const auto& [fullName_, displayName_] : names) {
					const auto& fullName = fullName_; // clang workaround
					const auto& displayName = displayName_; // clang workaround
					if (ImGui::Selectable(displayName.c_str())) {
						manager.executeDelayed(makeTclList("reverse", "loadreplay", fullName));
					}
					im::PopupContextItem([&]{
						if (ImGui::MenuItem("delete")) {
							confirmCmd = makeTclList("file", "delete", fullName);
							confirmText = strCat("Delete replay '", displayName, "'?");
							openConfirmPopup = true;
						}
					});
				}
			});
		});
		saveReplayOpen = im::Menu("Save replay ...", reverseEnabled, [&]{
			auto exists = [&]{
				auto filename = FileOperations::parseCommandFileArgument(
					saveReplayName, ReverseManager::REPLAY_DIR, "", ReverseManager::REPLAY_EXTENSION);
				return FileOperations::exists(filename);
			};
			if (!saveReplayOpen) {
				// on each re-open of this menu, create a suggestion for a name
				if (auto result = manager.execute(makeTclList("guess_title", "replay"))) {
					saveReplayName = result->getString();
					if (exists()) {
						saveReplayName = stem(FileOperations::getNextNumberedFileName(
							ReverseManager::REPLAY_DIR, result->getString(), ReverseManager::REPLAY_EXTENSION, true));
					}
				}
			}
			ImGui::TextUnformatted("Enter name:"sv);
			ImGui::InputText("##save-replay-name", &saveReplayName);
			ImGui::SameLine();
			if (ImGui::Button("Create")) {
				ImGui::CloseCurrentPopup();

				confirmCmd = makeTclList("reverse", "savereplay", saveReplayName);
				if (exists()) {
					openConfirmPopup = true;
					confirmText = strCat("Overwrite replay with name '", saveReplayName, "'?");
				} else {
					manager.executeDelayed(confirmCmd);
				}
			}
		});
		if (ImGui::MenuItem("Open replays folder...")) {
			SDL_OpenURL(strCat("file://", FileOperations::getUserOpenMSXDir(), '/', ReverseManager::REPLAY_DIR).c_str());
		}
		im::Menu("Reverse/replay settings", [&]{
			if (ImGui::MenuItem("Enable reverse/replay", nullptr, &reverseEnabled)) {
				manager.executeDelayed(makeTclList("reverse", reverseEnabled ? "start" : "stop"));
			}
			simpleToolTip("Enable/disable reverse/replay right now, for the currently running machine");
			if (auto* autoEnableReverseSetting = dynamic_cast<BooleanSetting*>(manager.getReactor().getGlobalCommandController().getSettingsManager().findSetting("auto_enable_reverse"))) {

				bool autoEnableReverse = autoEnableReverseSetting->getBoolean();
				if (ImGui::MenuItem("Auto enable reverse", nullptr, &autoEnableReverse)) {
					autoEnableReverseSetting->setBoolean(autoEnableReverse);
				}
				simpleToolTip(autoEnableReverseSetting->getDescription());
			}

			ImGui::MenuItem("Show reverse bar", nullptr, &showReverseBar, reverseEnabled);
		});
	});

	const auto popupTitle = "Confirm##reverse";
	if (openConfirmPopup) {
		ImGui::OpenPopup(popupTitle);
	}
	im::PopupModal(popupTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize, [&]{
		ImGui::TextUnformatted(confirmText);

		bool close = false;
		if (ImGui::Button("Ok")) {
			manager.executeDelayed(confirmCmd);
			close = true;
		}
		ImGui::SameLine();
		close |= ImGui::Button("Cancel");
		if (close) {
			ImGui::CloseCurrentPopup();
			confirmCmd = TclObject();
		}
	});
}

void ImGuiReverseBar::paint(MSXMotherBoard* motherBoard)
{
	if (!showReverseBar) return;
	if (!motherBoard) return;
	const auto& reverseManager = motherBoard->getReverseManager();
	if (!reverseManager.isCollecting()) return;

	const auto& style = ImGui::GetStyle();
	auto textHeight = ImGui::GetTextLineHeight();
	auto windowHeight = style.WindowPadding.y + 2.0f * textHeight + style.WindowPadding.y;
	if (!reverseHideTitle) {
		windowHeight += style.FramePadding.y + textHeight + style.FramePadding.y;
	}
	ImGui::SetNextWindowSizeConstraints(ImVec2(250, windowHeight), ImVec2(FLT_MAX, windowHeight));

	// default placement: bottom right
	const auto* viewPort = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(gl::vec2(viewPort->Pos) + gl::vec2(viewPort->WorkSize) - gl::vec2(10.0f),
	                        ImGuiCond_FirstUseEver,
	                        {1.0f, 1.0f}); // pivot = bottom-right

	int flags = reverseHideTitle ? ImGuiWindowFlags_NoTitleBar |
	                               ImGuiWindowFlags_NoResize |
	                               ImGuiWindowFlags_NoScrollbar |
	                               ImGuiWindowFlags_NoScrollWithMouse |
	                               ImGuiWindowFlags_NoCollapse |
	                               ImGuiWindowFlags_NoBackground |
	                               ImGuiWindowFlags_NoFocusOnAppearing |
	                               (reverseAllowMove ? 0 : ImGuiWindowFlags_NoMove)
	                             : 0;
	adjust.pre();
	im::Window("Reverse bar", &showReverseBar, flags, [&]{
		bool isOnMainViewPort = adjust.post();
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
		gl::vec2 outerSize(availableSize.x, 2.0f * textHeight);
		gl::vec2 outerTopLeft = pos;
		gl::vec2 outerBottomRight = outerTopLeft + outerSize;

		const auto& io = ImGui::GetIO();
		bool hovered = ImGui::IsWindowHovered();
		bool replaying = reverseManager.isReplaying();
		if (!reverseHideTitle || !reverseFadeOut || replaying ||
			ImGui::IsWindowDocked() || !isOnMainViewPort) {
			reverseAlpha = 1.0f;
		} else {
			auto target = hovered ? 1.0f : 0.0f;
			auto period = hovered ? 0.5f : 5.0f; // TODO configurable speed
			reverseAlpha = calculateFade(reverseAlpha, target, period);
		}
		if (reverseAlpha != 0.0f) {
			gl::vec2 innerSize = outerSize - gl::vec2(2, 2);
			gl::vec2 innerTopLeft = outerTopLeft + gl::vec2(1, 1);
			gl::vec2 innerBottomRight = innerTopLeft + innerSize;
			gl::vec2 barBottomRight = innerTopLeft + gl::vec2(innerSize.x * fraction, innerSize.y);

			gl::vec2 middleTopLeft    (barBottomRight.x - 2.0f, innerTopLeft.y);
			gl::vec2 middleBottomRight(barBottomRight.x + 2.0f, innerBottomRight.y);

			auto color = [&](gl::vec4 col) {
				return ImGui::ColorConvertFloat4ToU32(col * reverseAlpha);
			};

			auto* drawList = ImGui::GetWindowDrawList();
			drawList->AddRectFilled(innerTopLeft, innerBottomRight, color(gl::vec4(0.0f, 0.0f, 0.0f, 0.5f)));

			for (double s : snapshots) {
				float x = narrow_cast<float>((s - b) * recipLength) * innerSize.x;
				drawList->AddLine(gl::vec2(innerTopLeft.x + x, innerTopLeft.y),
						gl::vec2(innerTopLeft.x + x, innerBottomRight.y),
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

			auto timeStr = strCat(formatTime(playLength), " / ", formatTime(totalLength));
			auto timeSize = ImGui::CalcTextSize(timeStr).x;
			gl::vec2 cursor = ImGui::GetCursorPos();
			ImGui::SetCursorPos(cursor + gl::vec2(std::max(0.0f, 0.5f * (outerSize.x - timeSize)), textHeight * 0.5f));
			ImGui::TextColored(gl::vec4(1.0f) * reverseAlpha, "%s", timeStr.c_str());
			ImGui::SetCursorPos(cursor); // restore position (for later ImGui::Dummy())
		}

		if (hovered && ImGui::IsMouseHoveringRect(outerTopLeft, outerBottomRight)) {
			float ratio = (io.MousePos.x - pos.x) / outerSize.x;
			auto timeOffset = totalLength * double(ratio);
			im::Tooltip([&] {
				ImGui::TextUnformatted(formatTime(timeOffset));
			});
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
				manager.executeDelayed(makeTclList("reverse", "goto", b + timeOffset));
			}
		}

		ImGui::Dummy(availableSize);
		im::PopupContextItem("reverse context menu", [&]{
			ImGui::Checkbox("Hide title", &reverseHideTitle);
			im::Indent([&]{
				im::Disabled(!reverseHideTitle, [&]{
					ImGui::Checkbox("Fade out", &reverseFadeOut);
					ImGui::Checkbox("Allow move", &reverseAllowMove);
				});
			});
		});
	});
}

} // namespace openmsx
