#ifndef OSDGUILAYER_HH
#define OSDGUILAYER_HH

namespace openmsx {

class OSDGUI;
class OutputDimensions;

class OSDGUILayer final
{
public:
	explicit OSDGUILayer(OSDGUI& gui);
	~OSDGUILayer();

	[[nodiscard]] OSDGUI& getGUI() { return gui; }

	// Layer
	void paint(const OutputDimensions& output);

private:
	OSDGUI& gui;
};

} // namespace openmsx

#endif
