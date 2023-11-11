#include "ImGuiMessages.hh"

#include "CustomFont.h"
#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "GlobalCliComm.hh"
#include "Reactor.hh"

#include "stl.hh"

#include <imgui_stdlib.h>
#include <imgui.h>

#include <cassert>
#include <concepts>

namespace openmsx {

using namespace std::literals;

ImGuiMessages::ImGuiMessages(ImGuiManager& manager_)
	: manager(manager_)
	, modalMessages(10)
	, popupMessages(10)
	, allMessages(10)
{
	popupAction[CliComm::INFO] = NO_POPUP;
	popupAction[CliComm::WARNING] = POPUP;
	popupAction[CliComm::LOGLEVEL_ERROR] = MODAL_POPUP;
	popupAction[CliComm::PROGRESS] = NO_POPUP;
	openLogAction[CliComm::INFO] = OPEN_LOG;
	openLogAction[CliComm::WARNING] = NO_OPEN_LOG;
	openLogAction[CliComm::LOGLEVEL_ERROR] = NO_OPEN_LOG;
	openLogAction[CliComm::PROGRESS] = NO_OPEN_LOG;

	auto& reactor = manager.getReactor();
	auto& cliComm = reactor.getGlobalCliComm();
	listenerHandle = cliComm.addListener(std::make_unique<Listener>(*this));
}

ImGuiMessages::~ImGuiMessages()
{
	auto& reactor = manager.getReactor();
	auto& cliComm = reactor.getGlobalCliComm();
	cliComm.removeListener(*listenerHandle);
}

void ImGuiMessages::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
	buf.appendf("popupActions=[%d %d %d]\n",
		popupAction[CliComm::LOGLEVEL_ERROR],
		popupAction[CliComm::WARNING],
		popupAction[CliComm::INFO]);
	buf.appendf("openLogActions=[%d %d %d]\n",
		openLogAction[CliComm::LOGLEVEL_ERROR],
		openLogAction[CliComm::WARNING],
		openLogAction[CliComm::INFO]);
}

void ImGuiMessages::loadLine(std::string_view name, zstring_view value)
{
	if (loadOnePersistent(name, value, *this, persistentElements)) {
		// already handled
	} else if (name == "popupActions"sv) {
		std::array<int, 3> a = {};
		if (sscanf(value.c_str(), "[%d %d %d]", &a[0], &a[1], &a[2]) == 3) {
			popupAction[CliComm::LOGLEVEL_ERROR] = PopupAction(a[0]);
			popupAction[CliComm::WARNING]        = PopupAction(a[1]);
			popupAction[CliComm::INFO]           = PopupAction(a[2]);
		}
	} else if (name == "openLogActions"sv) {
		std::array<int, 3> a = {};
		if (sscanf(value.c_str(), "[%d %d %d]", &a[0], &a[1], &a[2]) == 3) {
			openLogAction[CliComm::LOGLEVEL_ERROR] = OpenLogAction(a[0]);
			openLogAction[CliComm::WARNING]        = OpenLogAction(a[1]);
			openLogAction[CliComm::INFO]           = OpenLogAction(a[2]);
		}
	}
}

void ImGuiMessages::paint(MSXMotherBoard* /*motherBoard*/)
{
	paintModal();
	paintPopup();
	paintProgress();
	if (showLog)       paintLog();
	if (showConfigure) paintConfigure();
}

template<std::predicate<std::string_view, std::string_view> Filter = always_true>
static void printMessages(const circular_buffer<ImGuiMessages::Message>& messages, Filter filter = {})
{
	im::TextWrapPos(ImGui::GetFontSize() * 50.0f, [&]{
		for (const auto& message : messages) {
			auto [color, prefix_] = [&]() -> std::pair<ImVec4, std::string_view> {
				switch (message.level) {
					case CliComm::LOGLEVEL_ERROR: return {ImVec4{1.0f, 0.2f, 0.2f, 1.0f}, "Error:"};
					case CliComm::WARNING:        return {ImVec4{1.0f, 0.7f, 0.2f, 1.0f}, "Warning:"};
					case CliComm::INFO:           return {ImVec4{1.0f, 1.0f, 1.0f, 1.0f}, "Info:"};
					default: assert(false);       return {ImVec4{1.0f, 1.0f, 1.0f, 1.0f}, "Unknown:"};
				}
			}();
			auto prefix = prefix_; // clang workaround
			if (std::invoke(filter, prefix, message.text)) {
				im::StyleColor(ImGuiCol_Text, color, [&]{
					ImGui::TextUnformatted(prefix);
					ImGui::SameLine();
					ImGui::TextUnformatted(message.text);
				});
			}
		}
	});
}

