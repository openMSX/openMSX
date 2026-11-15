#ifndef HD_HH
#define HD_HH

#include "HDCommand.hh"

#include "DiskContainer.hh"
#include "File.hh"
#include "Filename.hh"
#include "MSXMotherBoard.hh"
#include "SectorAccessibleDisk.hh"
#include "serialize_meta.hh"

#include "TigerTree.hh"

#include <bitset>
#include <optional>
#include <string>

namespace openmsx {

class DeviceConfig;

class HD : public SectorAccessibleDisk, public DiskContainer
         , public TTData, public MediaProvider
{
public:
	static constexpr unsigned MAX_HD = 26;
	/** YES: the image can be changed or ejected while the MSX is powered on
	  * (an SD card), NO: only while powered off (a hard disk). */
	enum class HotSwap : bool { NO, YES };
	using HDInUse = std::bitset<MAX_HD>;
	static std::shared_ptr<HDInUse> getDrivesInUse(MSXMotherBoard& motherBoard);

public:
	explicit HD(const DeviceConfig& config, HotSwap hotSwap = HotSwap::NO);
	~HD() override;

	[[nodiscard]] const std::string& getName() const { return name; }
	[[nodiscard]] const Filename& getImageName() const { return filename; }
	void switchImage(const Filename& filename);
	void eject(); // only for hot-swappable HDs
	// Only an SD card can be ejected; SdCard checks this before any sector access.
	[[nodiscard]] bool isEjected() const { return hotSwappable && !file.is_open(); }
	[[nodiscard]] bool isHotSwappable() const { return hotSwappable; }
	// DiskContainer: true once after an image change or eject (read by SdCard)
	[[nodiscard]] bool diskChanged() override;

	[[nodiscard]] std::string getTigerTreeHash();

	// MediaInfoProvider
	void getMediaInfo(TclObject& result) override;
	void setMedia(const TclObject& info, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	[[nodiscard]] MSXMotherBoard& getMotherBoard() const { return motherBoard; }

private:
	// SectorAccessibleDisk:
	void readSectorsImpl(
		std::span<SectorBuffer> buffers, size_t startSector) override;
	void writeSectorImpl(size_t sector, const SectorBuffer& buf) override;
	[[nodiscard]] size_t getNbSectorsImpl() override;
	[[nodiscard]] bool isWriteProtectedImpl() const override;
	[[nodiscard]] Sha1Sum getSha1SumImpl(FilePool& filePool) override;

	// DiskContainer:
	[[nodiscard]] SectorAccessibleDisk* getSectorAccessibleDisk() override;
	[[nodiscard]] std::string_view getContainerName() const override;
	int insertDisk(const std::string& newFilename) override;

	// TTData
	[[nodiscard]] uint8_t* getData(size_t offset, size_t size) override;
	[[nodiscard]] bool isCacheStillValid(time_t& time) override;

	void showProgress(size_t position, size_t maxPosition);
	void closeImage();

private:
	MSXMotherBoard& motherBoard;
	std::string name;
	std::optional<HDCommand> hdCommand; // delayed init
	std::optional<TigerTree> tigerTree; // delayed init

	File file;
	Filename filename;
	size_t filesize = 0;

	std::shared_ptr<HDInUse> hdInUse;

	uint64_t lastProgressTime;
	bool changed = false; // image changed or ejected, see diskChanged()
	const bool hotSwappable;
	bool everDidProgress;
};

REGISTER_BASE_CLASS(HD, "HD");
SERIALIZE_CLASS_VERSION(HD, 3);

} // namespace openmsx

#endif
