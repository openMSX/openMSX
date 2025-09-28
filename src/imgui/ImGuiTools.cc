#include "ImGuiTools.hh"

#include "ImGuiCheatFinder.hh"
#include "ImGuiConsole.hh"
#include "ImGuiCpp.hh"
#include "ImGuiDiskManipulator.hh"
#include "ImGuiKeyboard.hh"
#include "ImGuiManager.hh"
#include "ImGuiMessages.hh"
#include "ImGuiMsxMusicViewer.hh"
#include "ImGuiSCCViewer.hh"
#include "ImGuiTrainer.hh"
#include "ImGuiUtils.hh"
#include "ImGuiWaveViewer.hh"

#include "AviRecorder.hh"
#include "Display.hh"

#include "FileOperations.hh"

#include "StringOp.hh"
#include "enumerate.hh"
#include "escape_newline.hh"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <string>
#include <vector>

namespace openmsx {

using namespace std::literals;

ImGuiTools::ImGuiTools(ImGuiManager& manager_)
	: ImGuiPart(manager_)
	, confirmDialog(manager_, "Confirm##Tools")
{
}

void ImGuiTools::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);

	for (const auto& note : notes) {
		buf.appendf("note.show=%d\n", note.show);
		buf.appendf("note.text=%s\n", escape_newline::encode(note.text).c_str());
	}
}

void ImGuiTools::loadStart()
{
	notes.clear();
}

void ImGuiTools::loadLine(std::string_view name, zstring_view value)
{
	if (loadOnePersistent(name, value, *this, persistentElements)) {
		// already handled
	} else if (name.starts_with("note.")) {
		if (name.ends_with(".show")) {
			auto& note = notes.emplace_back();
			note.show = StringOp::stringToBool(value);
		} else if (name.ends_with(".text") && !notes.empty()) {
			notes.back().text = escape_newline::decode(value);
		}
	}
}

static const std::vector<std::string>& getAllToyScripts(ImGuiManager& manager)
{
	static float refresh = FLT_MAX;
	static std::vector<std::string> result;

	if (refresh > 2.5f) { // only recalculate every so often
		refresh = 0.0f;

		if (auto commands = manager.execute(TclObject("openmsx::all_command_names"))) {
			result.clear();
			for (const auto& cmd : *commands) {
				if (cmd.starts_with("toggle_")) {
					// filter out exceptions (not all toggle commands are useful toys for the GUI)
					if (cmd == "toggle_breaked") continue;
					result.emplace_back(cmd.view());
				}
			}
			std::ranges::sort(result, StringOp::caseless{});
		}
	}
	refresh += ImGui::GetIO().DeltaTime;
	return result;
}

