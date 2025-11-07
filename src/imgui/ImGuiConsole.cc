#include "ImGuiConsole.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "BooleanSetting.hh"
#include "CliComm.hh"
#include "Completer.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "FileOperations.hh"
#include "GlobalCommandController.hh"
#include "Interpreter.hh"
#include "Reactor.hh"
#include "TclParser.hh"
#include "Version.hh"

#include "narrow.hh"
#include "strCat.hh"
#include "utf8_unchecked.hh"
#include "xrange.hh"

#include <imgui.h>
#include <imgui_internal.h> /**/ // Hack: see below
#include <imgui_stdlib.h>

#include <fstream>
#include <ranges>

namespace openmsx {

using namespace std::literals;

static constexpr std::string_view PROMPT_NEW  = "> ";
static constexpr std::string_view PROMPT_CONT = "| ";
static constexpr std::string_view PROMPT_BUSY = "*busy*";

ImGuiConsole::ImGuiConsole(ImGuiManager& manager_)
	: ImGuiPart(manager_)
	, consoleSetting(
		manager.getReactor().getCommandController(), "console",
		"turns console display on/off", false, Setting::Save::NO)
	, history(1000)
	, lines(1000)
	, prompt(PROMPT_NEW)
{
	loadHistory();

	Completer::setOutput(this);
	manager.getInterpreter().setOutput(this);
	consoleSetting.attach(*this);

	const auto& fullVersion = Version::full();
	print(fullVersion);
	print(std::string(fullVersion.size(), '-'));
	print("\n"
	      "General information about openMSX is available at http://openmsx.org.\n"
	      "\n"
	      "Type 'help' to see a list of available commands.\n"
	      "Or read the Console Command Reference in the manual.\n"
	      "\n");
}

ImGuiConsole::~ImGuiConsole()
{
	consoleSetting.detach(*this);
}

void ImGuiConsole::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiConsole::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

void ImGuiConsole::print(std::string_view text, imColor color)
{
	do {
		auto pos = text.find('\n');
		newLineConsole(ConsoleLine(std::string(text.substr(0, pos)), color));
		if (pos == std::string_view::npos) break;
		text.remove_prefix(pos + 1); // skip newline
	} while (!text.empty());
}

void ImGuiConsole::newLineConsole(ConsoleLine line)
{
	auto addLine = [&](ConsoleLine&& l) {
		if (lines.full()) lines.pop_front();
		lines.push_back(std::move(l));
	};

	if (wrap) {
		do {
			auto rest = line.splitAtColumn(columns);
			addLine(std::move(line));
			line = std::move(rest);
		} while (!line.str().empty());
	} else {
		addLine(std::move(line));
	}

	scrollToBottom = true;
}

static void drawLine(const ConsoleLine& line)
{
	auto n = line.numChunks();
	for (auto i : xrange(n)) {
		im::StyleColor(ImGuiCol_Text, getColor(line.chunkColor(i)), [&]{
			ImGui::TextUnformatted(line.chunkText(i));
			if (i != (n - 1)) ImGui::SameLine(0.0f, 0.0f);
		});
	}
}

void ImGuiConsole::computeCompletionColumns(float availableWidth)
{
	//std::cerr << "DEBUG: computeCompletionColumns(" << availableWidth << ")\n";
	//for (const auto& s : completions) {
	//	std::cerr << "  " << s << '\n';
	//}
	const auto& style = ImGui::GetStyle();
	availableWidth -= style.ScrollbarSize; // reserve space for scrollbar (may not be needed)

	// measure item widths
	assert(!completions.empty());
	auto padding = 2.0f * style.CellPadding.x;
	auto widths = to_vector(std::views::transform(completions, [&](const auto& s) {
		return ImGui::CalcTextSize(s.c_str()).x + padding;
	}));
	auto [minIt, maxIt] = std::ranges::minmax_element(widths);
	auto minWidth = *minIt;
	auto maxWidth = *maxIt;

	// estimate number of columns
	int N = narrow_cast<int>(completions.size());
	int maxColumnsEstimate = std::min(
		1 + int((availableWidth - maxWidth) / minWidth),
		N);
	colWidths.resize(maxColumnsEstimate);

	// iterate to find the number of columns that fit
	int cols = maxColumnsEstimate;
	for (/**/; cols > 1; --cols) {
		int rows = (N + cols - 1) / cols;
		int c2 = (N + rows - 1) / rows; // actual number of columns
		assert(c2 > 1);
		assert(c2 <= cols);
		cols = c2;
		float totalWidth = 0.0f;
		for (int c = 0; c < cols; ++c) {
			int from = c * rows;
			int to   = std::min(from + rows, N);
			assert(from < to);
			auto width = *std::ranges::max_element(subspan(widths, from, to - from));
			colWidths[c] = width;
			totalWidth += width;
		}
		if (totalWidth <= availableWidth) break;
	}
	if (cols == 1) colWidths[0] = std::min(availableWidth, maxWidth);
	assert(cols <= maxColumnsEstimate);
	assert(cols >= 1);
	colWidths.resize(cols);
	//std::cerr << "  " << colWidths.size() << " columns:\n";
	//for (const auto& w : colWidths) {
	//	std::cerr << "    " << w << '\n';
	//}
	//std::cerr << "  totalWidth=" << sum(colWidths) << "\n";
}

void ImGuiConsole::paint(MSXMotherBoard* /*motherBoard*/)
{
	if (!replayInput.empty()) {
		auto& io = ImGui::GetIO();
		for (auto ch : replayInput) {
			io.AddInputCharacter(ch);
		}
		replayInput.clear();
	}

	bool reclaimFocus = show && !wasShown; // window appears
	wasShown = show;
	if (!show) return;

	ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
	im::Window("Console", &show, [&]{
		im::ScopedFont sf(manager.fontMono);

		if (!completions.empty() && !completionPopupOpen) {
			// just opened popup
			ImGui::OpenPopup("MessagePopup");
			completionIndex = 0;

			const auto& style = ImGui::GetStyle();
			gl::vec2 parentSize = ImGui::GetWindowSize();
			computeCompletionColumns(0.95f * parentSize.x);
			auto tableWidth = sum(colWidths);
			auto N = narrow<int>(completions.size());
			auto cols = narrow<int>(colWidths.size());
			auto rows = (N + cols - 1) / cols;
			auto tableHeight = float(rows) * (ImGui::GetTextLineHeight() + 2 * style.CellPadding.y);
			auto tableSize = gl::vec2{tableWidth, tableHeight};
			auto requiredWindowSize = tableSize + 2.0f * (gl::vec2(style.WindowPadding) + gl::vec2(style.WindowBorderSize));
			popupSize = min(requiredWindowSize, parentSize * 0.95f);



			gl::vec2 parentPos = ImGui::GetWindowPos();

			// Position popup centered over parent
			ImGui::SetNextWindowPos(parentPos + 0.5f * parentSize, ImGuiCond_Always, gl::vec2{0.5f});
			//ImGui::SetNextWindowSizeConstraints(gl::vec2{}, parentSize * 0.9f);
		}
		ImGui::SetNextWindowSize(popupSize);
		bool openNow = im::Popup("MessagePopup", ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav, [&]{
			completionPopupOpen = true;
			auto N = narrow<int>(completions.size());
			auto C = narrow<int>(colWidths.size());
			auto R = (N + C - 1) / C;
			im::Child("list", {}, /*ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY,*/ [&]{
				// TODO clipper
				/*im::ListClipper(completions.size(), [&](int i) {
					bool selected = i == completionIndex;
					if (ImGui::Selectable(completions[i].c_str(), selected)) {
						// TODO
					}
					if (selected) ImGui::SetScrollHereY(0.5f);

				});*/
				im::Table("completionTable", C, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX, [&]{
					for (int r = 0; r < R; ++r) {
						for (int c = 0; c < C; ++c) {
							if (ImGui::TableNextColumn()) {
								int i = c * R + r;
								if (i >= N) break;
								bool selected = i == completionIndex;
								if (ImGui::Selectable(completions[i].c_str(), selected)) {
									// TODO
								}
								if (selected) ImGui::SetScrollHereY(0.5f);
							}
						}
					}
				});
			});
			auto& io = ImGui::GetIO();
			if (io.InputQueueCharacters.Size != 0) {
				replayInput.assign(io.InputQueueCharacters.begin(), io.InputQueueCharacters.end());
				completions.clear();
				ImGui::CloseCurrentPopup();
			} else if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
				if (io.KeyShift) {
					completionIndex = (completionIndex == 0) ? (N - 1) : (completionIndex - 1);
				} else {
					completionIndex = (completionIndex == (N - 1)) ? 0 : (completionIndex + 1);
				}
			} else if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter) ||
			           ImGui::IsKeyPressed(ImGuiKey_Space)) {
				// select completion
				completionReplacement = completions[completionIndex];
				completions.clear();
				completionPopupOpen = false;
				io.ClearInputKeys();
				ImGui::CloseCurrentPopup();
			} else {
				bool up    = ImGui::IsKeyPressed(ImGuiKey_UpArrow);
				bool down  = ImGui::IsKeyPressed(ImGuiKey_DownArrow);
				bool left  = ImGui::IsKeyPressed(ImGuiKey_LeftArrow);
				bool right = ImGui::IsKeyPressed(ImGuiKey_RightArrow);
				if (up || down || left || right) {
					int c = completionIndex / R;
					int r = completionIndex % R;
					int lastR = N - (C - 1) * R;
					int nr = (c == (C - 1)) ? lastR : R;
					int nc = (r >= lastR) ? (C - 1) : C;

					if (up) {
						r = (r > 0) ? (r - 1) : (nr - 1);
					} else if (down) {
						r = (r < (nr - 1)) ? (r + 1) : 0;
					} else if (left) {
						c = (c > 0) ? (c - 1) : (nc - 1);
					} else if (right) {
						c = (c < (nc - 1)) ? (c + 1) : 0;
					}

					completionIndex = c * R + r;
					assert(completionIndex < N);
				}
			}
		});
		if (!openNow && completionPopupOpen) {
			// just closed popup
			completionPopupOpen = false;
			completions.clear();
			// Clear ESC key state so InputText doesn't see it as pressed
			ImGui::GetIO().ClearInputKeys();
		}

