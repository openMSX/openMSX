#ifndef DISKNAME_HH
#define DISKNAME_HH

#include "Filename.hh"

namespace openmsx {

class DiskName
{
public:
	explicit DiskName(Filename name);
	DiskName(Filename name, std::string extra);

	[[nodiscard]] std::string getOriginal() const;
	[[nodiscard]] std::string getResolved() const;
	void updateAfterLoadState();
	[[nodiscard]] bool empty() const;
	[[nodiscard]] const Filename& getFilename() const { return name; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Filename name;
	std::string extra;
};

} // namespace openmsx

#endif