void ImGuiTools::showMenu(MSXMotherBoard* motherBoard)
{
	im::Menu("Tools", [&]{
		const auto& hotKey = manager.getReactor().getHotKey();

		ImGui::MenuItem("Show virtual keyboard", nullptr, &manager.keyboard->show);
		auto consoleShortCut = getShortCutForCommand(hotKey, "toggle console");
		ImGui::MenuItem("Show console", consoleShortCut.c_str(), &manager.console->show);
		ImGui::MenuItem("Show message log", nullptr, &manager.messages->logWindow.open);
		ImGui::Separator();

		std::string_view copyCommand = "copy_screen_to_clipboard";
		auto copyShortCut = getShortCutForCommand(hotKey, copyCommand);
		if (ImGui::MenuItem("Copy screen text to clipboard", copyShortCut.c_str(), nullptr, motherBoard != nullptr)) {
			manager.executeDelayed(TclObject(copyCommand));
		}
		std::string_view pasteCommand = "type_clipboard";
		auto pasteShortCut = getShortCutForCommand(hotKey, pasteCommand);
		if (ImGui::MenuItem("Paste clipboard into MSX", pasteShortCut.c_str(), nullptr, motherBoard != nullptr)) {
			manager.executeDelayed(TclObject(pasteCommand));
		}
		if (ImGui::MenuItem("Simple notes widget...")) {
			if (auto it = std::ranges::find(notes, false, &Note::show);
			    it != notes.end()) {
				// reopen a closed note
				it->show = true;
			} else {
				// create a new note
				auto& note = notes.emplace_back();
				note.show = true;
			}
		}
		simpleToolTip("Typical use: dock into a larger layout to add free text.");
		ImGui::Separator();

		im::Menu("Capture", [&]{
			ImGui::MenuItem("Screenshot...", nullptr, &showScreenshot);
			ImGui::MenuItem("Audio/Video...", nullptr, &showRecord);
		});
		ImGui::Separator();

		ImGui::MenuItem("Disk Manipulator", nullptr, &manager.diskManipulator->show);
		ImGui::Separator();

		ImGui::MenuItem("Trainer Selector", nullptr, &manager.trainer->show);
		ImGui::MenuItem("Cheat Finder", nullptr, &manager.cheatFinder->show);
		ImGui::Separator();

		ImGui::MenuItem("SCC viewer", nullptr, &manager.sccViewer->show);
		ImGui::MenuItem("MSX-Music viewer", nullptr, &manager.msxMusicViewer->show);
		ImGui::MenuItem("Audio channel viewer", nullptr, &manager.waveViewer->show);
		ImGui::Separator();

		im::Menu("Toys", [&]{
			const auto& toys = getAllToyScripts(manager);
			for (const auto& toy : toys) {
				std::string displayText = toy.substr(7);
				std::ranges::replace(displayText, '_', ' ');
				if (ImGui::MenuItem(displayText.c_str())) {
					manager.executeDelayed(TclObject(toy));
				}
				auto help = manager.execute(TclObject("help " + toy));
				if (help) {
					simpleToolTip(help->getString());
				}
			}
		});
	});
}

void ImGuiTools::paint(MSXMotherBoard* /*motherBoard*/)
{
	if (showScreenshot) paintScreenshot();
	if (showRecord) paintRecord();
	paintNotes();

	confirmDialog.execute();
}

bool ImGuiTools::screenshotNameExists() const
{
	auto filename = FileOperations::parseCommandFileArgument(
		screenshotName, Display::SCREENSHOT_DIR, "", Display::SCREENSHOT_EXTENSION);
	return FileOperations::exists(filename);
}

void ImGuiTools::generateScreenshotName()
{
	if (auto result = manager.execute(makeTclList("guess_title", "openmsx"))) {
		screenshotName = result->getString();
	}
	if (screenshotName.empty()) {
		screenshotName = "openmsx";
	}
	if (screenshotNameExists()) {
		nextScreenshotName();
	}
}

void ImGuiTools::nextScreenshotName()
{
	std::string_view prefix = screenshotName;
	if (prefix.ends_with(Display::SCREENSHOT_EXTENSION)) prefix.remove_suffix(Display::SCREENSHOT_EXTENSION.size());
	if (prefix.size() > 4) {
		auto counter = prefix.substr(prefix.size() - 4);
		if (std::ranges::all_of(counter, [](char c) { return ('0' <= c) && (c <= '9'); })) {
			prefix.remove_suffix(4);
			if (prefix.ends_with(' ') || prefix.ends_with('_')) {
				prefix.remove_suffix(1);
			}
		}
	}
	screenshotName = FileOperations::stem(FileOperations::getNextNumberedFileName(
		Display::SCREENSHOT_DIR, prefix, Display::SCREENSHOT_EXTENSION, true));
}

