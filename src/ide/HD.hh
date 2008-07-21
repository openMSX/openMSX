// $Id$

#ifndef HD_HH
#define HD_HH

#include "serialize_meta.hh"
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
	const std::string& getImageName() const;
	void switchImage(const std::string& filename_);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	void readFromImage(unsigned offset, unsigned size, byte* buf);
	void writeToImage (unsigned offset, unsigned size, const byte* buf);
	unsigned getImageSize() const;
	bool isImageReadOnly();

private:
	void openImage();

	MSXMotherBoard& motherBoard;
	std::string name;
	std::auto_ptr<HDCommand> hdCommand;

	std::auto_ptr<File> file;
	std::string filename;
	unsigned filesize;
	bool alreadyTried;
};

REGISTER_BASE_CLASS(HD, "HD");

} // namespace openmsx

#endif
