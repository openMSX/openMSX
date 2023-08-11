#ifndef HD_HH
#define HD_HH

#include "DiskContainer.hh"
#include "File.hh"
#include "Filename.hh"
#include "HDCommand.hh"
#include "SectorAccessibleDisk.hh"
#include "MSXMotherBoard.hh"
#include "TigerTree.hh"
#include "serialize_meta.hh"
#include <bitset>
#include <string>
#include <optional>

namespace openmsx {

class DeviceConfig;

class HD : public SectorAccessibleDisk, public DiskContainer
         , public TTData, public MediaInfoProvider
{
public:
	static constexpr unsigned MAX_HD = 26;
	using HDInUse = std::bitset<MAX_HD>;
	static std::shared_ptr<HDInUse> getDrivesInUse(MSXMotherBoard& motherBoard);

public:
	explicit HD(const DeviceConfig& config);
	~HD() override;

	[[nodiscard]] const std::string& getName() const { return name; }
	[[nodiscard]] const Filename& getImageName() const { return filename; }
	void switchImage(const Filename& filename);

	[[nodiscard]] std::string getTigerTreeHash();

	// MediaInfoProvider
	void getMediaInfo(TclObject& result) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	[[nodiscard]] MSXMotherBoard& getMotherBoard() const { return motherBoard; }

private:
	// SectorAccessibleDisk:
	void readSectorsImpl(
		std::span<SectorBuffer> buffers, size_t startSector) override;
	void writeSectorImpl(size_t sector, const SectorBuffer& buf) override;
	[[nodiscard]] size_t getNbSectorsImpl() const override;
	[[nodiscard]] bool isWriteProtectedImpl() const override;
	[[nodiscard]] Sha1Sum getSha1SumImpl(FilePool& filePool) override;

	// DiskContainer:
	[[nodiscard]] SectorAccessibleDisk* getSectorAccessibleDisk() override;
	[[nodiscard]] std::string_view getContainerName() const override;
	[[nodiscard]] bool diskChanged() override;
	int insertDisk(const std::string& newFilename) override;

	// TTData
	[[nodiscard]] uint8_t* getData(size_t offset, size_t size) override;
	[[nodiscard]] bool isCacheStillValid(time_t& time) override;

	void showProgress(size_t position, size_t maxPosition);

private:
	MSXMotherBoard& motherBoard;
	std::string name;
	std::optional<HDCommand> hdCommand; // delayed init
	std::optional<TigerTree> tigerTree; // delayed init

	File file;
	Filename filename;
	size_t filesize;

	std::shared_ptr<HDInUse> hdInUse;

	uint64_t lastProgressTime;
	bool everDidProgress;
};

REGISTER_BASE_CLASS(HD, "HD");
SERIALIZE_CLASS_VERSION(HD, 2);

} // namespace openmsx

#endif
