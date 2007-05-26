// $Id$

#ifndef HD_HH
#define HD_HH

#include <string>

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

	virtual const std::string& getName() const;	

protected:
	std::auto_ptr<File> file;	

private:
	MSXMotherBoard& motherBoard;
	std::string name;
	std::auto_ptr<HDCommand> hdCommand;

	friend class HDCommand;	
};

} // namespace openmsx

#endif
