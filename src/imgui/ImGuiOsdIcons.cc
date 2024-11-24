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

using namespace std::literals;


namespace openmsx {

ImGuiOsdIcons::ImGuiOsdIcons(ImGuiManager& manager_)
	: ImGuiPart(manager_)
{
	// Usually immediately overridden by loading imgui.ini
	// But nevertheless required for the initial state.
	setDefaultIcons();
}

void ImGuiOsdIcons::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
	adjust.save(buf);
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
	if (loadOnePersistent(name, value, *this, persistentElements)) {
		// already handled
	} else if (adjust.loadLine(name, value)) {
		// already handled
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
	maxIconSize = gl::vec2();
	numIcons = 0;
	FileContext context = systemFileContext();
	for (auto& icon : iconInfo) {
		if (!icon.enable) continue;

		auto load = [&](IconInfo::Icon& i) {
			try {
				if (!i.filename.empty()) {
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

		auto m = max(icon.on.size, icon.off.size);
		maxIconSize = max(maxIconSize, gl::vec2(m));

		++numIcons;
	}
	iconInfoDirty = false;
}

void ImGuiOsdIcons::paint(MSXMotherBoard* /*motherBoard*/)
{
	if (iconInfoDirty) loadIcons();
	if (showConfigureIcons) paintConfigureIcons();
	if (!showIcons) return;

	// default placement: bottom left
	const auto* mainViewPort = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(gl::vec2(mainViewPort->Pos) + gl::vec2{10.0f, mainViewPort->WorkSize.y - 10.0f},
	                        ImGuiCond_FirstUseEver,
	                        {0.0f, 1.0f}); // pivot = bottom-left
	int flags = hideTitle ? ImGuiWindowFlags_NoTitleBar |
	                        ImGuiWindowFlags_NoResize |
	                        ImGuiWindowFlags_NoScrollbar |
	                        ImGuiWindowFlags_NoScrollWithMouse |
	                        ImGuiWindowFlags_NoCollapse |
	                        ImGuiWindowFlags_NoBackground |
	                        ImGuiWindowFlags_NoFocusOnAppearing |
	                        ImGuiWindowFlags_NoNav |
	                        (allowMove ? 0 : ImGuiWindowFlags_NoMove)
	                      : 0;
	adjust.pre();
	im::Window("Icons", &showIcons, flags | ImGuiWindowFlags_HorizontalScrollbar, [&]{
		bool isOnMainViewPort = adjust.post();
		gl::vec2 topLeft = ImGui::GetCursorPos();

		const auto& style = ImGui::GetStyle();
		const auto& io = ImGui::GetIO();

		auto availableSize = ImGui::GetContentRegionAvail();
		auto columns = std::max(int(floor((availableSize.x + style.ItemSpacing.x) / (maxIconSize.x + style.ItemSpacing.x))), 1);
		auto rows = (numIcons + columns - 1) / columns; // round up

		// cover full canvas, both for context menu and to set dimensions (SetCursorPos() can't extend canvas)
		ImGui::Dummy(gl::vec2(float(columns) * maxIconSize.x + float(columns - 1) * style.ItemSpacing.x,
		                      float(rows   ) * maxIconSize.y + float(rows    - 1) * style.ItemSpacing.y));
		if (allowMove) {
			im::PopupContextItem("icons context menu", [&]{
				if (ImGui::MenuItem("Configure icons ...")) {
					showConfigureIcons = true;
				}
			});
		}

		bool fade = hideTitle && !ImGui::IsWindowDocked() && isOnMainViewPort;

		auto cursor = topLeft;
		int col = 0;
		for (auto& icon : iconInfo) {
			if (!icon.enable) continue;

			// is the icon on or off?
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
			icon.time += io.DeltaTime;

			// calculate fade status
			float alpha = [&] {
				if (!fade || !icon.fade) return 1.0f;
				auto t = icon.time - fadeDelay;
				if (t <= 0.0f) return 1.0f;
				if (t >= fadeDuration) return 0.0f;
				return 1.0f - (t / fadeDuration);
			}();

			// draw icon
			const auto& ic = state ? icon.on : icon.off;
			if (alpha > 0.0f && ic.tex.get()) {
				ImGui::SetCursorPos(cursor);
				ImGui::Image(ic.tex.getImGui(), gl::vec2(ic.size),
				             {0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, alpha});
			}

			// draw outline
			if (showConfigureIcons) {
				ImGui::SetCursorPos(cursor);
				gl::vec2 rectMin = ImGui::GetCursorScreenPos();
				gl::vec2 rectMax = rectMin + gl::vec2(maxIconSize);
				ImGui::GetWindowDrawList()->AddRect(rectMin, rectMax, IM_COL32(255, 0, 0, 128), 0.0f, 0, 1.0f);
			}

			// advance to next icon position
			if (++col == columns) {
				col = 0;
				cursor = gl::vec2(topLeft.x, cursor.y + style.ItemSpacing.y + maxIconSize.y);
			} else {
				cursor = gl::vec2(cursor.x + style.ItemSpacing.x + maxIconSize.x, cursor.y);
			}
		}

		if (hideTitle && ImGui::IsWindowFocused()) {
			ImGui::SetWindowFocus(nullptr); // give-up focus
		}
	});
}

void ImGuiOsdIcons::paintConfigureIcons()
{
	ImGui::SetNextWindowSize(gl::vec2{37, 17} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Configure Icons", &showConfigureIcons, [&]{
		ImGui::Checkbox("Show OSD icons", &showIcons);
		ImGui::Separator();

		if (ImGui::Checkbox("Hide Title", &hideTitle)) {
			// reset fade-out-delay (on hiding the title)
			for (auto& icon : iconInfo) {
				icon.time = 0.0f;
			}
		}
		HelpMarker("When you want the icons inside the MSX window, you might want to hide the window title.\n"
		           "To further hide the icons, it's possible to make them fade-out after some time.");
		im::Indent([&]{
			im::Disabled(!hideTitle, [&]{
				ImGui::Checkbox("Allow move", &allowMove);
				HelpMarker("When the icons are in the MSX window (without title bar), you might want "
				           "to lock them in place, to prevent them from being moved by accident.\n"
				           "Move by click and drag the icon box.\n");
				auto width = ImGui::GetFontSize() * 10.0f;
				ImGui::SetNextItemWidth(width);
				ImGui::SliderFloat("Fade-out delay",    &fadeDelay,    0.0f, 30.0f, "%.1f");
				HelpMarker("After some delay, fade-out icons that haven't changed status for a while.\n"
				           "Note: by default some icons are configured to never fade-out.");
				ImGui::SetNextItemWidth(width);
				ImGui::SliderFloat("Fade-out duration", &fadeDuration, 0.0f, 30.0f, "%.1f");
				HelpMarker("Configure the fade-out speed.");
			});
		});
		ImGui::Separator();

		im::TreeNode("Configure individual icons", [&]{
			HelpMarker("Change the order and properties of the icons (for advanced users).\n"
			           "Right-click in a row to reorder, insert, delete that row.\n"
			           "Right-click on an icon to remove it.");
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

				enum class Cmd { MOVE_FRONT, MOVE_FWD, MOVE_BWD, MOVE_BACK, INSERT, DELETE };
				using enum Cmd;
				std::pair<int, Cmd> cmd(-1, MOVE_FRONT);
				auto lastRow = narrow<int>(iconInfo.size()) - 1;
				im::ID_for_range(iconInfo.size(), [&](int row) {
					auto& icon = iconInfo[row];
					if (ImGui::TableNextColumn()) { // enabled
						auto pos = ImGui::GetCursorPos();
						const auto& style = ImGui::GetStyle();
						auto textHeight = ImGui::GetTextLineHeight();
						float rowHeight = std::max({
							2.0f * style.FramePadding.y + textHeight,
							float(icon.on.size.y),
							float(icon.off.size.y)});
						ImGui::Selectable("##row", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0, rowHeight));
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
						im::Disabled(!hideTitle, [&]{
							if (ImGui::Checkbox("##fade-out", &icon.fade)) {
								iconInfoDirty = true;
							}
						});
					}

					auto image = [&](IconInfo::Icon& ic, const char* id) {
						if (ic.tex.get()) {
							ImGui::Image(ic.tex.getImGui(), gl::vec2(ic.size));
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
							manager.openFile->selectFile(
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
						im::StyleColor(!valid, ImGuiCol_Text, getColor(imColor::ERROR), [&]{
							auto expr = std::string(icon.expr.getString());
							if (ImGui::InputText("##expr", &expr)) {
								icon.expr = expr;
								iconInfoDirty = true;
							}
						});
					}
				});
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
	});
}

} // namespace openmsx
