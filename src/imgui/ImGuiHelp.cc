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

void ImGuiHelp::showMenu(MSXMotherBoard* /*motherBoard*/)
{
	auto docDir = FileOperations::getSystemDocDir();
	im::Menu("Help", [&]{
		im::Menu("Manual", [&]{
			drawURL("FAQ", strCat("file://", docDir, "/manual/faq.html"));
			drawURL("Setup Guide", strCat("file://", docDir, "/manual/setup.html"));
			drawURL("User Manual", strCat("file://", docDir, "/manual/user.html"));
		});
		ImGui::MenuItem("About", nullptr, &showAboutWindow);
	});
}

void ImGuiHelp::paint(MSXMotherBoard* /*motherBoard*/)
{
	if (showAboutWindow) paintAbout();
}

void ImGuiHelp::paintAbout()
{
	im::Window("About openMSX", &showAboutWindow, [&]{
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
			ImGui::Image(reinterpret_cast<void*>(logo->texture.get()), logo->size);
			ImGui::SameLine();
		}
		im::Group([&]{
			ImGui::TextUnformatted(Version::full());
			ImGui::Spacing();
			ImGui::Text("platform:   %s", TARGET_PLATFORM);
			ImGui::Text("target CPU: %s", TARGET_CPU);
			ImGui::Text("flavour:    %s", BUILD_FLAVOUR);
			ImGui::Text("components: %s", BUILD_COMPONENTS);
			ImGui::Spacing();
			ImGui::TextUnformatted("license: GPL2");
			ImGui::Text("%s The openMSX Team", Version::COPYRIGHT);

			ImGui::TextUnformatted("Visit our website:");
			ImGui::SameLine();
			drawURL("openMSX.org", "https://openmsx.org");
		});
	});
}

} // namespace openmsx
