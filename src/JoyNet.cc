// $Id$

#include "JoyNet.hh"
#include "PluggingController.hh"
#include "MSXConfig.hh"


JoyNet::JoyNet()
{
	sockfd = 0;
	listener = 0;
	try {
		setupConnections();
		PluggingController::instance()->registerPluggable(this);
	} catch (MSXException &e) {
		PRT_DEBUG("No correct JoyNet configuration");
	}
}

JoyNet::~JoyNet()
{
	PluggingController::instance()->unregisterPluggable(this);
	
	// destroy writer
	if (sockfd) close(sockfd);
	// destroy listener
	delete listener;
}

//Pluggable
const std::string &JoyNet::getName()
{
	static const std::string name("joynet");
	return name;
}


//JoystickDevice
byte JoyNet::read(const EmuTime &time)
{
	return status;
}

void JoyNet::write(byte value, const EmuTime &time)
{
	sendByte(value);	//do nothing
	PRT_DEBUG("TCP/IP sending byte "<<(int)value<<" on time "<<time);
}


void JoyNet::setupConnections()
{
	MSXConfig::Config *config = MSXConfig::Backend::instance()->getConfigById("joynet");
	hostname = config->getParameter("connecthost");
	portname = config->getParameterAsInt("connectport");
	int listenport = config->getParameterAsInt("listenport");
	// setup  tcp stream with second (master) msx and listerenr for third (slave) msx

	//first listener in case the connect wants to talk to it's own listener
	if (config->getParameterAsBool("startlisten"))
		listener = new ConnectionListener(listenport, &status);

	// Currently done when first write is tried
	/*if (config->getParameterAsBool("startconnect")) {
		sleep(1);	//give listener-thread some time to initialize itself
		setupWriter();
	}*/
}


void JoyNet::setupWriter()
{
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		PRT_INFO("JoyNet: socket error in connection setup");
		sockfd = 0;
	} else {
		memset(&servaddr, 0, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(portname);

		if (inet_pton(AF_INET, hostname.c_str(), &servaddr.sin_addr) <= 0) {
			PRT_INFO("JoyNet: inet_pton error for " << hostname);
			sockfd = 0;
		} else {
			if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
				PRT_INFO("JoyNet: connect error");
				sockfd = 0;
			}
		}
	}
}

void JoyNet::sendByte(byte value)
{
	// No transformation of bits to be directly read into openMSX later on
	// needed since it is a one-on-one mapping

	if (!sockfd) setupWriter();
	if (sockfd) ::write(sockfd, &value, 1);	//TODO non-blocking

	/* Joynet cable looped for Maartens test program
	   status=value;
	 */
}

JoyNet::ConnectionListener::ConnectionListener(int listenport, byte* linestatus) :
	thread(this)
{
	port = listenport;
	statuspointer = linestatus;
	thread.start();
}

JoyNet::ConnectionListener::~ConnectionListener()
{
}

void JoyNet::ConnectionListener::run()
{
	int listenfd;
	int connectfd;
	struct sockaddr_in servaddr;

	// Build a socket -> bind -> listen

	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		PRT_INFO("Socket error");
		return;
	}

	int opt = 1;
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == -1)
		PRT_INFO("TCP/IP Problems SO_REUSEADDR");

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	PRT_INFO("TCP/IP Trying to listen on port " << port);
	servaddr.sin_port = htons(port);

	if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		PRT_INFO("TCP/IP Bind error");
		return;
	}

	if (listen(listenfd, 1024) < 0) {
		PRT_INFO("TCP/IP Listen error");
		return;
	}
	if ((connectfd = accept(listenfd, (struct sockaddr*)NULL, NULL)) < 0) {
		PRT_INFO("TCP/IP accept error");
		return;
	}
	//Accept only one connection!
	close(listenfd);

	int charcounter;
	while ((charcounter = ::read(connectfd, &statuspointer, 1)) > 0) {
		PRT_DEBUG("got from TCP/IP code " << std::hex << (int)*statuspointer << std::dec);
	}
	if (charcounter < 0) {
		PRT_INFO("TCP/IP read error ");
	}
}
