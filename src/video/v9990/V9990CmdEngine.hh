// $Id$

#ifndef V9990CMDENGINE_HH
#define V9990CMDENGINE_HH

#include "openmsx.hh"
#include "V9990ModeEnum.hh"
#include "BooleanSetting.hh"

namespace openmsx {

class V9990;
class V9990VRAM;
class EmuTime;

/** Command engine.
  */
class V9990CmdEngine {
public:
	// status bits
	static const byte TR = 0x80;
	static const byte BD = 0x10;
	static const byte CE = 0x01;
	
	/** Constructor
	  */
	V9990CmdEngine(V9990* vdp, const EmuTime& time);

	/** Destructor
	  */
	~V9990CmdEngine();

	/** Re-initialise the command engine's state
	  * @param time   Moment in emulated time the reset occurs
	  */
	void reset(const EmuTime& time);

	/** Synchronises the command engine with the V9990
	  * @param time The moment in emulated time to sync to.
	  */  
	inline void sync(const EmuTime& time) {
		if (currentCommand) currentCommand->execute(time);
	}
	
	/** Set a value to one of the command registers
	  */
	void setCmdReg(byte reg, byte val, const EmuTime& time);

	/** set the data byte
	  */
	void setCmdData(byte value, const EmuTime& time);

	/** read the command data byte
	  */
	byte getCmdData(const EmuTime& time);

	/** Get command engine related status bits
	  *  - TR command data transfer ready (bit 7)
	  *  - BD border color detect         (bit 4)   
	  *  - CE command being executed      (bit 0)
	  */
	byte getStatus(const EmuTime& time) {
		sync(time);
		return status;
	}

	word getBorderX(const EmuTime& time) {
		sync(time);
		return borderX;
	}

private:
	class V9990Bpp2 {
	public:
		static const word BITS_PER_PIXEL  = 2;
		static const word MASK            = 0x0003;
		static const word PIXELS_PER_BYTE = 4;
		static const word ADDRESS_MASK    = 0x0003;
		static inline uint addressOf(int x, int y, int imageWidth);
		static inline word shiftDown(word data, int x);
		static inline word point(V9990VRAM* vram, 
		                         int x, int y, int imageWidth);
		static inline void pset(V9990VRAM* vram,
		                        int x, int y, int imageWidth,
		                        word color);
	};
	
	class V9990Bpp4 {
	public:
		static const word BITS_PER_PIXEL  = 4;
		static const word MASK            = 0x000F;
		static const word PIXELS_PER_BYTE = 2;
		static const word ADDRESS_MASK    = 0x0001;
		static inline uint addressOf(int x, int y, int imageWidth);
		static inline word shiftDown(word data, int x);
		static inline word point(V9990VRAM* vram, 
		                         int x, int y, int imageWidth);
		static inline void pset(V9990VRAM* vram,
		                        int x, int y, int imageWidth,
		                        word color);
	};
	
	class V9990Bpp8 {
	public:
		static const word BITS_PER_PIXEL  = 8;
		static const word MASK            = 0x00FF;
		static const word PIXELS_PER_BYTE = 1;
		static const word ADDRESS_MASK    = 0x0000;
		static inline uint addressOf(int x, int y, int imageWidth);
		static inline word shiftDown(word data, int x);
		static inline word point(V9990VRAM* vram, 
		                         int x, int y, int imageWidth);
		static inline void pset(V9990VRAM* vram,
		                        int x, int y, int imageWidth,
		                        word color);
	};
	
	class V9990Bpp16 {
	public:
		static const word BITS_PER_PIXEL  = 16;
		static const word MASK            = 0xFFFF;
		static const word PIXELS_PER_BYTE = 0;
		static const word ADDRESS_MASK    = 0x0000;
		static inline uint addressOf(int x, int y, int imageWidth);
		static inline word shiftDown(word data, int x);
		static inline word point(V9990VRAM* vram, 
		                         int x, int y, int imageWidth);
		static inline void pset(V9990VRAM* vram,
		                        int x, int y, int imageWidth,
		                        word color);
	};
	
	/** This is an abstract base class for V9990 commands
	  */
	class V9990Cmd {
	public:
		V9990Cmd(V9990CmdEngine* engine_, V9990VRAM* vram_);
		virtual ~V9990Cmd();

		virtual void start(const EmuTime& time) = 0;
		virtual void execute(const EmuTime &time) = 0;
	
	protected:
		V9990CmdEngine* engine;
		V9990VRAM*      vram;
	};

	class CmdSTOP: public V9990Cmd {
	public:
		CmdSTOP(V9990CmdEngine* engine, V9990VRAM* vram);
		virtual void start(const EmuTime& time);
		virtual void execute(const EmuTime& time);
	};

	template <class Mode>
	class CmdLMMC: public V9990Cmd {
	public:
		CmdLMMC(V9990CmdEngine* engine, V9990VRAM* vram);
		virtual void start(const EmuTime& time);
		virtual void execute(const EmuTime& time);
	};

	template <class Mode>
	class CmdLMMV: public V9990Cmd {
	public:
		CmdLMMV(V9990CmdEngine* engine, V9990VRAM* vram);
		virtual void start(const EmuTime& time);
		virtual void execute(const EmuTime& time);
	};

