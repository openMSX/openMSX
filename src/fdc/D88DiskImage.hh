#ifndef D88DISKIMAGE_HH
#define D88DISKIMAGE_HH

#include "Disk.hh"
#include "RawTrack.hh"

namespace openmsx {

class File;

class D88DiskImage final : public Disk
{
public:
    D88DiskImage(const Filename& filename, std::shared_ptr<File> file);

    // Disk
    void readTrack(uint8_t track, uint8_t side, RawTrack& output) override;
    void writeTrackImpl(uint8_t track, uint8_t side, const RawTrack& input) override;

    // SectorAccessibleDisk
	void readSectorImpl (size_t sector,       SectorBuffer& buf) override;
	void writeSectorImpl(size_t sector, const SectorBuffer& buf) override;
	[[nodiscard]] size_t getNbSectorsImpl() const override;
	[[nodiscard]] bool isWriteProtectedImpl() const override;
	[[nodiscard]] Sha1Sum getSha1SumImpl(FilePool& filePool) override;

private:
    std::shared_ptr<File> file;
    std::array<int32_t, 164> trackOffsets;
    int cachedTrackNo;

    int32_t diskSize;
	bool singleSided;
	bool writeProtected;
    uint16_t sectorsPerTrack;

    bool readD88Track(uint8_t track, uint8_t side);

    void detectGeometryFallback() override;

    struct D88SectorData
    {
        uint8_t c;
        uint8_t h;
        uint8_t r;
        uint8_t n;
        uint8_t deleteFlag;
        uint8_t status;
        uint16_t sectorSize;
        uint16_t dataOffset;
        uint16_t dataSize;
    };
    std::vector<uint8_t> trackData;
    std::vector<D88SectorData> trackSectors;
};
}

#endif