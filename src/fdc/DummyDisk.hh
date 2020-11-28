#ifndef FDCDUMMYBACKEND_HH
#define FDCDUMMYBACKEND_HH

#include "SectorBasedDisk.hh"

namespace openmsx {

class DummyDisk final : public SectorBasedDisk
{
public:
	DummyDisk();
	[[nodiscard]] bool isDummyDisk() const override;

private:
	void readSectorImpl (size_t sector,       SectorBuffer& buf) override;
	void writeSectorImpl(size_t sector, const SectorBuffer& buf) override;
	[[nodiscard]] bool isWriteProtectedImpl() const override;
};

} // namespace openmsx

#endif
