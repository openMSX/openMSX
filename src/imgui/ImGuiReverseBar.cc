#include "ImGuiReverseBar.hh"

#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "FileOperations.hh"
#include "GLImage.hh"
#include "MSXMotherBoard.hh"
#include "ReverseManager.hh"

#include <imgui.h>

namespace openmsx {

ImGuiReverseBar::ImGuiReverseBar(ImGuiManager& manager_)
	: manager(manager_)
{
}

void ImGuiReverseBar::save(ImGuiTextBuffer& buf)
{
	buf.append("[openmsx][reverse bar]\n");
	buf.appendf("show=%d\n", showReverseBar);
	buf.appendf("hideTitle=%d\n", reverseHideTitle);
	buf.appendf("fadeOut=%d\n", reverseFadeOut);
	buf.appendf("allowMove=%d\n", reverseAllowMove);
	buf.append("\n");
}

void ImGuiReverseBar::loadLine(std::string_view name, zstring_view value)
{
	if (name == "show") {
		showReverseBar = StringOp::stringToBool(value);
	} else if (name == "hideTitle") {
		reverseHideTitle = StringOp::stringToBool(value);
	} else if (name == "fadeOut") {
		reverseFadeOut = StringOp::stringToBool(value);
	} else if (name == "allowMove") {
		reverseAllowMove = StringOp::stringToBool(value);
	}
}

void ImGuiReverseBar::showMenu(MSXMotherBoard* motherBoard)
{
	if (!ImGui::BeginMenu("Save state", motherBoard != nullptr)) {
		return;
	}
	assert(motherBoard);

	if (ImGui::MenuItem("Quick load state", "ALT+F7")) { // TODO check binding dynamically
		manager.executeDelayed(makeTclList("loadstate"));
	}
	if (ImGui::MenuItem("Quick save state", "ALT+F8")) { // TODO
		manager.executeDelayed(makeTclList("savestate"));
	}
	ImGui::Separator();

	auto existingStates = manager.execute(TclObject("list_savestates"));
	if (ImGui::BeginMenu("Load state ...", existingStates && !existingStates->empty())) {
		if (ImGui::BeginTable("table", 2, ImGuiTableFlags_BordersInnerV)) {
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("select savestate");
			if (ImGui::BeginListBox("##list", ImVec2(ImGui::GetFontSize() * 20.0f, 240.0f))) {
				for (const auto& name : *existingStates) {
					if (ImGui::Selectable(name.c_str())) {
						manager.executeDelayed(makeTclList("loadstate", name));
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
}

void ImGuiReverseBar::paint(MSXMotherBoard& motherBoard)
{
	if (!showReverseBar) return;

	const auto& style = ImGui::GetStyle();
	auto textHeight = ImGui::GetTextLineHeight();
	auto windowHeight = style.WindowPadding.y + 2.0f * textHeight + style.WindowPadding.y;
	if (!reverseHideTitle) {
		windowHeight += style.FramePadding.y + textHeight + style.FramePadding.y;
	}
	ImGui::SetNextWindowSizeConstraints(ImVec2(250, windowHeight), ImVec2(FLT_MAX, windowHeight));

	int flags = reverseHideTitle ? ImGuiWindowFlags_NoTitleBar |
	                               ImGuiWindowFlags_NoResize |
	                               ImGuiWindowFlags_NoScrollbar |
	                               ImGuiWindowFlags_NoScrollWithMouse |
	                               ImGuiWindowFlags_NoCollapse |
	                               ImGuiWindowFlags_NoBackground |
	                               (reverseAllowMove ? 0 : ImGuiWindowFlags_NoMove)
	                             : 0;
	ImGui::Begin("Reverse bar", &showReverseBar, flags);

	auto& reverseManager = motherBoard.getReverseManager();
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
				manager.executeDelayed(makeTclList("reverse", "goto", b + timeOffset));
			}
		}

		ImGui::SetCursorPos(cursor); // cover full window for context menu
		ImGui::Dummy(availableSize);
		if (ImGui::BeginPopupContextItem("reverse context menu")) {
			ImGui::Checkbox("Hide title", &reverseHideTitle);
			ImGui::Indent();
			ImGui::BeginDisabled(!reverseHideTitle);
			ImGui::Checkbox("Fade out", &reverseFadeOut);
			ImGui::Checkbox("Allow move", &reverseAllowMove);
			ImGui::EndDisabled();
			ImGui::Unindent();
			ImGui::EndPopup();
		}
	} else {
		ImGui::TextUnformatted("Reverse is disabled.");
		if (ImGui::Button("Enable")) {
			manager.executeDelayed(makeTclList("reverse", "start"));
		}
	}

	ImGui::End();
}

} // namespace openmsx
