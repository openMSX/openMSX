#ifndef OSDGUILAYER_HH
#define OSDGUILAYER_HH

#include "Layer.hh"

namespace openmsx {

class OSDGUI;

class OSDGUILayer : public Layer
{
public:
	OSDGUI& getGUI() { return gui; }

protected:
	explicit OSDGUILayer(OSDGUI& gui);
	~OSDGUILayer();

private:
	OSDGUI& gui;
};

class SDLOSDGUILayer final : public OSDGUILayer
{
public:
	explicit SDLOSDGUILayer(OSDGUI& gui);

	// Layer
	void paint(OutputSurface& output) override;
};

class GLOSDGUILayer final : public OSDGUILayer
{
public:
	explicit GLOSDGUILayer(OSDGUI& gui);

	// Layer
	void paint(OutputSurface& output) override;
};

} // namespace openmsx

#endif
