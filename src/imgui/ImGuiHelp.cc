#include "ImGuiHelp.hh"

#include "ImGuiCpp.hh"
#include "ImGuiUtils.hh"
#include "FileContext.hh"
#include "GLImage.hh"

#include "Version.hh"
#include "build-info.hh"
#include "components.hh"

#include <cassert>

namespace openmsx {

void ImGuiHelp::showMenu(MSXMotherBoard* /*motherBoard*/)
{
	im::Menu("Help", [&]{
		ImGui::MenuItem("User manual", nullptr, &showHelpWindow);
		ImGui::MenuItem("About", nullptr, &showAboutWindow);
	});
}

void ImGuiHelp::paint(MSXMotherBoard* /*motherBoard*/)
{
	if (showHelpWindow) paintHelp();
	if (showAboutWindow) paintAbout();
}

void ImGuiHelp::paintHelp()
{
	im::Window("Help", &showHelpWindow, [&]{
		static const char* const TEST = R"(
# User manual

Just an idea: maybe we can translate the user manual from HTML to markdown, and then use this both:
* To generate the HTML pages from (writing markdown is easier than writing HTML).
* To display them in the help menu in openMSX itself.

Here's a link to [the openMSX homepage](https://openmsx.org/) (you can click it).

1. *This is italic.*
1. **This is bold.**
1. ***This is italic and bold.***

Test proportional font:<br>
iiiiiiiiiii<br>
mmmmmmmmmmm

# Table

Name &nbsp; &nbsp; &nbsp; &nbsp; | Multiline &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;<br>header  | [Link&nbsp;](#link1)
:------|:-------------------|:--
Value-One | Long <br>explanation <br>with \<br\>\'s|1
~~Value-Two~~ | __text auto wrapped__\: long explanation here |25 37 43 56 78 90
**etc** | [~~Some **link**~~](https://github.com/mekhontsev/imgui_md)|3

# List

1. First ordered list item
2. Another item
   * Unordered sub-list 1.
   * Unordered sub-list 2.
1. Actual numbers don't matter, just that it's a number
   1. **Ordered** sub-list 1
   2. **Ordered** sub-list 2
4. And another item with minuses.
   - __sub-list with underline__
   - sub-list with escapes: \[looks like\]\(a link\)
5. ~~Item with pluses and strikethrough~~.
   + sub-list 1
   + sub-list 2
   + [Just a link](https://github.com/mekhontsev/imgui_md).
      * Item with [link1](#link1)
      * Item with bold [**link2**](#link1)
)";
		markdown.print(TEST);
	});
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
