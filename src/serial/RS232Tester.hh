// $Id$

#ifndef __RS232TESTER_HH__
#define __RS232TESTER_HH__

#include <fstream>
#include <cstdio>
#include <list>
#include "openmsx.hh"
#include "RS232Device.hh"
#include "Thread.hh"
#include "Schedulable.hh"
#include "Semaphore.hh"
#include "Settings.hh"

using std::list;
using std::ofstream;


namespace openmsx {

class RS232Connector;


class RS232Tester : public RS232Device, private Runnable, private Schedulable
{
	public:
		RS232Tester();
		virtual ~RS232Tester();
		
		// Pluggable
		virtual void plug(Connector* connector, const EmuTime& time)
			throw(PlugException);
		virtual void unplug(const EmuTime& time);
		virtual const string &getName() const;

		// input
		virtual void signal(const EmuTime& time);

		// output
		virtual void recvByte(byte value, const EmuTime& time);

	private:
		// Runnable
		virtual void run();

		// Schedulable
		virtual void executeUntilEmuTime(const EmuTime& time, int userData);
		virtual const string& schedName() const;

		Thread thread;
		FILE* inFile;
		RS232Connector* connector;
		list<byte> queue;
		Semaphore lock; // to protect queue

		ofstream outFile;

		StringSetting rs232InputFilenameSetting;
		StringSetting rs232OutputFilenameSetting;
};

} // namespace openmsx

#endif
