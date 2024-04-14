#ifndef ABSTRACTIDEDEVICE_HH
#define ABSTRACTIDEDEVICE_HH

#include "IDEDevice.hh"
#include "AlignedBuffer.hh"
#include "serialize_meta.hh"
#include <string>

namespace openmsx {

class MSXMotherBoard;

class AbstractIDEDevice : public IDEDevice
{
public:
	void reset(EmuTime::param time) override;

	[[nodiscard]] word readData(EmuTime::param time) override;
	[[nodiscard]] byte readReg(nibble reg, EmuTime::param time) override;

	void writeData(word value, EmuTime::param time) override;
	void writeReg(nibble reg, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	// Bit flags for the status register:
	static constexpr byte DRDY = 0x40;
	static constexpr byte DSC = 0x10;
	static constexpr byte DRQ = 0x08;
	static constexpr byte ERR = 0x01;

	// Bit flags for the error register:
	static constexpr byte UNC = 0x40;
	static constexpr byte IDNF = 0x10;
	static constexpr byte ABORT = 0x04;

	explicit AbstractIDEDevice(MSXMotherBoard& motherBoard);
	~AbstractIDEDevice() override = default;

	/** Is this device a packet (ATAPI) device?
	  * @return True iff this device supports the packet commands.
	  */
	[[nodiscard]] virtual bool isPacketDevice() = 0;

	/** Gets the device name to insert as "model number" into the identify
	  * block.
	  * @return An ASCII string, up to 40 characters long.
	  */
	[[nodiscard]] virtual std::string_view getDeviceName() = 0;

	/** Tells a subclass to fill the device specific parts of the identify
	  * block located in the buffer.
	  * The generic part is already written there.
	  * @param buffer Array of 512 bytes.
	  */
	virtual void fillIdentifyBlock(AlignedBuffer& buffer) = 0;

	/** Called when a block of read data should be buffered by the controller:
	  * when the buffer is empty or at the start of the transfer.
	  * @param buffer Pointer to the start of a byte array.
	  * @param count Number of bytes to be filled by this method.
	  *   This number will not exceed the array size nor the transfer length.
	  * @return The number of bytes that was added to the array,
	  *   or 0 if the transfer was aborted (the implementation of this method
	  *   must set the relevant error flags as well).
	  */
	[[nodiscard]] virtual unsigned readBlockStart(AlignedBuffer& buffer, unsigned count) = 0;

	/** Called when a read transfer completes.
	  * The default implementation does nothing.
	  */
	virtual void readEnd();

	/** Called when a block of written data has been buffered by the controller:
	  * when the buffer is full or at the end of the transfer.
	  * @param buffer Pointer to the start of a byte array.
	  * @param count Number of data bytes in the array.
	  */
	virtual void writeBlockComplete(AlignedBuffer& buffer, unsigned count) = 0;

	/** Starts execution of an IDE command.
	  * Override this to implement additional commands and make sure you call
	  * the superclass implementation for all commands that you don't handle.
	  */
	virtual void executeCommand(byte cmd);

	/** Indicates an error: sets error register, error flag, aborts transfers.
	  * @param error Value to be written to the error register.
	  */
	void setError(byte error);

	/** Creates an LBA sector address from the contents of the sectorNumReg,
	  * cylinderLowReg, cylinderHighReg and devHeadReg registers.
	  */
	[[nodiscard]] unsigned getSectorNumber() const;

	/** Gets the number of sectors indicated by the sector count register.
	  */
	[[nodiscard]] unsigned getNumSectors() const;

	/** Writes the interrupt reason register.
	  * This is the same as register as sector count, but serves a different
	  * purpose.
	  */
	void setInterruptReason(byte value);

	/** Reads the byte count limit of a packet transfer in the registers.
	  * The cylinder low/high registers are used for this.
	  */
	[[nodiscard]] unsigned getByteCount() const;

	/** Writes the byte count of a packet transfer in the registers.
	  * The cylinder low/high registers are used for this.
	  */
	void setByteCount(unsigned count);

	/** Writes a 28-bit LBA sector number in the registers.
	  * The cylinder low/high registers are used for this.
	  */
	void setSectorNumber(unsigned lba);

	/** Indicates the start of a read data transfer which uses blocks.
	  * The readBlockStart() method is called at the start of each block.
	  * The first block will be read immediately, so make sure you initialise
	  * all variables needed by readBlockStart() before calling this method.
	  * @param count Total number of bytes to transfer.
	  */
	void startLongReadTransfer(unsigned count);

	/** Indicates the start of a read data transfer where all data fits
	  * into the buffer at once.
	  * @param count Total number of bytes to transfer.
	  * @return Pointer to the start of the buffer.
	  *   The caller should write the data there.
	  *   The relevant part of the buffer contains zeroes.
	  */
	[[nodiscard]] AlignedBuffer& startShortReadTransfer(unsigned count);

	/** Aborts the read transfer in progress.
	  */
	void abortReadTransfer(byte error);

	/** Indicates the start of a write data transfer.
	  * @param count Total number of bytes to transfer.
	  */
	void startWriteTransfer(unsigned count);

	/** Aborts the write transfer in progress.
	  */
	void abortWriteTransfer(byte error);

	[[nodiscard]] byte getFeatureReg() const { return featureReg; }
	void setLBALow (byte value) { sectorNumReg    = value; }
	void setLBAMid (byte value) { cylinderLowReg  = value; }
	void setLBAHigh(byte value) { cylinderHighReg = value; }

	[[nodiscard]] MSXMotherBoard& getMotherBoard() const { return motherBoard; }

private:
	/** Perform diagnostic and return result.
	  * Actually, just return success, because we don't emulate faulty hardware.
	  */
	[[nodiscard]] byte diagnostic() const;

	/** Puts special values in the sector address, sector count and device
	  * registers to identify the type of device.
	  * @param preserveDevice If true, preserve the value of the DEV bit;
	  *   if false, set the DEV bit to 0.
	  */
	void createSignature(bool preserveDevice = false);

	/** Puts the output for the IDENTIFY DEVICE command in the buffer.
	  * @param buffer Pointer to the start of the buffer.
	  *   The buffer must be at least 512 bytes in size.
	  */
	void createIdentifyBlock(AlignedBuffer& buffer);

	/** Initialises registers for a data transfer.
	  */
	void startReadTransfer();

	/** Initialises buffer related variables for the next data block.
	  * Calls readBlockStart() to deliver the actual data.
	  */
	void readNextBlock();

	/** Indicates that a read transfer starts.
	  */
	void setTransferRead(bool status);

	/** Initialises buffer related variables for the next data block.
	  * Make sure transferCount is initialised before calling this method.
	  */
	void writeNextBlock();

	/** Indicates that a write transfer starts.
	  */
	void setTransferWrite(bool status);

private:
	MSXMotherBoard& motherBoard;

	/** Data buffer shared by all transfers.
	  * The size must be a multiple of 512.
	  * Right now I don't see any reason to make it larger than the minimum
	  * size of 1 * 512.
	  */
	AlignedByteArray<512> buffer;

	/** Index of current read/write position in the buffer.
	  */
	unsigned transferIdx = 0; // avoid UMR on serialize

	/** Number of bytes remaining in the buffer.
	  */
	unsigned bufferLeft = 0;

	/** Number of bytes remaining in the transfer after this buffer.
	  * (total bytes remaining == transferCount + bufferLeft)
	  */
	unsigned transferCount = 0;

	// ATA registers:
	byte errorReg;
	byte sectorCountReg;
	byte sectorNumReg;
	byte cylinderLowReg;
	byte cylinderHighReg;
	byte devHeadReg;
	byte statusReg;
	byte featureReg;

	bool transferRead = false;
	bool transferWrite = false;
};

REGISTER_BASE_NAME_HELPER(AbstractIDEDevice, "IDEDevice");

} // namespace openmsx

#endif // ABSTRACTIDEDEVICE_HH