bool ImGuiMessages::paintButtons()
{
	ImGui::SetCursorPosX(40.0f);
	bool close = ImGui::Button("Ok");
	ImGui::SameLine(0.0f, 30.0f);
	if (ImGui::SmallButton("Configure...")) {
		close = true;
		showConfigure = true;
	}
	return close;
}

void ImGuiMessages::paintModal()
{
	if (doOpenModal) {
		doOpenModal = false;
		ImGui::OpenPopup("Message");
	}

	bool open = true;
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
	im::PopupModal("Message", &open, ImGuiWindowFlags_AlwaysAutoResize, [&]{
		printMessages(modalMessages);
		bool close = paintButtons();
		if (!open || close) {
			modalMessages.clear();
			ImGui::CloseCurrentPopup();
		}
	});
}

void ImGuiMessages::paintPopup()
{
	// TODO automatically fade-out and close
	if (doOpenPopup) {
		// on first open, clear old messages
		if (!ImGui::IsPopupOpen("popup-message")) {
			while (popupMessages.size() > doOpenPopup) {
				popupMessages.pop_back();
			}
		}
		doOpenPopup = 0;
		ImGui::OpenPopup("popup-message");
	}

	im::Popup("popup-message", [&]{
		printMessages(popupMessages);
		bool close = paintButtons();
		if (close) ImGui::CloseCurrentPopup();
	});
}

void ImGuiMessages::paintProgress()
{
	if (doOpenProgress) {
		doOpenProgress = false;
		ImGui::OpenPopup("popup-progress");
	}

	im::Popup("popup-progress", [&]{
		if (progressFraction >= 1.0f) {
			ImGui::CloseCurrentPopup();
		} else {
			ImGui::TextUnformatted(progressMessage);
			if (progressFraction >= 0.0f) {
				ImGui::ProgressBar(progressFraction);
			} else {
				// unknown fraction, animate progress bar, no label
				progressTime = fmodf(progressTime + ImGui::GetIO().DeltaTime, 2.0f);
				float fraction = (progressTime < 1.0f) ? progressTime : (2.0f - progressTime);
				ImGui::ProgressBar(fraction, {}, "");
			}
		}
	});
}

void ImGuiMessages::paintLog()
{
	if (focusLog) {
		focusLog = false;
		ImGui::SetNextWindowFocus();
	}
	ImGui::SetNextWindowSize(gl::vec2{53, 14} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Message Log", &showLog, [&]{
		const auto& style = ImGui::GetStyle();
		auto buttonHeight = ImGui::GetFontSize() + 2.0f * style.FramePadding.y + style.ItemSpacing.y;
		im::Child("messages", {0.0f, -buttonHeight}, true, ImGuiWindowFlags_HorizontalScrollbar, [&]{
			printMessages(allMessages, [&](std::string_view prefix, std::string_view message) {
				if (filterLog.empty()) return true;
				auto full = tmpStrCat(prefix, message);
				return ranges::all_of(StringOp::split_view<StringOp::REMOVE_EMPTY_PARTS>(filterLog, ' '),
					[&](auto part) { return StringOp::containsCaseInsensitive(full, part); });
			});
		});
		if (ImGui::Button("Clear")) {
			allMessages.clear();
		}
		simpleToolTip("Remove all log messages");
		ImGui::SameLine(0.0f, 30.0f);
		ImGui::TextUnformatted(ICON_IGFD_SEARCH);
		ImGui::SameLine();
		auto size = ImGui::CalcTextSize("Configure..."sv).x + 30.0f + style.WindowPadding.x;
		ImGui::SetNextItemWidth(-size);
		ImGui::InputTextWithHint("##filter", "enter search terms", &filterLog);
		ImGui::SameLine(0.0f, 30.0f);
		if (ImGui::SmallButton("Configure...")) {
			showConfigure = true;
		}
	});
}