void ImGuiTools::paintScreenshot()
{
	ImGui::SetNextWindowSize(gl::vec2{24, 19} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Capture screenshot", &showScreenshot, [&]{
		if (ImGui::IsWindowAppearing()) {
			// on each re-open of this window, create a suggestion for a name
			generateScreenshotName();
		}
		im::TreeNode("Settings", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			ImGui::RadioButton("Rendered image", &screenshotType, static_cast<int>(SsType::RENDERED));
			HelpMarker("Include all rendering effect like scale-algorithm, horizontal-stretch, color effects, ...");
			im::DisabledIndent(screenshotType != static_cast<int>(SsType::RENDERED), [&]{
				ImGui::Checkbox("With OSD elements", &screenshotWithOsd);
				HelpMarker("Include non-MSX elements, e.g. the GUI");
			});
			ImGui::RadioButton("Raw MSX image", &screenshotType, static_cast<int>(SsType::MSX));
			HelpMarker("The raw unscaled MSX image, without any rendering effects");
			im::DisabledIndent(screenshotType != static_cast<int>(SsType::MSX), [&]{
				ImGui::RadioButton("auto", &screenshotSize, static_cast<int>(SsSize::AUTO));
				HelpMarker("One of the other sizes is selected based on the current MSX screen mode, e.g. for a 512 x 212 mode, the 640 x 480 option will be used");
				ImGui::RadioButton("320 x 240", &screenshotSize, static_cast<int>(SsSize::S_320));
				ImGui::RadioButton("640 x 480", &screenshotSize, static_cast<int>(SsSize::S_640));
			});
			ImGui::Checkbox("Hide sprites", &screenshotHideSprites);
			HelpMarker("Note: screen must be re-rendered for this, "
				"so emulation must be (briefly) unpaused before the screenshot can be taken");
		});
		ImGui::Separator();

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Name"sv);
		ImGui::SameLine();
		const auto& style = ImGui::GetStyle();
		auto buttonWidth = style.ItemSpacing.x + 2.0f * style.FramePadding.x +
		                   ImGui::CalcTextSize("Create").x;
		ImGui::SetNextItemWidth(-buttonWidth);
		ImGui::InputText("##name", &screenshotName);
		ImGui::SameLine();
		if (ImGui::Button("Create")) {
			auto cmd = makeTclList("screenshot", screenshotName);
			if (screenshotType == static_cast<int>(SsType::RENDERED)) {
				if (screenshotWithOsd) {
					cmd.addListElement("-with-osd");
				}
			} else {
				cmd.addListElement("-raw");
				cmd.addListElement("-size");
				cmd.addListElement(screenshotSize == static_cast<int>(SsSize::S_640) ? "640"sv
						: screenshotSize == static_cast<int>(SsSize::S_320) ? "320"sv
						: "auto"sv);
			}
			if (screenshotHideSprites) {
				cmd.addListElement("-no-sprites");
			}

			if (screenshotNameExists()) {
				confirmDialog.open(
					strCat("Overwrite screenshot with name '", screenshotName, "'?"),
					cmd);
				// note: don't auto generate next name
			} else {
				manager.executeDelayed(cmd,
				                       [&](const TclObject&) { nextScreenshotName(); });
			}
		}
		ImGui::Separator();
		if (ImGui::Button("Open screenshots folder")) {
			SDL_OpenURL(strCat("file://", FileOperations::getUserOpenMSXDir(Display::SCREENSHOT_DIR)).c_str());
		}

	});
}

std::string ImGuiTools::getRecordFilename() const
{
	bool recordVideo = recordSource != static_cast<int>(Source::AUDIO);
	std::string_view directory = recordVideo ? AviRecorder::VIDEO_DIR : AviRecorder::AUDIO_DIR;
	std::string_view extension = recordVideo ? AviRecorder::VIDEO_EXTENSION : AviRecorder::AUDIO_EXTENSION;
	return FileOperations::parseCommandFileArgument(
		recordName, directory, "openmsx", extension);
}

