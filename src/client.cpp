#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>  // for addrinfo stuff
#include <sys/select.h>

#include "client.hpp"
#include "tracker-response.hpp"
#include "meta-info.hpp"
#include "common.hpp"
#include "msg/handshake.hpp"
#include "msg/msg-base.hpp"
#include "util/buffer-stream.hpp"
#include "util/bencoding.hpp"
#include "util/hash.hpp"
#include "util/buffer.hpp"
#include "http/url-encoding.hpp"
#include "http/http-request.hpp"
#include "http/http-response.hpp"

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>


using namespace sbt;
using namespace msg;


Client::Client(const std::string& port1, const std::string& torrent)
{
    this->port = port1;
    this->torrent = torrent; 
    //std::ifstreams (argv[2], std::ifstream::in);
	std::ifstream ifs(torrent, std::ifstream::in);
	torrentInfo.wireDecode(ifs);
	encodedPeer = "SIMPLEBT.TEST.PEERID";
	
	setupTrackerRequest();
    sendTrackerRequest();
    lastCheck = time(0);
    
	fd_set tmpFds;
	FD_ZERO(&tmpFds);
	fd_set readFds;
//	fd_set tmpFds; //moved up to pass as parameter to setupPeerListener
	FD_ZERO(&readFds);
	
	maxSockfd = 0;
	
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // allowed because we close old sockets
	maxSockfd = sockfd;

	// put the socket in the socket set
	FD_SET(sockfd, &readFds);

	// allow others to reuse the address
	int yes = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
	{
		perror("setsockopt");
		//return 1;
	}	

	// bind to socket
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(port.c_str()));     //TODO set this to argv
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));

	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) 
	{
		perror("bind");
		// 2;
	}

	// set the socket in listen status
	if (listen(sockfd, 10) == -1) 
	{
		perror("listen");
		//return 3;
	}
	
	listenerFD = sockfd;
    
	//clientHandShake.setInfoHash(mi.getHash());
	clientHandShake.setInfoHash(torrentInfo.getHash()); // tommy
	clientHandShake.setPeerId("SIMPLEBT.TEST.PEERID");
	// initialize timer
	struct timeval tv;
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	
	
	
	
//	FD_ZERO(&tmpFds);
	//int maxSockfd = 0;
//	maxSockfd = 0;
	
	while(true)
	{
		
		// set up watcher
		if (select(maxSockfd + 1, &readFds, NULL, NULL, &tv) == -1) 
		{
			perror("select"); std::cout << "select" << std::endl;
			// return 4;
			exit(4); // no return from constructor?
		}
		
		for(int fd = 0; fd <= maxSockfd; fd++)
		{
			std::cout << " fd: " << fd << " maxSockfd: " << maxSockfd << std::endl;
			if(shouldUpdateTracker())
				sendTrackerRequest();
		    if(FD_ISSET(fd, &readFds))
		    {
		        if(fd == listenerFD)
		        {
		        	std::cout << "adding fd captured by listener: " << std::endl;
		            int childFD = addPeer();
		            FD_SET(childFD, &tmpFds);
		            if(childFD > maxSockfd)
		                maxSockfd = childFD;
		            std::cout << "added fd captured by listener: " << childFD << std::endl;
		        }
		        else if(isFDPeer(fd))
		        {
		        	std::cout << "looping through fd: " << fd << std::endl;
		            if (getFDStatus(fd) == 0 || getFDStatus(fd) == 2) //TWO STATES EXPECTING A HANDSHAKE
					{
						std::cout << "About to recieve handshake" << std::endl;
					    HandShake hs = receiveHandShake(fd);
					    std::cout << "just received handshake" << std::endl;
					    //TODO VERIFY THIS
					    if(getFDStatus(fd) == 0)
					    {
					       // sendBitfield();
					        setFDStatus(fd, 1);
					    }
					    if(getFDStatus(fd) == 2)
					    {
					    	std::cout << "sending handshake to : " << fd << std::endl;
					        sendHandShake(fd);
					        setFDStatus(fd, 3);
					    }
					}
					else if(getFDStatus(fd) >= 3) //expecting a message
					{
						std::cout << "FD Status == 3: Expecting Message" << std::endl;
					    //MsgBase mb = receiveMessage(fd);
					    MsgBase* mb	= receiveMessage(fd);
						if(mb == NULL)
							exit(1); // not supposed to be null
					    switch (mb->getId())
						{
							case MSG_ID_UNCHOKE: 
							{		// 1
								if(socketStatus[fd] == 8)
									socketStatus[fd] = 9;
									
								if(socketStatus[fd] == 9)
									socketStatus[fd] = 10;
								break;
							} 
							case MSG_ID_INTERESTED:	
							{	// 2
								if(socketStatus[fd] == 5)
									socketStatus[fd] = 8;
									
								if(socketStatus[fd] == 6)
									socketStatus[fd] = 9;
									
								if(socketStatus[fd] == 7)
									socketStatus[fd] = 10;
								break;
							}
						    //TODO add other cases;
						}
					}
					
		        }
		        else
		        {
		        	std::cout<<"Why did i get here?" << std::endl;
		        }
		    }
		    else
		    {
		    	std::cout<<"Why did i get here? #2" << std::endl;
		    }
			
		} //end of fd loop
		
	} //end of while true loop
	
}

