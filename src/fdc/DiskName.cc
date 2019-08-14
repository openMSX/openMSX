#include "DiskName.hh"
#include "serialize.hh"

using std::string;

namespace openmsx {

DiskName::DiskName(Filename name_)
	: name(std::move(name_))
{
}

DiskName::DiskName(Filename name_, string extra_)
	: name(std::move(name_))
	, extra(std::move(extra_))
{
}

string DiskName::getOriginal() const
{
	return name.getOriginal() + extra;
}

string DiskName::getResolved() const
{
	return name.getResolved() + extra;
}

void DiskName::updateAfterLoadState()
{
	name.updateAfterLoadState();
}

bool DiskName::empty() const
{
	return name.empty() && extra.empty();
}

template<typename Archive>
void DiskName::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("filename", name,
	             "extra",    extra);
}
INSTANTIATE_SERIALIZE_METHODS(DiskName);

} // namespace openmsx
