#include "DiskName.hh"
#include "serialize.hh"

namespace openmsx {

DiskName::DiskName(Filename name_)
	: name(std::move(name_))
{
}

DiskName::DiskName(Filename name_, std::string extra_)
	: name(std::move(name_))
	, extra(std::move(extra_))
{
}

std::string DiskName::getOriginal() const
{
	return name.getOriginal() + extra;
}

std::string DiskName::getResolved() const
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

void DiskName::serialize(Archive auto& ar, unsigned /*version*/)
{
	ar.serialize("filename", name,
	             "extra",    extra);
}
INSTANTIATE_SERIALIZE_METHODS(DiskName);

} // namespace openmsx