HandShake Client::receiveHandShake(int fd)
{
    	char buf[68] = {0};
    	HandShake handS;

		if (recv(fd, buf, 68, 0) == -1) 
		{
			perror("recv handshake");
			//return 6;
			return handS; // uninitialized
		}
	
	
		// cast char* buffer to ConstBuffPtr using make_share or OBufferStream
		OBufferStream obuf;
		obuf.put(0);
		obuf.write(buf, 67);
		shared_ptr<Buffer> bufNew = obuf.buf(); // obuf.get()?

		std::cout << "buf: " << buf << " buf size :" << bufNew->size() << std::endl;
		std::cout << "bufnew: " << bufNew << std::endl;

		// message be a handshake
		std::cout << "received handshake" << std::endl;
		// create an empty Handshake object

    	// use handshake object's decode which takes a CBP
    	handS.decode(bufNew);
    	return handS;
}

void Client::sendHandShake(int fd)
{
    //ConstBufferPtr hj = handjob.encode();
	ConstBufferPtr hj = clientHandShake.encode(); // tommy
    const char* hjc = reinterpret_cast<const char*>(hj->buf());
    std::cout << "sending handshake back, socket status is now 3" << std::endl;
	//SEEND HANDSHAKE BACK
	if (send(fd, hjc, 68, 0) == -1) 
	{
		perror("send");
		return;
	}
}

