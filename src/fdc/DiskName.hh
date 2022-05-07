#ifndef DISKNAME_HH
#define DISKNAME_HH

#include "Filename.hh"
#include "serialize_meta.hh"

namespace openmsx {

class DiskName
{
public:
	/*implicit*/ DiskName(Filename name);
	DiskName(Filename name, std::string extra);

	[[nodiscard]] std::string getOriginal() const;
	[[nodiscard]] std::string getResolved() const;
	void updateAfterLoadState();
	[[nodiscard]] bool empty() const;
	[[nodiscard]] const Filename& getFilename() const { return name; }

	void serialize(Archive auto& ar, unsigned version);

private:
	Filename name;
	std::string extra;
};

} // namespace openmsx

#endif
