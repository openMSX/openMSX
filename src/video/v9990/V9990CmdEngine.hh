// $Id$

#ifndef __V9990CMDENGINE_HH__
#define __V9990CMDENGINE_HH__

#include "openmsx.hh"
#include "V9990.hh"

namespace openmsx {

class V9990VRAM;
class EmuTime;

/** Command engine.
  */
class V9990CmdEngine {
public:
	/** Constructor
	  */
	V9990CmdEngine(V9990* vdp_);

	/** Destructor
	  */
	~V9990CmdEngine();

	/** Re-initialise the command engine's state
	  * @param time   Moment in emulated time the reset occurs
	  */
	void reset(const EmuTime& time);

	/** Set a value to one of the command registers
	  */
	void setCmdReg(byte reg, byte val, const EmuTime& time);

	/** set the data byte
	  */
	void setCmdData(byte value, const EmuTime& time);

	/** read the command data byte
	  */
	byte getCmdData(const EmuTime& time);

private:
	class V9990Bpp2 {
	public:
		static const word BITS_PER_PIXEL  = 2;
		static const word MASK            = 0x0003;
		static const word PIXELS_PER_BYTE = 4;
		static const word ADDRESS_MASK    = 0x0003;
		static inline uint addressOf(int x, int y, int imageWidth);
		static inline byte writeMask(int x);
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
		static inline byte writeMask(int x);
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
		static inline byte writeMask(int x);
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
		static inline byte writeMask(int x);
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

	// Just to make life (typing, that is) a bit easier...
	#define CLASS_COMMAND(NAME)                            \
		template <class Mode>                              \
		class NAME: public V9990Cmd {                      \
		public:                                            \
			NAME(V9990CmdEngine* engine, V9990VRAM* vram); \
			virtual void start(const EmuTime& time);       \
			virtual void execute(const EmuTime& time);     \
		}

	CLASS_COMMAND(CmdLMMC);
	CLASS_COMMAND(CmdLMMV);
	CLASS_COMMAND(CmdLMCM);
	CLASS_COMMAND(CmdLMMM);
	CLASS_COMMAND(CmdCMMC);
	CLASS_COMMAND(CmdCMMK);
	CLASS_COMMAND(CmdCMMM);
	CLASS_COMMAND(CmdBMXL);
	CLASS_COMMAND(CmdBMLX);
	CLASS_COMMAND(CmdBMLL);
	CLASS_COMMAND(CmdLINE);
	CLASS_COMMAND(CmdSRCH);
	CLASS_COMMAND(CmdPOINT);
	CLASS_COMMAND(CmdPSET);
	CLASS_COMMAND(CmdADVN);

	/** V9990 VDP this engine belongs to
	  */
	V9990 *vdp;

	/** VRAM
	  */
	//V9990VRAM* vram;
	
	/** Transfer a byte to/from the CPU
	  */
	bool transfer;

	/** Data byte to transfer between V9990 and CPU
	  */
	byte data;

	/** All commands
	  */
	V9990Cmd *commands[16][BP2 + 1];

	/** The current command
	  */
	V9990Cmd* currentCommand;

	/** Command parameters
	  */
	word SX, SY, DX, DY, NX, NY;
	byte COL, ARG, CMD, LOG;

	/** VRAM read/write address for xMMC and xMCM commands
	  */
	uint address;

	/** counters
	  */
	word ANX, ANY;
	
	/** Create the engines for a given command.
	  * For each bitdepth, a separate engine is created.
	  */
	template <template <class Mode> class Command>
	void createEngines(int cmd);

	/** 
	  */
	/** Perform the specified logical operation on a byte
	  */
	byte logOp(byte src, byte dest);

	/** Perform the specified operation on a pixel, specified as
	  * inverse write mask (eg. 0xF0, will write in lower nibble and
	  * leave upper nibble intact)
	  */
	 byte logOp(byte src, byte dest, byte mask);

	/** The running command is complete. Perform neccessary clean-up actions.
	  */
	void cmdReady(void);
};

} // namespace openmsx

#endif

