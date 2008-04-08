// $Id$

#ifndef OSDTEXT_HH
#define OSDTEXT_HH

#include "OSDImageBasedWidget.hh"
#include "openmsx.hh"

namespace openmsx {

class TTFFont;

class OSDText : public OSDImageBasedWidget
{
public:
	OSDText(const OSDGUI& gui, const std::string& name);
	~OSDText();

	virtual void getProperties(std::set<std::string>& result) const;
	virtual void setProperty(const std::string& name, const std::string& value);
	virtual std::string getProperty(const std::string& name) const;
	virtual std::string getType() const;

private:
	virtual void invalidateLocal();
	virtual BaseImage* createSDL(OutputSurface& output);
	virtual BaseImage* createGL (OutputSurface& output);
	template <typename IMAGE> BaseImage* create(OutputSurface& output);

	std::string text;
	std::string fontfile;
	int size;
	TTFFont* font;
};

} // namespace openmsx

#endif
