#include "ImGuiMarkdown.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"

#include <SDL.h>

namespace openmsx {

void ImGuiMarkdown::print(std::string_view str)
{
	im::Font(manager.vera13, [&]{ // initial font
		imgui_md::print(str.data(), str.data() + str.size()); // this can still change font
	});
}

ImFont* ImGuiMarkdown::get_font() const
{
	if (m_is_table_header) return manager.veraBold13;

	switch (m_hlevel) {
	case 0:
		return m_is_strong ? (m_is_em ? manager.veraBoldItalic13
		                              : manager.veraBold13)
		                   : (m_is_em ? manager.veraItalic13
		                              : manager.vera13);
	case 1:
		return manager.veraBold16;
	default:
		return manager.veraBold13;
	}
}

void ImGuiMarkdown::open_url() const
{
#if SDL_VERSION_ATLEAST(2, 0, 14)
	SDL_OpenURL(m_href.c_str());
#endif
}

/*bool ImGuiMarkdown::get_image(image_info& nfo) const
{
	//use m_href to identify images
	nfo.texture_id = g_texture1;
	nfo.size = {40,20};
	nfo.uv0 = { 0,0 };
	nfo.uv1 = {1,1};
	nfo.col_tint = { 1,1,1,1 };
	nfo.col_border = { 0,0,0,0 };
	return true;
}*/

/*void ImGuiMarkdown::html_div(const std::string& divClass, bool e)
{
	if (divClass == "red") {
		if (e) {
			m_table_border = false;
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		} else {
			ImGui::PopStyleColor();
			m_table_border = true;
		}
	}
}*/

} // namespace openmsx