// void Client::receiveMessage(int fd)
MsgBase* Client::receiveMessage(int fd)
{
	char buf[5] = {0};
	int status = 0;
	if ((status =recv(fd, buf, 5, 0)) == -1) 
	{
		perror("recv");
		return NULL;
	}
	
	std::cout << "recv size = " << status << std::endl;
	std::cout << "buf: " << buf;
	
	std::cout << "socket status = " << socketStatus[fd] << std::endl;
	std::cout << "STRLEN " << strlen(buf) << std::endl;
	char pleadTheFifth = buf[4];
	uint8_t typeId = (uint8_t)pleadTheFifth;
	
	std::cout << "msg type: " << typeId << std::endl;
	
	// ??????
	// are we receiving messages correctly? 
	// based on call to receiveMessage(fd) in line 111, 
	// this function needs to return a MsgBase
	// MsgBase is abstract, so the best that can be returned
	// is a generic pointer.
	
	MsgBase* mb;
	if(typeId == MSG_ID_UNCHOKE)
		mb = new Unchoke;
	else if(typeId == MSG_ID_INTERESTED)
		mb = new Interested;
	else if(typeId == MSG_ID_HAVE)
		mb = new Have;
	else if(typeId == MSG_ID_BITFIELD)
		mb = new Bitfield;
	else if(typeId == MSG_ID_REQUEST)
		mb = new Request;
	else if(typeId == MSG_ID_PIECE)
		mb = new Piece;
	else
		return NULL;
		
	OBufferStream obuf;
	obuf.write(buf, 5);
	
	ConstBufferPtr cnstBufPtr = obuf.buf();
	if(mb != NULL)
		mb->decode(cnstBufPtr); 
	
	return mb;
		
}
void Client::setupTrackerRequest()
{

	ConstBufferPtr cbp = torrentInfo.getHash();
	std::vector<uint8_t> v = *cbp;
	std::string encodedHash = url::encode(cbp->get(), v.size());

	std::size_t pos = torrentInfo.getAnnounce().find("//");
	std::string host = torrentInfo.getAnnounce().substr(pos+2);
	std::string port = host.substr(host.find(":")+1);
	std::string location = port;
	location = location.substr(port.find("/"));
	port = port.substr(0, port.find("/"));
	pos = host.find(":");
	host = host.substr(0, pos);

	trRequest.setHost(host);
	trRequest.setPort(std::stoi(port));
	trRequest.setMethod(HttpRequest::GET);
	trRequest.setPath(location+"?info_hash=" + encodedHash+"&peer_id="+encodedPeer+"&port="+port+"&uploaded=0&downloaded=0&left=46822&event=started"); //this needs to be the query
	trRequest.setVersion("1.0");
	trRequest.addHeader("Accept-Language", "en-US");
}

void Client::sendTrackerRequest()
{
    std::size_t reqLen = trRequest.getTotalLength();
	char* buf = new char[reqLen];
	trRequest.formatRequest(buf);

	//resolve host name to IP
	struct addrinfo hints;
	struct addrinfo* res;


	// prepare hints
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // IPv4
	hints.ai_socktype = SOCK_STREAM;

	// get address
	int status = 0;
	//if ((status = getaddrinfo(trRequest.getHost().c_str(), trRequest.getPort().c_str(), &hints, &res)) != 0) 
	/*
		TODO:
		HttpRequest::getPort() returns an unsigned short. 
		Can't convert to c string with this function.
		This argument will be left as NULL to aid compilation and shall be addressed later
	*/
	int thePort = 0;
	thePort += trRequest.getPort();
	char strPort[6];
	
	snprintf(strPort, sizeof(strPort), "%d", thePort);
	if ((status = getaddrinfo(trRequest.getHost().c_str(), (const char*)&strPort , &hints, &res)) != 0) 
	{
		std::cerr << "getaddrinfo: " << gai_strerror(status) << std::endl;
		return;
	}
	//std::cout << "resolved addrinfo" << std::endl;

	struct addrinfo* p = res;
	if(p==0)
		return;
	// convert address to IPv4 address
	struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
	
	// convert the IP to a string and print it:
	char ipstr[INET_ADDRSTRLEN] = {'\0'};
	inet_ntop(p->ai_family, &(ipv4->sin_addr), ipstr, sizeof(ipstr));
	freeaddrinfo(res); // free the linked list
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);          
	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(thePort);     // short, network byte order
	serverAddr.sin_addr.s_addr = inet_addr(ipstr);
	// connect to the server
	if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
		perror("connect");
		return;// 2;
	}
	//std::cout << "connected -<3 Yingdi" << std::endl;
	struct sockaddr_in clientAddr;

	socklen_t clientAddrLen = sizeof(clientAddr);
	if (getsockname(sockfd, (struct sockaddr *)&clientAddr, &clientAddrLen) == -1) 
	{
		perror("getsockname");
		return;// 3;
	}

	char ipstr1[INET_ADDRSTRLEN] = {'\0'};
	inet_ntop(clientAddr.sin_family, &clientAddr.sin_addr, ipstr1, sizeof(ipstr1));

	std::string input;
	char buf1[BUFSIZE] = {0};
	std::stringstream ss;

	//memset(buf1, '\0', sizeof(buf1)); // tommy

	//if (send(sockfd, buf, req.getTotalLength(), 0) == -1)
	if(send(sockfd, buf, reqLen, 0) == -1)
	{
		perror("send");
		return;// 4;
	}

	if (recv(sockfd, buf1, BUFSIZE, 0) == -1) 
	{
		perror("recv");
		return;// 5;
	}
	close(sockfd);

	HttpResponse hres;
	hres.parseResponse(buf1, BUFSIZE);
	char* body = strstr(buf1, "\r\n\r\n")+4;
	std::string bodys = body;
	std::istringstream iss(bodys);
	sbt::bencoding::Dictionary theD;
	theD.wireDecode(iss);
	TrackerResponse tr;
	tr.decode(theD);
	trackerPeers = tr.getPeers();
	trInterval = tr.getInterval();
	
	lastCheck = time(0);
	//free [] buf; // ???? Tommy
}

