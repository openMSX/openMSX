// $Id$

#ifndef __JOYNET_HH__
#define __JOYNET_HH__

#include "JoystickDevice.hh"
#include "MSXException.hh"
#include "Thread.hh"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
/*

Standard JoyNet cable for MSX (to link two or more MSX computers) 
=================================================================
Notice that if you want to link two MSX computers with standard
JoyNet cables, you will need two cables.

SEND (DIN-5 180/m)   RECV (DIN-5 180/f)
1 --------------+     +---------------1
2 ------------+ |     | +-------------2
3 ----------+ | |     | | +-----------3
5 ----------|-|-|-----|-|-|---+-------5
            | | |     | | |   |
            | | |     | | |   |
            3 7 6     1 2 8   9
	    
MSX(DB9 /f)Alternative JoyNet cable for MSX (to link only two MSX computers) 
=============================================================================

If you want to build an alternative cable to link always just two MSX
computers, you can make a cable like the one used by F1 Spirit 3D Special:
(DB-9 /f)        (DB-9 /f)
1 --------------- 6
2 --------------- 7
3 --------------- 8
6 --------------- 1
7 --------------- 2
8 --------------- 3
9 --------------- 9
*/

class JoyNetException : public MSXException {
	public:
		JoyNetException(const std::string &desc) : MSXException(desc) {}
};

class JoyNet : public JoystickDevice
{
	public:
		JoyNet(int joyNum);
		virtual ~JoyNet();

		//Pluggable
		virtual const std::string &getName();
		
		//JoystickDevice
		virtual byte read(const EmuTime &time);
		virtual void write(byte value, const EmuTime &time);

		//Sub class for listener thread
		class connectionListener: public Runnable
		{
			public:
			  connectionListener(int listenport,byte* linestatus);
			  virtual ~connectionListener();
			  void run();
			private:
			  int port;
			  byte* statuspointer;
		};

	private:
		static const int JOY_UP      = 0x01;
		static const int JOY_DOWN    = 0x02;
		static const int JOY_LEFT    = 0x04;
		static const int JOY_RIGHT   = 0x08;
		static const int JOY_BUTTONA = 0x10;
		static const int JOY_BUTTONB = 0x20;

		std::string name;

		int joyNum;
		byte status;
		//For IP connection
		bool IPwritable;
		std::string hostname;
		int portname;
		int listenport;

		int sockfd;
		struct sockaddr_in servaddr;

		void setupConnections();
		void destroyConnections();
		void setupListener();
		void destroyListener();
		void setupWriter();
		void destroyWriter();
		void sendByte(byte value);
		connectionListener* myListener;
		Thread* thread;
};
#endif
