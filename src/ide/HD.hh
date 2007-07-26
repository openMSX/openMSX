// $Id$

#ifndef HD_HH
#define HD_HH

#include "openmsx.hh"
#include <string>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class HDCommand;
class File;
class XMLElement;

class HD
{
public:
	HD(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~HD();

	const std::string& getName() const;

protected:
	void readFromImage(unsigned offset, unsigned size, byte* buf);
	void writeToImage (unsigned offset, unsigned size, const byte* buf);
	unsigned getImageSize() const;
	std::string getImageURL() const;
	bool isImageReadOnly() const;

private:
	MSXMotherBoard& motherBoard;
	std::string name;
	std::auto_ptr<HDCommand> hdCommand;
	std::auto_ptr<File> file;

	friend class HDCommand;
};

} // namespace openmsx

#endif
