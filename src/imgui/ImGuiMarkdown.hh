#ifndef IMGUI_MARKDOWN_HH
#define IMGUI_MARKDOWN_HH

#include <imgui_md.h>

namespace openmsx {

class ImGuiLayer;

class ImGuiMarkdown : public imgui_md
{
public:
	ImGuiMarkdown(ImGuiLayer& layer_)
		: layer(layer_) {}

	ImFont* get_font() const override;
	void open_url() const override;
	//bool get_image(image_info& nfo) const override;
	//void html_div(const std::string& divClass, bool e) override;

	// void print(const char* str, const char* str_end);

private:
	ImGuiLayer& layer;
};

}

#endif
