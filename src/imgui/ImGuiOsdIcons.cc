#include "ImGuiOsdIcons.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiOpenFile.hh"
#include "ImGuiUtils.hh"

#include "CommandException.hh"
#include "FileContext.hh"
#include "GLImage.hh"
#include "Interpreter.hh"
#include "StringOp.hh"
#include "enumerate.hh"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cstdlib>

namespace openmsx {

void ImGuiOsdIcons::save(ImGuiTextBuffer& buf)
{
	buf.appendf("show=%d\n", showIcons);
	buf.appendf("hideTitle=%d\n", iconsHideTitle);
	buf.appendf("allowMove=%d\n", iconsAllowMove);
	buf.appendf("layout=%d\n", iconsHorizontal);
	buf.appendf("fadeDuration=%f\n", iconsFadeDuration);
	buf.appendf("fadeDelay=%f\n", iconsFadeDelay);
	buf.appendf("showConfig=%d\n", showConfigureIcons);
	for (const auto& [i, icon] : enumerate(iconInfo)) {
		auto n = narrow<int>(i + 1);
		buf.appendf("icon.%d.enabled=%d\n",   n, icon.enable);
		buf.appendf("icon.%d.fade=%d\n",      n, icon.fade);
		buf.appendf("icon.%d.on-image=%s\n",  n, icon.on.filename.c_str());
		buf.appendf("icon.%d.off-image=%s\n", n, icon.off.filename.c_str());
		buf.appendf("icon.%d.expr=%s\n",      n, icon.expr.getString().c_str());
	}
}

void ImGuiOsdIcons::loadStart()
{
	iconInfo.clear();
}

void ImGuiOsdIcons::loadLine(std::string_view name, zstring_view value)
{
	if (name == "show") {
		showIcons = StringOp::stringToBool(value);
	} else if (name == "hideTitle") {
		iconsHideTitle = StringOp::stringToBool(value);
	} else if (name == "allowMove") {
		iconsAllowMove = StringOp::stringToBool(value);
	} else if (name == "layout") {
		if (auto r = StringOp::stringTo<unsigned>(value)) {
			if (*r <= 1) iconsHorizontal = *r;
		}
	} else if (name == "fadeDuration") {
		iconsFadeDuration = strtof(value.c_str(), nullptr); // TODO error handling
	} else if (name == "fadeDelay") {
		iconsFadeDelay = strtof(value.c_str(), nullptr); // TODO error handling
	} else if (name == "showConfig") {
		showConfigureIcons = StringOp::stringToBool(value);
	} else if (name.starts_with("icon.")) {
		auto [numStr, suffix] = StringOp::splitOnFirst(name.substr(5), '.');
		auto n = StringOp::stringTo<size_t>(numStr);
		if (!n || *n == 0) return;
		while (iconInfo.size() < *n) {
			iconInfo.emplace_back();
		}
		auto& info = iconInfo[*n - 1];
		if (suffix == "enabled") {
			info.enable = StringOp::stringToBool(value);
		} else if (suffix == "fade") {
			info.fade = StringOp::stringToBool(value);
		} else if (suffix == "on-image") {
			info.on.filename = value;
		} else if (suffix == "off-image") {
			info.off.filename = value;
		} else if (suffix == "expr") {
			info.expr = value;
		}
	}
}

void ImGuiOsdIcons::loadEnd()
{
	if (iconInfo.empty()) setDefaultIcons();
	iconInfoDirty = true;
}

void ImGuiOsdIcons::setDefaultIcons()
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

void ImGuiOsdIcons::loadIcons()
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

void ImGuiOsdIcons::paint(MSXMotherBoard* /*motherBoard*/)
{
	if (iconInfoDirty) loadIcons();
	if (showConfigureIcons) paintConfigureIcons();
	if (!showIcons) return;

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
	im::Window("Icons", &showIcons, flags | ImGuiWindowFlags_HorizontalScrollbar, [&]{
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
					return icon.expr.evalBool(manager.getInterpreter());
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
		im::PopupContextItem("icons context menu", [&]{
			if (ImGui::MenuItem("Configure ...")) {
				showConfigureIcons = true;
			}
		});
	});
}

void ImGuiOsdIcons::paintConfigureIcons()
{
	im::Window("Configure Icons", &showConfigureIcons, [&]{
		ImGui::TextUnformatted("Layout:");
		ImGui::SameLine();
		ImGui::RadioButton("Horizontal", &iconsHorizontal, 1);
		ImGui::SameLine();
		ImGui::RadioButton("Vertical", &iconsHorizontal, 0);
		ImGui::Separator();

		ImGui::Checkbox("Hide Title", &iconsHideTitle);
		im::Indent([&]{
			im::Disabled(!iconsHideTitle, [&]{
				ImGui::Checkbox("Allow move", &iconsAllowMove);
				HelpMarker("Click and drag the icon box.");
				ImGui::SliderFloat("Fade-out duration", &iconsFadeDuration, 0.0f, 30.0f);
				ImGui::SliderFloat("Fade-out delay",    &iconsFadeDelay,    0.0f, 30.0f);
			});
		});
		ImGui::Separator();

		ImGui::TextUnformatted("Advanced:");
		HelpMarker("Right-click to reorder, insert, delete.");
		int flags = ImGuiTableFlags_RowBg |
				ImGuiTableFlags_BordersOuterH |
				ImGuiTableFlags_BordersInnerH |
				ImGuiTableFlags_BordersV |
				ImGuiTableFlags_BordersOuter |
				ImGuiTableFlags_Resizable;
		im::Table("table", 5, flags, [&]{
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
				im::ID(narrow<int>(row), [&]{
					if (ImGui::TableNextColumn()) { // enabled
						auto pos = ImGui::GetCursorPos();
						const auto& style = ImGui::GetStyle();
						auto textHeight = ImGui::GetTextLineHeight();
						float rowHeight = std::max(2.0f * style.FramePadding.y + textHeight,
									std::max(float(icon.on.size[1]), float(icon.off.size[1])));
						ImGui::Selectable("##row", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap, ImVec2(0, rowHeight));
						if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
							ImGui::OpenPopup("config-icon-context");
						}
						im::Popup("config-icon-context", [&]{
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
						});

						ImGui::SetCursorPos(pos);
						if (ImGui::Checkbox("##enabled", &icon.enable)) {
							iconInfoDirty = true;
						}
					}
					if (ImGui::TableNextColumn()) { // fade-out
						im::Disabled(!iconsHideTitle, [&]{
							if (ImGui::Checkbox("##fade-out", &icon.fade)) {
								iconInfoDirty = true;
							}
						});
					}

					auto image = [&](IconInfo::Icon& ic, const char* id) {
						if (ic.tex.get()) {
							ImGui::Image(reinterpret_cast<void*>(ic.tex.get()),
									gl::vec2(ic.size));
							im::PopupContextItem(id, [&]{
								if (ImGui::MenuItem("Remove image")) {
									ic.filename.clear();
									iconInfoDirty = true;
									ImGui::CloseCurrentPopup();
								}
							});
						} else {
							ImGui::Button("Select ...");
						}
						if (ImGui::IsItemClicked()) {
							manager.openFile.selectFile(
								"Select image for icon", "PNG (*.png){.png}",
								[this, &ic](const std::string& filename) {
									ic.filename = filename;
									iconInfoDirty = true;
								});
						}
					};
					if (ImGui::TableNextColumn()) { // true-image
						image(icon.on, "##on");
					}
					if (ImGui::TableNextColumn()) { // false-image
						image(icon.off, "##off");
					}
					if (ImGui::TableNextColumn()) { // expression
						ImGui::SetNextItemWidth(-FLT_MIN);
						bool valid = manager.getInterpreter().validExpression(icon.expr.getString());
						im::StyleColor(ImGuiCol_Text, valid ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
										: ImVec4(1.0f, 0.5f, 0.5f, 1.0f), [&]{
							auto expr = std::string(icon.expr.getString());
							if (ImGui::InputText("##expr", &expr)) {
								icon.expr = expr;
								iconInfoDirty = true;
							}
						});
					}
				});
			}
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
		});

		if (ImGui::Button("Restore default")) {
			setDefaultIcons();
		}
	});
}

} // namespace openmsx
