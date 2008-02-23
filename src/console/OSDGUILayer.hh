// $Id$

#ifndef OSDGUILAYER_HH
#define OSDGUILAYER_HH

#include "Layer.hh"

namespace openmsx {

class OSDGUI;

class OSDGUILayer : public Layer
{
public:
	OSDGUI& getGUI();

	// Layer
	virtual const std::string& getName();

protected:
	explicit OSDGUILayer(OSDGUI& gui);
	~OSDGUILayer();

private:
	OSDGUI& gui;
};

class SDLOSDGUILayer : public OSDGUILayer
{
public:
	explicit SDLOSDGUILayer(OSDGUI& gui);

	// Layer
	virtual void paint();
};

class GLOSDGUILayer : public OSDGUILayer
{
public:
	explicit GLOSDGUILayer(OSDGUI& gui);

	// Layer
	virtual void paint();
};

} // namespace openmsx

#endif