	template <class Mode>
	class CmdLMCM: public V9990Cmd {
	public:
		CmdLMCM(V9990CmdEngine* engine, V9990VRAM* vram);
		virtual void start(const EmuTime& time);
		virtual void execute(const EmuTime& time);
	};

	template <class Mode>
	class CmdLMMM: public V9990Cmd {
	public:
		CmdLMMM(V9990CmdEngine* engine, V9990VRAM* vram);
		virtual void start(const EmuTime& time);
		virtual void execute(const EmuTime& time);
	};

	template <class Mode>
	class CmdCMMC: public V9990Cmd {
	public:
		CmdCMMC(V9990CmdEngine* engine, V9990VRAM* vram);
		virtual void start(const EmuTime& time);
		virtual void execute(const EmuTime& time);
	};

	template <class Mode>
	class CmdCMMK: public V9990Cmd {
	public:
		CmdCMMK(V9990CmdEngine* engine, V9990VRAM* vram);
		virtual void start(const EmuTime& time);
		virtual void execute(const EmuTime& time);
	};

	template <class Mode>
	class CmdCMMM: public V9990Cmd {
	public:
		CmdCMMM(V9990CmdEngine* engine, V9990VRAM* vram);
		virtual void start(const EmuTime& time);
		virtual void execute(const EmuTime& time);
	};

	template <class Mode>
	class CmdBMXL: public V9990Cmd {
	public:
		CmdBMXL(V9990CmdEngine* engine, V9990VRAM* vram);
		virtual void start(const EmuTime& time);
		virtual void execute(const EmuTime& time);
	};

	template <class Mode>
	class CmdBMLX: public V9990Cmd {
	public:
		CmdBMLX(V9990CmdEngine* engine, V9990VRAM* vram);
		virtual void start(const EmuTime& time);
		virtual void execute(const EmuTime& time);
	};

	template <class Mode>
	class CmdBMLL: public V9990Cmd {
	public:
		CmdBMLL(V9990CmdEngine* engine, V9990VRAM* vram);
		virtual void start(const EmuTime& time);
		virtual void execute(const EmuTime& time);
	};

	template <class Mode>
	class CmdLINE: public V9990Cmd {
	public:
		CmdLINE(V9990CmdEngine* engine, V9990VRAM* vram);
		virtual void start(const EmuTime& time);
		virtual void execute(const EmuTime& time);
	};

	template <class Mode>
	class CmdSRCH: public V9990Cmd {
	public:
		CmdSRCH(V9990CmdEngine* engine, V9990VRAM* vram);
		virtual void start(const EmuTime& time);
		virtual void execute(const EmuTime& time);
	};

	template <class Mode>
	class CmdPOINT: public V9990Cmd {
	public:
		CmdPOINT(V9990CmdEngine* engine, V9990VRAM* vram);
		virtual void start(const EmuTime& time);
		virtual void execute(const EmuTime& time);
	};

	template <class Mode>
	class CmdPSET: public V9990Cmd {
	public:
		CmdPSET(V9990CmdEngine* engine, V9990VRAM* vram);
		virtual void start(const EmuTime& time);
		virtual void execute(const EmuTime& time);
	};

	template <class Mode>
	class CmdADVN: public V9990Cmd {
	public:
		CmdADVN(V9990CmdEngine* engine, V9990VRAM* vram);
		virtual void start(const EmuTime& time);
		virtual void execute(const EmuTime& time);
	};

	/** V9990 VDP this engine belongs to
	  */
	V9990* vdp;

	/** VRAM
	  */
	//V9990VRAM* vram;

	/** Status bits
	 */
	byte status;

	/** The X coord of a border detected by SRCH
	 */
	word borderX;
	
	/** Data byte to transfer between V9990 and CPU
	  */
	byte data;

	/** Bit counter for CMMx commands
	  */
	byte bitsLeft;

	/** All commands
	  */
	V9990Cmd *commands[16][BP2 + 1];

	/** The current command
	  */
	V9990Cmd* currentCommand;

	/** Command parameters
	  */
	word SX, SY, DX, DY, NX, NY;
	word WM, fgCol, bgCol;
	byte ARG, LOG, CMD;

	/** VRAM read/write address for various commands
	  */
	unsigned srcAddress;
	unsigned dstAddress;
	unsigned nbBytes;

	/** counters
	  */
	word ASX, ADX, ANX, ANY;
	
	/** Only call reportV9990Command() when this setting is turned on
	  */
	BooleanSetting cmdTraceSetting;
	
	/** Create the engines for a given command.
	  * For each bitdepth, a separate engine is created.
	  */
	template <template <class Mode> class Command>
	void createEngines(int cmd);

	/** Perform the specified operation on a pixel, specified as
	  * inverse write mask (eg. 0xF0, will write in lower nibble and
	  * leave upper nibble intact)
	  */
	 word logOp(word src, word dest, word mask);

	/** The running command is complete. Perform neccessary clean-up actions.
	  */
	void cmdReady();

	/** For debugging: Print the info about the current command.
	  */ 
	void reportV9990Command();
};

} // namespace openmsx

#endif