void ImGuiTools::paintRecord()
{
	bool recording = manager.getReactor().getRecorder().isRecording();

	auto title = recording ? strCat("Recording to ", FileOperations::getFilename(getRecordFilename()))
	                       : std::string("Capture Audio/Video");
	im::Window(tmpStrCat(title, "###A/V").c_str(), &showRecord, [&]{
		im::Disabled(recording, [&]{
			im::TreeNode("Settings", ImGuiTreeNodeFlags_DefaultOpen, [&]{
				ImGui::TextUnformatted("Source:"sv);
				ImGui::SameLine();
				ImGui::RadioButton("Audio", &recordSource, static_cast<int>(Source::AUDIO));
				ImGui::SameLine(0.0f, 30.0f);
				ImGui::RadioButton("Video", &recordSource, static_cast<int>(Source::VIDEO));
				ImGui::SameLine(0.0f, 30.0f);
				ImGui::RadioButton("Both", &recordSource, static_cast<int>(Source::BOTH));

				im::Disabled(recordSource == static_cast<int>(Source::VIDEO), [&]{
					ImGui::TextUnformatted("Audio:"sv);
					ImGui::SameLine();
					ImGui::RadioButton("Mono", &recordAudio, static_cast<int>(Audio::MONO));
					ImGui::SameLine(0.0f, 30.0f);
					ImGui::RadioButton("Stereo", &recordAudio, static_cast<int>(Audio::STEREO));
					ImGui::SameLine(0.0f, 30.0f);
					ImGui::RadioButton("Auto-detect", &recordAudio, static_cast<int>(Audio::AUTO));
					HelpMarker("At the start of the recording check if any stereo or "
						"any off-center-balanced mono sound devices are present.");
				});

				im::Disabled(recordSource == static_cast<int>(Source::AUDIO), [&]{
					ImGui::TextUnformatted("Video:"sv);
					ImGui::SameLine();
					ImGui::RadioButton("320 x 240", &recordVideoSize, static_cast<int>(VideoSize::V_320));
					ImGui::SameLine(0.0f, 30.0f);
					ImGui::RadioButton("640 x 480", &recordVideoSize, static_cast<int>(VideoSize::V_640));
					ImGui::SameLine(0.0f, 30.0f);
					ImGui::RadioButton("960 x 720", &recordVideoSize, static_cast<int>(VideoSize::V_960));
				});
			});
			ImGui::Separator();

			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Name"sv);
			ImGui::SameLine();
			const auto& style = ImGui::GetStyle();
			auto buttonWidth = style.ItemSpacing.x + 2.0f * style.FramePadding.x +
			                   ImGui::CalcTextSize("Start").x;
			ImGui::SetNextItemWidth(-buttonWidth);
			ImGui::InputText("##name", &recordName);
			ImGui::SameLine();
			if (!recording && ImGui::Button("Start")) {
				auto cmd = makeTclList("record", "start");
				if (!recordName.empty()) {
					cmd.addListElement(recordName);
				}
				if (recordSource == static_cast<int>(Source::AUDIO)) {
					cmd.addListElement("-audioonly");
				} else if (recordSource == static_cast<int>(Source::VIDEO)) {
					cmd.addListElement("-videoonly");
				}
				if (recordSource != static_cast<int>(Source::VIDEO)) {
					if (recordAudio == static_cast<int>(Audio::MONO)) {
						cmd.addListElement("-mono");
					} else if (recordAudio == static_cast<int>(Audio::STEREO)) {
						cmd.addListElement("-stereo");
					}
				}
				if (recordSource != static_cast<int>(Source::AUDIO)) {
					if (recordVideoSize == static_cast<int>(VideoSize::V_640)) {
						cmd.addListElement("-doublesize");
					} else if (recordVideoSize == static_cast<int>(VideoSize::V_960)) {
						cmd.addListElement("-triplesize");
					}
				}

				if (FileOperations::exists(getRecordFilename())) {
					confirmDialog.open(
						strCat("Overwrite recording with name '", recordName, "'?"),
						cmd);
					// note: don't auto generate next name
				} else {
					manager.executeDelayed(cmd);
				}
			}
		});
		if (recording && ImGui::Button("Stop")) {
			manager.executeDelayed(makeTclList("record", "stop"));
		}
		ImGui::Separator();
		if (ImGui::Button("Open recordings folder")) {
			bool recordVideo = recordSource != static_cast<int>(Source::AUDIO);
			std::string_view directory = recordVideo ? AviRecorder::VIDEO_DIR : AviRecorder::AUDIO_DIR;
			SDL_OpenURL(strCat("file://", FileOperations::getUserOpenMSXDir(directory)).c_str());
		}
	});
}

void ImGuiTools::paintNotes()
{
	for (auto [i, note] : enumerate(notes)) {
		if (!note.show) continue;

		im::Window(tmpStrCat("Note ", i + 1).c_str(), &note.show, [&]{
			ImGui::InputTextMultiline("##text", &note.text, {-FLT_MIN, -FLT_MIN});
		});
	}
}

} // namespace openmsx
