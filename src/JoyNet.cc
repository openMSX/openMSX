// $Id$

#include <cassert>
#include "JoyNet.hh"
#include "PluggingController.hh"
#include "MSXConfig.hh"
//#include "EventDistributor.hh"


JoyNet::JoyNet(int joyNum)
{
	PRT_DEBUG("Creating a JoyNet object for joystick " << joyNum);


	//throw JoyNetException("No such joystick number");

	name = std::string("joynet")+(char)('1'+joyNum);

	PluggingController::instance()->registerPluggable(this);
	IPwritable=false;
	setupConnections();
}

JoyNet::~JoyNet()
{
	PluggingController::instance()->unregisterPluggable(this);
}

//Pluggable
const std::string &JoyNet::getName()
{
	return name;
}


//JoystickDevice
byte JoyNet::read(const EmuTime &time)
{
	return status;
}

void JoyNet::write(byte value, const EmuTime &time)
{
	sendByte(value); //do nothing
	PRT_DEBUG("TCP/IP sending byte "<<(int)value<<" on time "<<time );
}

void JoyNet::setupConnections()
{
	try {
	MSXConfig::Config *config = MSXConfig::Backend::instance()->getConfigById(name);
	hostname = config->getParameter("connecthost");
	portname = atoi( (config->getParameter("connectport")).c_str() );
	listenport = atoi( (config->getParameter("listenport")).c_str() );
	// setup  tcp stream with second (master) msx and listerenr for third (slave) msx
	
	//first listener in case the connect wants to talk to it's own listener
	if (config->getParameterAsBool("startlisten") ){
		setupListener();
	};

	/*
	 * Currently done when first write is tried
	 */
	//if (config->getParameterAsBool("startconnect") ){
	//	sleep(1); //give listener-thread some time to initialize itself
	//	setupWriter();
	//};
	
	} catch (MSXException& e) {
	PRT_DEBUG("No correct connection configuration for " << name << ":" << e.desc );
	}
}


void JoyNet::setupListener()
{
	myListener=new connectionListener(listenport,&status);
	// Start a new thread for IP listening
	thread=new Thread(myListener);
	thread->start();
}

void JoyNet::setupWriter()
{
	sockfd=0;
	if ( (sockfd= socket(AF_INET, SOCK_STREAM, 0)) <0){
		PRT_INFO( "JoyNet: socket error in connection setup\n");
		sockfd=0;
	}else {
	  memset(&servaddr,0, sizeof(servaddr));
	  servaddr.sin_family = AF_INET;
	  servaddr.sin_port = htons(portname);

	  if (inet_pton(AF_INET, hostname.c_str() , &servaddr.sin_addr) <= 0){
	  	PRT_INFO( "JoyNet: inet_pton error for " << hostname );
		sockfd=0;
	  } else {
	    if (connect(sockfd,(struct sockaddr*)&servaddr,sizeof(servaddr)) < 0){
		PRT_INFO( "JoyNet: connect error");
		sockfd=0;
	    } else IPwritable=true;

	  }
	}
}

void JoyNet::destroyConnections()
{
	// destroy connection (and thread?)
	destroyWriter();
	destroyListener();
}

void JoyNet::destroyListener()
{
	if (thread) thread->stop();
}

void JoyNet::destroyWriter()
{
	IPwritable=false;
	close(sockfd);
}

void JoyNet::sendByte(byte value)
{
	// No transformation of bits to be directly read into openMSX later on
	// needed since it is a one-on-one mapping
	
	if (!IPwritable) setupWriter();

	//TODO: make write non-blocking!!!
	if (sockfd) ::write(sockfd,&value,1);

	/* Joynet cable looped for Maartens test program
	status=value;
	*/
}

JoyNet::connectionListener::connectionListener(int listenport,byte *linestatus)
{
	port=listenport;
	statuspointer=linestatus;
}

JoyNet::connectionListener::~connectionListener()
{
}

void JoyNet::connectionListener::run()
{	
	byte buf;
	int listenfd;
	int connectfd;
	struct sockaddr_in servaddr;

	/*
	 * Build a socket -> bind -> listen
	 */

	if ( (listenfd= socket(AF_INET, SOCK_STREAM, 0)) <0)
	{
	  PRT_INFO("Socket error");
	  return;
	};

	{
	  int opt = 1;
	  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*) &opt, sizeof(opt)) == -1)
	  {
	    PRT_INFO("TCP/IP Problems SO_REUSEADDR");
	  }
	}

	memset(&servaddr,0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	PRT_INFO("TCP/IP Trying to listen on port " << port);
	servaddr.sin_port = htons(port);

	if (bind(listenfd, (struct sockaddr*)&servaddr,sizeof(servaddr)) < 0){
	  PRT_INFO("TCP/IP Bind error");
	  return;
	};

	if (listen(listenfd,1024 ) < 0){
	  PRT_INFO("TCP/IP Listen error");
	  return;
	};
	if ( (connectfd = accept(listenfd,(struct sockaddr*)NULL,NULL)) <0 ){
	  PRT_INFO("TCP/IP accept error");
	  return;
	}
	//Accept only one connection!
	close(listenfd);
	
	//TODO: check if read is blocking.(preferable)
	int charcounter;
	while ( (charcounter=::read(connectfd,&buf,1)) > 0) {
	  PRT_DEBUG("got from TCP/IP code " << std::hex << (int)buf << std::dec );
	  *statuspointer=buf;
	};
	if (charcounter<0){
	  PRT_INFO("TCP/IP read error ");
	}

}