		// Reserve enough left-over height for 1 separator + 1 input text
		const auto& style = ImGui::GetStyle();
		const float footerHeightToReserve = style.ItemSpacing.y +
						ImGui::GetFrameHeightWithSpacing();

		bool scrollUp   = ImGui::Shortcut(ImGuiKey_PageUp);
		bool scrollDown = ImGui::Shortcut(ImGuiKey_PageDown);
		im::Child("ScrollingRegion", ImVec2(0, -footerHeightToReserve), 0,
		          ImGuiWindowFlags_HorizontalScrollbar, [&]{
			im::PopupContextWindow([&]{
				if (ImGui::Selectable("Clear")) {
					lines.clear();
				}
				ImGui::Checkbox("Wrap (new) output", &wrap);
			});

			im::StyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1), [&]{ // Tighten spacing
				im::ListClipper(lines.size(), [&](int i) {
					drawLine(lines[i]);
				});
			});
			ImGui::Spacing();

			// Keep up at the bottom of the scroll region if we were already
			// at the bottom at the beginning of the frame.
			if (scrollToBottom || (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
				scrollToBottom = false;
				ImGui::SetScrollHereY(1.0f);
			}

			auto scrollDelta = ImGui::GetWindowHeight() * 0.5f;
			if (scrollUp) {
				ImGui::SetScrollY(std::max(ImGui::GetScrollY() - scrollDelta, 0.0f));
			}
			if (scrollDown) {
				ImGui::SetScrollY(std::min(ImGui::GetScrollY() + scrollDelta, ImGui::GetScrollMaxY()));
			}

			// recalculate the number of columns
			auto width = ImGui::GetContentRegionAvail().x;
			auto charWidth = ImGui::CalcTextSize("M"sv).x;
			columns = narrow_cast<unsigned>(width / charWidth);
		});
		ImGui::Separator();

		// Command-line
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(prompt);
		ImGui::SameLine(0.0f, 0.0f);

		ImGui::SetNextItemWidth(-FLT_MIN); // full window width
		/**/ // Hack: see below
		/**/ auto cursorScrnPos = ImGui::GetCursorScreenPos();
		/**/ auto itemWidth = ImGui::CalcItemWidth();

		ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue |
					ImGuiInputTextFlags_EscapeClearsAll |
					ImGuiInputTextFlags_CallbackEdit |
					ImGuiInputTextFlags_CallbackCompletion |
					ImGuiInputTextFlags_CallbackHistory |
					ImGuiInputTextFlags_CallbackAlways;
		bool enter = false;
		im::StyleColor(ImGuiCol_Text, 0x00000000, [&]{ // transparent, see HACK below
			enter = ImGui::InputTextWithHint("##Input", "enter command", &inputBuf, flags, &textEditCallbackStub, this);
			if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
				commandBuffer.clear();
				prompt = PROMPT_NEW;
			}
		});
		if (enter && (prompt != PROMPT_BUSY)) {
			// print command in output buffer, with prompt prepended
			ConsoleLine cmdLine(prompt);
			cmdLine.addLine(coloredInputBuf);
			newLineConsole(std::move(cmdLine));

			// append (partial) command to a possibly multi-line command
			strAppend(commandBuffer, inputBuf, '\n');

			putHistory(std::move(inputBuf));
			saveHistory(); // save at this point already, so that we don't lose history in case of a crash
			inputBuf.clear();
			coloredInputBuf.clear();
			historyPos = -1;
			historyBackupLine.clear();

			auto& commandController = manager.getReactor().getGlobalCommandController();
			if (commandController.isComplete(commandBuffer)) {
				// Normally the busy prompt is NOT shown (not even briefly
				// because the screen is not redrawn), though for some commands
				// that potentially take a long time to execute, we explicitly
				// do redraw.
				prompt = PROMPT_BUSY;

				manager.executeDelayed(TclObject(commandBuffer),
					[this](const TclObject& result) {
						if (const auto& s = result.getString(); !s.empty()) {
							this->print(s);
						}
						prompt = PROMPT_NEW;
					},
					[this](const std::string& error) {
						this->print(error, imColor::ERROR);
						prompt = PROMPT_NEW;
					});
				commandBuffer.clear();
			} else {
				prompt = PROMPT_CONT;
			}
			reclaimFocus = true;
		}
		ImGui::SetItemDefaultFocus();

		if (reclaimFocus ||
		(ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
		!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
		!ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0) && !ImGui::IsMouseClicked(1))) {
			ImGui::SetKeyboardFocusHere(-1); // focus the InputText widget
		}

		/**/ // Hack: currently ImGui::InputText() does not support colored text.
		/**/ // Though there are plans to extend this. See:
		/**/ //     https://github.com/ocornut/imgui/pull/3130
		/**/ //     https://github.com/ocornut/imgui/issues/902
		/**/ // To work around this limitation, we use ImGui::InputText() as-is,
		/**/ // but then overdraw the text using the correct colors. This works,
		/**/ // but it's fragile because it depends on some internal implementation
		/**/ // details. More specifically: the scroll-position. And obtaining this
		/**/ // information required stuff from <imgui_internal.h>.
		/**/ auto* font = ImGui::GetFont();
		/**/ auto fontSize = ImGui::GetFontSize();
		/**/ gl::vec2 frameSize(itemWidth, fontSize + style.FramePadding.y * 2.0f);
		/**/ gl::vec2 topLeft = cursorScrnPos;
		/**/ gl::vec2 bottomRight = topLeft + frameSize;
		/**/ gl::vec2 drawPos = topLeft + gl::vec2(style.FramePadding);
		/**/ ImVec4 clipRect = gl::vec4(topLeft, bottomRight);
		/**/ auto* drawList = ImGui::GetWindowDrawList();
		/**/ if (ImGui::IsItemActive()) {
		/**/	auto id = ImGui::GetID("##Input");
		/**/	if (const auto* state = ImGui::GetInputTextState(id)) { // Internal API !!!
		/**/		// adjust for scroll
		/**/		drawPos.x -= state->Scroll.x;
		/**/		// redraw cursor (it was drawn transparent before)
		/**/		bool cursorIsVisible = (state->CursorAnim <= 0.0f) || ImFmod(state->CursorAnim, 1.20f) <= 0.80f;
		/**/		if (cursorIsVisible) {
		/**/			// This assumes a single line
		/**/			auto strToCursor = zstring_view(coloredInputBuf.str()).substr(0, state->GetCursorPos());
		/**/			gl::vec2 cursorOffset(ImGui::CalcTextSize(strToCursor).x, 0.0f);
		/**/			gl::vec2 cursorScreenPos = ImTrunc(drawPos + cursorOffset);
		/**/			ImRect cursorScreenRect(cursorScreenPos.x, cursorScreenPos.y - 0.5f, cursorScreenPos.x + 1.0f, cursorScreenPos.y + fontSize - 1.5f);
		/**/			if (cursorScreenRect.Overlaps(clipRect)) {
		/**/				drawList->AddLine(cursorScreenRect.Min, cursorScreenRect.GetBL(), getColor(imColor::TEXT));
		/**/			}
		/**/		}
		/**/	}
		/**/ }
		/**/ for (auto i : xrange(coloredInputBuf.numChunks())) {
		/**/ 	auto text = coloredInputBuf.chunkText(i);
		/**/ 	auto rgba = getColor(coloredInputBuf.chunkColor(i));
		/**/ 	const char* begin = text.data();
		/**/ 	const char* end = begin + text.size();
		/**/ 	drawList->AddText(font, fontSize, drawPos, rgba, begin, end, 0.0f, &clipRect);
		/**/    drawPos.x += ImGui::CalcTextSize(text).x;
		/**/ }
	});
}

