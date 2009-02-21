// $Id$

#ifndef DISKNAME_HH
#define DISKNAME_HH

#include "Filename.hh"

namespace openmsx {

class DiskName
{
public:
	DiskName(const Filename& name, const std::string& extra = "");

	std::string getOriginal() const;
	std::string getResolved() const;
	void updateAfterLoadState(CommandController& controller);
	bool empty() const;
	const Filename& getFilename() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Filename name;
	std::string extra;
};

} // namespace openmsx

#endif
