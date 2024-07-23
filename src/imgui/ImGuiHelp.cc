#include "ImGuiHelp.hh"

#include "ImGuiCpp.hh"
#include "ImGuiUtils.hh"

#include "FileContext.hh"
#include "FileOperations.hh"
#include "GLImage.hh"

#include "Version.hh"
#include "build-info.hh"
#include "components.hh"

#include <cassert>

namespace openmsx {

using namespace std::literals;

void ImGuiHelp::showMenu(MSXMotherBoard* /*motherBoard*/)
{
	im::Menu("Help", [&]{
		auto docDir = FileOperations::getSystemDocDir();
		im::Menu("Manual", [&]{
			drawURL("FAQ", strCat("file://", docDir, "/manual/faq.html"));
			drawURL("Setup Guide", strCat("file://", docDir, "/manual/setup.html"));
			drawURL("User Manual", strCat("file://", docDir, "/manual/user.html"));
		});
		ImGui::MenuItem("Dear ImGui user guide", nullptr, &showImGuiUserGuide);
		ImGui::Separator();
		ImGui::MenuItem("About openMSX", nullptr, &showAboutOpenMSX);
		ImGui::MenuItem("About Dear ImGui", nullptr, &showAboutImGui);
	});
}

void ImGuiHelp::paint(MSXMotherBoard* /*motherBoard*/)
{
	if (showAboutOpenMSX) paintAbout();
	if (showAboutImGui) ImGui::ShowAboutWindow(&showAboutImGui);
	if (showImGuiUserGuide) {
		im::Window("Dear ImGui User Guide", &showImGuiUserGuide, [&]{
			ImGui::ShowUserGuide();
		});
	}
}

void ImGuiHelp::paintAbout()
{
	im::Window("About openMSX", &showAboutOpenMSX, [&]{
		if (!logo) {
			logo.emplace(); // initialize with null-image
			try {
				FileContext context = systemFileContext();
				auto r = context.resolve("icons/openMSX-logo-256.png");
				gl::ivec2 isize;
				logo->texture = loadTexture(r, isize);
				logo->size = gl::vec2(isize);
			} catch (...) {
				// ignore, don't try again
			}
		}
		assert(logo);
		if (logo->texture.get()) {
			ImGui::Image(logo->texture.getImGui(), logo->size);
			ImGui::SameLine();
		}
		im::Group([&]{
			ImGui::TextUnformatted(Version::full());
			ImGui::Spacing();
			im::Table("##table", 2, ImGuiTableFlags_SizingFixedFit, [&]{
				if (ImGui::TableNextColumn()) ImGui::TextUnformatted("platform:"sv);
				if (ImGui::TableNextColumn()) ImGui::TextUnformatted(TARGET_PLATFORM);

				if (ImGui::TableNextColumn()) ImGui::TextUnformatted("target CPU:"sv);
				if (ImGui::TableNextColumn()) ImGui::TextUnformatted(TARGET_CPU);

				if (ImGui::TableNextColumn()) ImGui::TextUnformatted("flavour:"sv);
				if (ImGui::TableNextColumn()) ImGui::TextUnformatted(BUILD_FLAVOUR);

				if (ImGui::TableNextColumn()) ImGui::TextUnformatted("components:"sv);
				if (ImGui::TableNextColumn()) ImGui::TextUnformatted(BUILD_COMPONENTS);
			});
			ImGui::Spacing();
			ImGui::TextUnformatted("license: GPL2"sv);
			ImGui::Text("%s The openMSX Team", Version::COPYRIGHT);

			ImGui::TextUnformatted("Visit our website:"sv);
			ImGui::SameLine();
			drawURL("openMSX.org", "https://openmsx.org");
		});
	});
}

} // namespace openmsx