int ImGuiConsole::textEditCallbackStub(ImGuiInputTextCallbackData* data)
{
	auto* console = static_cast<ImGuiConsole*>(data->UserData);
	return console->textEditCallback(data);
}

void ImGuiConsole::tabEdit(ImGuiInputTextCallbackData* data, function_ref<std::string(std::string_view)> action)
{
	std::string_view oldLine{data->Buf, narrow<size_t>(data->BufTextLen)};
	auto front = oldLine.substr(0, data->CursorPos);
	auto back  = oldLine.substr(data->CursorPos);

	std::string newFront = action(front);
	auto newPos = narrow<int>(newFront.size());
	historyBackupLine = strCat(std::move(newFront), back);
	historyPos = -1;

	data->DeleteChars(0, data->BufTextLen);
	data->InsertChars(0, historyBackupLine.c_str());
	data->CursorPos = newPos;

	colorize(historyBackupLine);
}

int ImGuiConsole::textEditCallback(ImGuiInputTextCallbackData* data)
{
	if (data->EventFlag & ImGuiInputTextFlags_CallbackCompletion) {
		auto& commandController = manager.getReactor().getGlobalCommandController();
		tabEdit(data, [&](std::string_view front) {
			return commandController.tabCompletion(front);
		});
	}
	if (data->EventFlag & ImGuiInputTextFlags_CallbackHistory) {
		bool match = false;
		if (data->EventKey == ImGuiKey_UpArrow) {
			while (!match && (historyPos < narrow<int>(history.size() - 1))) {
				++historyPos;
				match = history[historyPos].starts_with(historyBackupLine);
			}
		} else if ((data->EventKey == ImGuiKey_DownArrow) && (historyPos != -1)) {
			while (!match) {
				if (--historyPos == -1) break;
				match = history[historyPos].starts_with(historyBackupLine);
			}
		}
		if (match || (historyPos == -1)) {
			const auto& historyStr = (historyPos >= 0) ? history[historyPos] : historyBackupLine;
			data->DeleteChars(0, data->BufTextLen);
			data->InsertChars(0, historyStr.c_str());
			colorize(std::string_view{data->Buf, narrow<size_t>(data->BufTextLen)});
		}
	}
	if (data->EventFlag & ImGuiInputTextFlags_CallbackEdit) {
		historyBackupLine.assign(data->Buf, narrow<size_t>(data->BufTextLen));
		historyPos = -1;
		colorize(historyBackupLine);
	}
	if (data->EventFlag & ImGuiInputTextFlags_CallbackAlways) {
		if (!completionReplacement.empty()) {
			auto& commandController = manager.getReactor().getGlobalCommandController();
			tabEdit(data, [&](std::string_view front) {
				return commandController.tabCompletionReplace(front, completionReplacement);
			});
			completionReplacement.clear();
		}
	}
	return 0;
}