void ImGuiMessages::paintConfigure()
{
	ImGui::SetNextWindowSize(gl::vec2{32, 13} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Configure messages", &showConfigure, [&]{
		im::TreeNode("When a message is emitted", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			im::Table("table", 4, ImGuiTableFlags_SizingFixedFit, [&]{
				auto size = ImGui::CalcTextSize("Warning"sv);
				ImGui::TableSetupColumn("");
				ImGui::TableSetupColumn("Error",   ImGuiTableColumnFlags_WidthFixed, size.x);
				ImGui::TableSetupColumn("Warning", ImGuiTableColumnFlags_WidthFixed, size.x);
				ImGui::TableSetupColumn("Info",    ImGuiTableColumnFlags_WidthFixed, size.x);

				//ImGui::TableHeadersRow(); // don't want a colored header row
				if (ImGui::TableNextColumn()) {}
				if (ImGui::TableNextColumn()) ImGui::TextUnformatted("Error"sv);
				if (ImGui::TableNextColumn()) ImGui::TextUnformatted("Warning"sv);
				if (ImGui::TableNextColumn()) ImGui::TextUnformatted("Info"sv);

				if (ImGui::TableNextColumn()) {
					ImGui::TextUnformatted("Show modal popup"sv);
				}
				for (auto level : {CliComm::LOGLEVEL_ERROR, CliComm::WARNING, CliComm::INFO}) {
					if (ImGui::TableNextColumn()) {
						auto& action = popupAction[level];
						if (ImGui::RadioButton(tmpStrCat("##modal" , level).c_str(), action == MODAL_POPUP)) {
							action = (action == MODAL_POPUP) ? NO_POPUP : MODAL_POPUP;
						}
					}
				}

				if (ImGui::TableNextColumn()) {
					ImGui::TextUnformatted("Show non-modal popup"sv);
				}
				for (auto level : {CliComm::LOGLEVEL_ERROR, CliComm::WARNING, CliComm::INFO}) {
					if (ImGui::TableNextColumn()) {
						auto& action = popupAction[level];
						if (ImGui::RadioButton(tmpStrCat("##popup" , level).c_str(), action == POPUP)) {
							action = (action == POPUP) ? NO_POPUP : POPUP;
						}
					}
				}

				if (ImGui::TableNextColumn()) {
					ImGui::TextUnformatted("Open log window and focus"sv);
				}
				for (auto level : {CliComm::LOGLEVEL_ERROR, CliComm::WARNING, CliComm::INFO}) {
					if (ImGui::TableNextColumn()) {
						auto& action = openLogAction[level];
						if (ImGui::RadioButton(tmpStrCat("##focus" , level).c_str(), action == OPEN_LOG_FOCUS)) {
							action = (action == OPEN_LOG_FOCUS) ? NO_OPEN_LOG : OPEN_LOG_FOCUS;
						}
					}
				}

				if (ImGui::TableNextColumn()) {
					ImGui::TextUnformatted("Open log window, without focus"sv);
				}
				for (auto level : {CliComm::LOGLEVEL_ERROR, CliComm::WARNING, CliComm::INFO}) {
					if (ImGui::TableNextColumn()) {
						auto& action = openLogAction[level];
						if (ImGui::RadioButton(tmpStrCat("##log" , level).c_str(), action == OPEN_LOG)) {
							action = (action == OPEN_LOG) ? NO_OPEN_LOG : OPEN_LOG;
						}
					}
				}
			});
		});
	});
}

void ImGuiMessages::log(CliComm::LogLevel level, std::string_view text, float fraction)
{
	if (level == CliComm::PROGRESS) {
		progressMessage = text;
		progressFraction = fraction;
		if (progressFraction < 1.0f) doOpenProgress = true;
		return;
	}

	Message message{level, std::string(text)};

	if (popupAction[level] == MODAL_POPUP) {
		if (modalMessages.full()) modalMessages.pop_back();
		modalMessages.push_front(message);
		doOpenModal = true;
	} else if (popupAction[level] == POPUP) {
		if (popupMessages.full()) popupMessages.pop_back();
		popupMessages.push_front(message);
		doOpenPopup = popupMessages.size();
	}

	if (openLogAction[level] != NO_OPEN_LOG) {
		showLog = true;
		if (openLogAction[level] == OPEN_LOG_FOCUS) {
			focusLog = true;
		}
	}

	if (allMessages.full()) allMessages.pop_back();
	allMessages.push_front(std::move(message));
}

} // namespace openmsx
