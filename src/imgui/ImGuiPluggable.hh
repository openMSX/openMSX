#ifndef IMGUI_PLUGGABLE_HH
#define IMGUI_PLUGGABLE_HH

namespace openmsx {

class ImGuiPluggable
{
public:
	virtual ~ImGuiPluggable() = default;
	virtual void renderGuiExtra() = 0;
};

} // namespace openmsx

#endif
