#ifndef HD_HH
#define HD_HH

#include "Filename.hh"
#include "File.hh"
#include "SectorAccessibleDisk.hh"
#include "DiskContainer.hh"
#include "TigerTree.hh"
#include "serialize_meta.hh"
#include <bitset>
#include <string>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class HDCommand;
class DeviceConfig;

class HD : public SectorAccessibleDisk, public DiskContainer
         , public TTData
{
public:
	explicit HD(const DeviceConfig& config);
	~HD() override;

	[[nodiscard]] const std::string& getName() const { return name; }
	[[nodiscard]] const Filename& getImageName() const { return filename; }
	void switchImage(const Filename& filename);

	[[nodiscard]] std::string getTigerTreeHash();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	[[nodiscard]] MSXMotherBoard& getMotherBoard() const { return motherBoard; }

private:
	// SectorAccessibleDisk:
	void readSectorsImpl(
		SectorBuffer* buffers, size_t startSector, size_t num) override;
	void writeSectorImpl(size_t sector, const SectorBuffer& buf) override;
	[[nodiscard]] size_t getNbSectorsImpl() const override;
	[[nodiscard]] bool isWriteProtectedImpl() const override;
	[[nodiscard]] Sha1Sum getSha1SumImpl(FilePool& filePool) override;

	// Diskcontainer:
	[[nodiscard]] SectorAccessibleDisk* getSectorAccessibleDisk() override;
	[[nodiscard]] const std::string& getContainerName() const override;
	[[nodiscard]] bool diskChanged() override;
	int insertDisk(const std::string& newFilename) override;

	// TTData
	[[nodiscard]] uint8_t* getData(size_t offset, size_t size) override;
	[[nodiscard]] bool isCacheStillValid(time_t& time) override;

	void showProgress(size_t position, size_t maxPosition);

private:
	MSXMotherBoard& motherBoard;
	std::string name;
	std::unique_ptr<HDCommand> hdCommand;
	std::unique_ptr<TigerTree> tigerTree;

	File file;
	Filename filename;
	size_t filesize;

	static constexpr unsigned MAX_HD = 26;
	using HDInUse = std::bitset<MAX_HD>;
	std::shared_ptr<HDInUse> hdInUse;

	uint64_t lastProgressTime;
	bool everDidProgress;
};

REGISTER_BASE_CLASS(HD, "HD");
SERIALIZE_CLASS_VERSION(HD, 2);

} // namespace openmsx

#endif