bool Client::shouldUpdateTracker()
{
    if(((int)time(0) - (int)lastCheck ) > trInterval) 
	    return true;
    return false;
}

int Client::setupPeerListener(fd_set& tmpFds)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // allowed because we close old sockets
	maxSockfd = sockfd;

	// put the socket in the socket set
	FD_SET(sockfd, &tmpFds);

	// allow others to reuse the address
	int yes = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
	{
		perror("setsockopt");
		return 1;
	}	

	// bind to socket
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(port.c_str()));     //TODO set this to argv
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));

	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) 
	{
		perror("bind");
		return 2;
	}

	// set the socket in listen status
	if (listen(sockfd, 10) == -1) 
	{
		perror("listen");
		return 3;
	}
	
	listenerFD = sockfd;
	return sockfd;
}

int Client::addPeer()
{
    struct sockaddr_in clientAddr;
	socklen_t clientAddrSize;
	std::cout << "Trying to accept a connection" << std::endl;
	int clientSockfd = accept(listenerFD, (struct sockaddr*)&clientAddr, &clientAddrSize);
	if (clientSockfd == -1) 
	{
		perror("accept");
		return -1;
	}

	char ipstr[INET_ADDRSTRLEN] = {'\0'};
	inet_ntop(clientAddr.sin_family, &clientAddr.sin_addr, ipstr, sizeof(ipstr));
	std::cout << "Accept a connection from: " << ipstr << ":" << ntohs(clientAddr.sin_port) << std::endl;
	
	// update maxSockfd
	if (maxSockfd < clientSockfd)
		maxSockfd = clientSockfd;

	PeerInfo thePeerInfo;
	thePeerInfo.ip = ipstr;
	thePeerInfo.port = ntohs(clientAddr.sin_port);
	//TODO Prevent duplicates

	setFDStatus(clientSockfd, 2); //client has connected
	
	return clientSockfd;
}

bool Client::isFDPeer(int fd)
{
	if(socketStatus.find(fd) != socketStatus.end())
		return true;
	else
		return false;
	
}

int Client::getFDStatus(int fd)
{
	if(socketStatus.find(fd) != socketStatus.end())
		return socketStatus[fd];
	else
		return -1;
}

void Client::setFDStatus(int fd, int status)
{
		socketStatus[fd] = status;
}

/*
int Client::getFDofPeer(PeerID pi, std::vector<PeerFD>& pfd)
{
	for(int i=0; i < pfd.size(); i++)
	{
		if(pfd[i].pi.ip.compare(pi.ip) == 0 && pfd[i].pi.port==pi.port)
			return pfd[i].fd;
	}
	return -1; //if not found, return -1
}
*/