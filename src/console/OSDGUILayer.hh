#ifndef OSDGUILAYER_HH
#define OSDGUILAYER_HH

#include "Layer.hh"

namespace openmsx {

class OSDGUI;

class OSDGUILayer final : public Layer
{
public:
	explicit OSDGUILayer(OSDGUI& gui);
	~OSDGUILayer() override;

	[[nodiscard]] OSDGUI& getGUI() { return gui; }

	// Layer
	void paint(OutputSurface& output) override;

private:
	OSDGUI& gui;
};

} // namespace openmsx

#endif
