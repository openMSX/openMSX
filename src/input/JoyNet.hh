// $Id$

#ifndef __JOYNET_HH__
#define __JOYNET_HH__

#include "config.h"
#ifdef	HAVE_SYS_SOCKET_H

#include "JoystickDevice.hh"
#include "Thread.hh"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace openmsx {

/*
 * Standard JoyNet cable for MSX (to link two or more MSX computers) 
 * =================================================================
 * Notice that if you want to link two MSX computers with standard
 * JoyNet cables, you will need two cables.
 *
 * SEND (DIN-5 180/m)   RECV (DIN-5 180/f)
 * 1 --------------+     +---------------1
 * 2 ------------+ |     | +-------------2
 * 3 ----------+ | |     | | +-----------3
 * 5 ----------|-|-|-----|-|-|---+-------5
 *             | | |     | | |   |
 *             | | |     | | |   |
 *             3 7 6     1 2 8   9
 *
 * MSX(DB9 /f)Alternative JoyNet cable for MSX (to link only two MSX computers) 
 * =============================================================================
 *
 * If you want to build an alternative cable to link always just two MSX
 * computers, you can make a cable like the one used by F1 Spirit 3D Special:
 * (DB-9 /f)        (DB-9 /f)
 * 1 --------------- 6
 * 2 --------------- 7
 * 3 --------------- 8
 * 6 --------------- 1
 * 7 --------------- 2
 * 8 --------------- 3
 * 9 --------------- 9
 */

class JoyNet : public JoystickDevice
{
	public:
		JoyNet();
		virtual ~JoyNet();

		//Pluggable
		virtual const string &getName() const;
		virtual void plug(Connector* connector, const EmuTime& time) throw();
		virtual void unplug(const EmuTime& time);

		//JoystickDevice
		virtual byte read(const EmuTime &time);
		virtual void write(byte value, const EmuTime &time);

	private:
		//Sub class for listener thread
		class ConnectionListener: public Runnable
		{
			public:
				ConnectionListener(int listenport,byte* linestatus);
				virtual ~ConnectionListener();
				virtual void run();
			private:
				int port;
				byte* statuspointer;
				Thread thread;
		};

		byte status;
		//For IP connection
		string hostname;
		int portname;

		int sockfd;
		struct sockaddr_in servaddr;

		void setupConnections();
		void setupWriter();
		void sendByte(byte value);
		ConnectionListener* listener;
};

} // namespace openmsx

#endif	// HAVE_SYS_SOCKET_H

#endif	// __JOYNET_HH__