void ImGuiConsole::colorize(std::string_view line)
{
	TclParser parser = manager.getInterpreter().parse(line);
	const auto& colors = parser.getColors();
	assert(colors.size() == line.size());

	coloredInputBuf.clear();
	size_t pos = 0;
	while (pos != colors.size()) {
		char col = colors[pos];
		size_t pos2 = pos++;
		while ((pos != colors.size()) && (colors[pos] == col)) {
			++pos;
		}
		imColor color = [&] {
			switch (col) {
			using enum imColor;
			case 'E': return ERROR;
			case 'c': return COMMENT;
			case 'v': return VARIABLE;
			case 'l': return LITERAL;
			case 'p': return PROC;
			case 'o': return OPERATOR;
			default:  return TEXT; // other
			}
		}();
		coloredInputBuf.addChunk(line.substr(pos2, pos - pos2), color);
	}
}

void ImGuiConsole::putHistory(std::string command)
{
	if (command.empty()) return;
	if (!history.empty() && (history.front() == command)) {
		return;
	}
	if (history.full()) history.pop_back();
	history.push_front(std::move(command));
}

void ImGuiConsole::saveHistory()
{
	try {
		std::ofstream outputFile;
		FileOperations::openOfStream(outputFile,
		        userFileContext("console").resolveCreate("history.txt"));
		if (!outputFile) {
			throw FileException("Error while saving the console history.");
		}
		for (const auto& s : std::views::reverse(history)) {
			outputFile << s << '\n';
		}
	} catch (FileException& e) {
		manager.getCliComm().printWarning(e.getMessage());
	}
}

