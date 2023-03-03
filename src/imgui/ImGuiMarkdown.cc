#include "ImGuiMarkdown.hh"
#include "ImGuiLayer.hh"
#include <SDL.h>

namespace openmsx {

void ImGuiMarkdown::print(std::string_view str)
{
	ImGui::PushFont(layer.vera13);
	imgui_md::print(str.begin(), str.end());
	ImGui::PopFont();
}

ImFont* ImGuiMarkdown::get_font() const
{
	if (m_is_table_header) return layer.veraBold13;

	switch (m_hlevel) {
	case 0:
		return m_is_strong ? (m_is_em ? layer.veraBoldItalic13
		                              : layer.veraBold13)
		                   : (m_is_em ? layer.veraItalic13
		                              : layer.vera13);
	case 1:
		return layer.veraBold16;
	default:
		return layer.veraBold13;
	}
}

void ImGuiMarkdown::open_url() const
{
	if (SDL_VERSION_ATLEAST(2, 0, 14)) {
		SDL_OpenURL(m_href.c_str());
	}
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
