// $Id$

#ifndef OSDWIDGET_HH
#define OSDWIDGET_HH

#include "openmsx.hh"
#include <string>
#include <set>

namespace openmsx {

class OSDGUI;
class OutputSurface;

class OSDWidget
{
public:
	virtual ~OSDWidget();

	std::string getName() const;
	int  getZ()     const { return z; }
	byte getRed()   const { return r; }
	byte getGreen() const { return g; }
	byte getBlue()  const { return b; }
	byte getAlpha() const { return a; }

	virtual void getProperties(std::set<std::string>& result) const;
	virtual void setProperty(const std::string& name, const std::string& value);
	virtual std::string getProperty(const std::string& name) const;
	virtual std::string getType() const = 0;

	virtual void paintSDL(OutputSurface& output) = 0;
	virtual void paintGL (OutputSurface& output) = 0;
	virtual void invalidate() = 0;

protected:
	OSDWidget(OSDGUI& gui);

	/** Returns true if RGBA changed.
	 * Separate RGB- and Alpha-changed info is stored in the optional
	 * bool* parameters. */
	bool setRGBA(const std::string& value,
	             bool* rgbChanged = NULL, bool* alphaChanged = NULL);
	/** Returns true if RGB changed. */
	bool setRGB(const std::string& value);
	/** Returns true if alpha changed. */
	bool setAlpha(const std::string& value);

private:
	OSDGUI& gui;
	unsigned id;
	int z;
	byte r, g, b, a; // note: when there are widgets without color, move
	                 //       this to an abstact colored-widget class
};

} // namespace openmsx

#endif