void ImGuiConsole::loadHistory()
{
	try {
		std::ifstream inputFile(
		        userFileContext("console").resolveCreate("history.txt"));
		std::string line;
		while (inputFile) {
			getline(inputFile, line);
			putHistory(line);
		}
	} catch (FileException&) {
		// Error while loading the console history, ignore
	}
}

void ImGuiConsole::output(std::string_view text)
{
	print(text);
}

unsigned ImGuiConsole::getOutputColumns() const
{
	return columns;
}

void ImGuiConsole::setCompletions(std::span<const std::string_view> completions_)
{
	assert(!completions_.empty());
	completions = to_vector<std::string>(completions_);
}

void ImGuiConsole::update(const Setting& /*setting*/) noexcept
{
	show = consoleSetting.getBoolean();
	if (!show) {
		// Close the console via the 'console' setting.  Typically this
		// means via the F10 hotkey (or possibly by typing 'set console
		// off' in the console).
		//
		// Give focus to the main openMSX window.
		//
		// This makes the following scenario work:
		// * You were controlling the MSX, e.g. playing a game.
		// * You press F10 to open the console.
		// * You type a command (e.g. swap a game disk, for some people
		//   the console is still more convenient and/or faster than the
		//   new media menu).
		// * You press F10 again to close the console
		// * At this point the focus should go back to the main openMSX
		//   window (so that MSX input works).
		SDL_RaiseWindow(SDL_GetWindowFromID(WindowEvent::getMainWindowId()));
		ImGui::SetWindowFocus(nullptr);
	}
}

} // namespace openmsx
