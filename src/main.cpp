#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <stdio.h>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <climits>
#include <map>
#include <ctime>

#include "client.hpp"
#include "meta-info.hpp"
#include "http/http-request.hpp"
#include "http/url-encoding.hpp"
#include "util/bencoding.hpp"
#include "util/buffer-stream.hpp"
#include "msg/handshake.hpp"
#include "msg/msg-base.hpp"
#include "http/http-response.hpp"
#include "tracker-response.hpp"
#include "common.hpp"

using namespace sbt;
using namespace msg;

#define PEERLEN 20
#define HANDSHAKELEN 68 
#define BUFSIZE 20000

struct cmpPeer {
    bool operator()(const PeerInfo& a, const PeerInfo& b) const {
        int str = a.ip.compare(b.ip);
        if(str < 0)
        	return true;
        else if (str > 0)
        	return false;
        else if(a.port < b.port)
        {
        	return true;
        }
        else
        	return false;
    }
};


// returns an int of next piece to grab
int checkFilePieces(MetaInfo& metaInfo, int fd) 
{
	int fileSize = metaInfo.getLength();
	int pieceLength = metaInfo.getPieceLength();
	int roundUp = (fileSize % pieceLength); // decide if need division truncated piece
	int numPieces = fileSize / pieceLength + roundUp ? 1 : 0;
	
	
}

int checkTracker(TrackerResponse& trackRes, HttpRequest& seqReq, struct sockaddr_in& serverAddr)
{
	std::size_t reqLen = seqReq.getTotalLength();
	char* buf = new char[reqLen];
	seqReq.formatRequest(buf);

	int secSockfd = socket(AF_INET, SOCK_STREAM, 0);

	// connect
	if(connect(secSockfd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) == -1) 
	{
		perror("connect");
		return 2;
	}

	struct sockaddr_in clientAddr;
	socklen_t clientAddrLen = sizeof(clientAddr);
	// get socket name
	if(getsockname(secSockfd, (struct sockaddr *) &clientAddr, &clientAddrLen) == -1) 
	{
		perror("getsockname");
		return 3;
	}

	// send request
	if(send(secSockfd, buf, seqReq.getTotalLength(), 0) == -1) {
		perror("send");
		return 4;
	} 

	// receive response
	char secReqRecBuf[BUFSIZE];
	memset(secReqRecBuf, '\0', sizeof(secReqRecBuf));
	if(recv(secSockfd, secReqRecBuf, BUFSIZE, 0) == -1) {
		perror("receive");
		return 5;
	}

	close(secSockfd);

	// decipher data from response
	HttpResponse secHRes;
	if(secReqRecBuf[0] != 0) 
		secHRes.parseResponse(secReqRecBuf, BUFSIZE);
	char *body = strstr(secReqRecBuf, "\r\n\r\n")+4;
	std::string bodys = body;
	std::istringstream iss2(bodys);
	sbt::bencoding::Dictionary elD;
	elD.wireDecode(iss2);
	TrackerResponse tr2;
	tr2.decode(elD);

	trackRes = tr2;    
	return 0;
	// sleep interval specified by tracker response
	// sleep(tr2.getInterval());
}

int
main(int argc, char** argv)
{

	// Check command line arguments.
	if (argc != 3)
	{
		std::cerr << "Usage: simple-bt <port> <torrent_file>\n";
		return 1;
	}	 

	std::map<int, int> socketStatus;
	std::map<PeerInfo, int, cmpPeer> peerToFD;
	//PART 1: TALKING TO THE TRACKER TO INITIALIZE

	std::ifstream ifs (argv[2], std::ifstream::in);

	MetaInfo mi;
	mi.wireDecode(ifs);
	ConstBufferPtr cbp = mi.getHash();
	std::vector<uint8_t> v = *cbp;

	std::vector<uint8_t> vp;
	uint8_t it = 0;
	while (it < PEERLEN) {
		vp.push_back(it++);
	}

	std::string encodedHash = url::encode(cbp->get(), v.size());
	std::string encodedPeer = "SIMPLEBT.TEST.PEERID";

	std::size_t pos = mi.getAnnounce().find("//");
	std::string host = mi.getAnnounce().substr(pos+2);
	std::string port = host.substr(host.find(":")+1);
	std::string location = port;
	location = location.substr(port.find("/"));
	port = port.substr(0, port.find("/"));
	pos = host.find(":");
	host = host.substr(0, pos);

	////	///


	HttpRequest req;
	req.setHost(host);
	req.setPort(std::stoi(port));
	req.setMethod(HttpRequest::GET);
	req.setPath(location+"?info_hash=" + encodedHash+"&peer_id="+encodedPeer+"&port="+argv[1]+"&uploaded=0&downloaded=0&left=300&event=started"); //this needs to be the query
	req.setVersion("1.0");
	req.addHeader("Accept-Language", "en-US");

	std::size_t reqLen = req.getTotalLength();
	char* buf = new char[reqLen];
	req.formatRequest(buf);


	//resolve host name to IP
	struct addrinfo hints;
	struct addrinfo* res;


	// prepare hints
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // IPv4
	hints.ai_socktype = SOCK_STREAM;

	// get address
	int status = 0;
	if ((status = getaddrinfo(host.c_str(), port.c_str(), &hints, &res)) != 0) {
		std::cerr << "getaddrinfo: " << gai_strerror(status) << std::endl;
		return 2;
	}


	struct addrinfo* p = res;
	if(p==0)
		return 2;
	// convert address to IPv4 address
	struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
	
	// convert the IP to a string and print it:
	char ipstr[INET_ADDRSTRLEN] = {'\0'};
	inet_ntop(p->ai_family, &(ipv4->sin_addr), ipstr, sizeof(ipstr));

	freeaddrinfo(res); // free the linked list
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);          
	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(std::stoi(port));     // short, network byte order
	serverAddr.sin_addr.s_addr = inet_addr(ipstr);
	// connect to the server
	if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
		perror("connect");
		return 2;
	}

	struct sockaddr_in clientAddr;

	socklen_t clientAddrLen = sizeof(clientAddr);
	if (getsockname(sockfd, (struct sockaddr *)&clientAddr, &clientAddrLen) == -1) {
		perror("getsockname");
		return 3;
	}

	char ipstr1[INET_ADDRSTRLEN] = {'\0'};
	inet_ntop(clientAddr.sin_family, &clientAddr.sin_addr, ipstr1, sizeof(ipstr1));

	std::string input;
	char buf1[BUFSIZE] = {0};
	std::stringstream ss;

	memset(buf1, '\0', sizeof(buf1));

	if (send(sockfd, buf, req.getTotalLength(), 0) == -1) {
		perror("send");
		return 4;
	}

	if (recv(sockfd, buf1, BUFSIZE, 0) == -1) {
		perror("recv");
		return 5;
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
	std::vector<PeerInfo> pi = tr.getPeers();

	PeerInfo myself; //add yourself to peertofd so you can't connect yourself
	myself.ip = i.ip;
	myself.port = atoi(argv[1]);
	
	peerToFD[myself] = -1; //don't check
	
	for (PeerInfo i : pi)
	{
	
		if(peerToFD.find(i) != peerToFD.end()) //the peer (or yourself) is already in the list
		{
			continue;
		}
		
		int peerfd = socket(AF_INET, SOCK_STREAM, 0);          
		struct sockaddr_in peerAddr;
		peerAddr.sin_family = AF_INET;
		peerAddr.sin_port = htons(i.port);     // short, network byte order
		peerAddr.sin_addr.s_addr = inet_addr(i.ip.c_str());
		// connect to the server
		if (connect(peerfd, (struct sockaddr *)&peerAddr, sizeof(peerAddr)) == -1) {
			perror("connect");
			return 2;
		}
		
			// make a handshake to send
		std::string eyeD = "SIMPLEBT.TEST.PEERID";
		HandShake handjob(mi.getHash(), eyeD); // m_encodedHash corresponds to encodedHash in main.cpp
		ConstBufferPtr hjBufPtr = handjob.encode();
		const char* hjc = reinterpret_cast<const char*>(hjBufPtr->buf());
		
		if(send(peerfd, hjc, 68, 0) < 0)
		{
			perror("send handshake");
			return 8;
		}
		
		peerToFD[i] = peerfd;
		socketStatus[peerfd] = 0;

		
	} 
	//Eventually incorperate
	


//START PART 2 CODE: P2P service 



	int maxSockfd = 0;

	fd_set readFds;
	fd_set tmpFds;
	FD_ZERO(&readFds);
	FD_ZERO(&tmpFds);
	
	// create a socket using TCP IP
	sockfd = socket(AF_INET, SOCK_STREAM, 0); // allowed because we close old sockets
	maxSockfd = sockfd;

	// put the socket in the socket set
	FD_SET(sockfd, &tmpFds);

	// allow others to reuse the address
	int yes = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		return 1;
	}	

	// bind to socket
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[1]));     //TODO set this to argv
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));

	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("bind");
		return 2;
	}

	// set the socket in listen status
	if (listen(sockfd, 10) == -1) {
		perror("listen");
		return 3;
	}
	
	//TODO: check the file we are downloading to see what pieces we have and need
	// use map of <int: piece number, string piece NULL if no piece>

	
	
	
	
	// initialize timer
	struct timeval tv;
	tv.tv_sec = 10;
	tv.tv_usec = 0;


	// get first interval time
	int trInterval = tr.getInterval();

	// get current time
	time_t t = time(0); // try casting to an int



	while (true) 
	{
		readFds = tmpFds;
		if(((int)time(0) - (int)t ) > trInterval) 
		{
			checkTracker(tr, req, serverAddr);
			trInterval = tr.getInterval();
			t = time(0);
		}

		// set up watcher
		if (select(maxSockfd + 1, &readFds, NULL, NULL, &tv) == -1) {
			perror("select");
			return 4;
		}

		for(int fd = 0; fd <= maxSockfd; fd++) 
		{
			//TODO: CHECK TRACKER TIMING INTERVAL
			// if time difference is greater than interval, run checkTracker and reset vals
			if(((int)time(0) - (int)t ) > trInterval) 
			{
				checkTracker(tr, req, serverAddr);
				trInterval = tr.getInterval();
				t = time(0);
			}

			// get one socket for reading
			if (FD_ISSET(fd, &readFds)) 
			{
				if (fd == sockfd) 
				{ // this is the listen socket
					struct sockaddr_in clientAddr;
					socklen_t clientAddrSize;
					int clientSockfd = accept(fd, (struct sockaddr*)&clientAddr, &clientAddrSize);
	
			
					

					if (clientSockfd == -1) 
					{
						perror("accept");
						return 5;
					}

					char ipstr[INET_ADDRSTRLEN] = {'\0'};
					inet_ntop(clientAddr.sin_family, &clientAddr.sin_addr, ipstr, sizeof(ipstr));
					std::cout << "Accept a connection from: " << ipstr << ":" << ntohs(clientAddr.sin_port) << std::endl;
					
					// update maxSockfd
					if (maxSockfd < clientSockfd)
						maxSockfd = clientSockfd;


					socketStatus[clientSockfd] = 2; //client has connected
					PeerInfo thePeerInfo;
					thePeerInfo.ip = ipstr;
					thePeerInfo.port = ntohs(clientAddr.sin_port);
					if(peerToFD[thePeerInfo] != peerToFD.end())
					{
						//client already connected
						close(clientSockfd);
						std::cout << "already connected so not allowign a second connection" << std::endl;
						continue;
					}
					// add the socket into the socket set
					FD_SET(clientSockfd, &tmpFds);
				} 
				else 
				{ // this is the normal socket and normal requests

					//FULLFILL WHATEVER REQUEST YOU RECEIVE
					//IF we have their bitfield, see if they have a file we want and keep track of that
					//-----If file is done, send completed event to tracker

					char buf[68];
					std::stringstream ss;

					memset(buf, '\0', sizeof(buf));
					if (recv(fd, buf, 68, 0) == -1) 
					{
						perror("recv");
						return 6;
					}
					//ss << buf << std::endl;

					// cast char* buffer to ConstBuffPtr using make_share or OBufferStream
					OBufferStream obuf;
					obuf.put(0);
					obuf.write(buf, 67);
					shared_ptr<Buffer> bufNew = obuf.buf(); // obuf.get()?

					std::cout << "buf: " << buf << " buf size :" << bufNew->size() << std::endl;
					std::cout << "bufnew: " << bufNew << std::endl;

					// check to see if message is a handshake
					if (socketStatus[fd] == 0 || socketStatus[fd] == 2) //TWO STATES EXPECTING A HANDSHAKE
					{
						// message be a handshake
						std::cout << "received handshake" << std::endl;
						// create an empty Handshake object
						HandShake handS;
						// use handshake object's decode which takes a CBP
						handS.decode(bufNew);
					
						HandShake handjob;
						handjob.setInfoHash(mi.getHash());
						handjob.setPeerId("SIMPLEBT.TEST.PEERID");
						ConstBufferPtr hj = handjob.encode();
						const char* hjc = reinterpret_cast<const char*>(hj->buf());
						
						if(socketStatus[fd] == 0)
						{
							socketStatus[fd] = 1; //sending bitfield
							//TODO SEND BITFIELD
						}
						else if (socketStatus[fd] ==2)
						{
							//SEEND HANDSHAKE BACK
							if (send(fd, hjc, HANDSHAKELEN, 0) == -1) 
							{
								perror("send");
								return 8;
							}
							socketStatus[fd] = 3; //sent them handshake back
						}
					}
					else if (socketStatus[fd] >= 4)
					{
						if (sizeof(buf) >= 5) {
							
							char pleadTheFifth = *(buf+4);
							uint8_t typeId = (uint8_t)pleadTheFifth;
							
							std::cout << "msg type: " << typeId << std::endl;
							
							switch (typeId)
							{
								case MSG_ID_CHOKE:			// 0
									// do nothing
									break;
								case MSG_ID_UNCHOKE:		// 1
									if(socketStatus[fd] == 8)
										socketStatus[fd] = 9;
										
									if(socketStatus[fd] == 9)
										socketStatus[fd] = 10;
									break;
								case MSG_ID_INTERESTED:		// 2
									if(socketStatus[fd] == 5)
										socketStatus[fd] = 8;
										
									if(socketStatus[fd] == 6)
										socketStatus[fd] = 9;
										
									if(socketStatus[fd] == 7)
										socketStatus[fd] = 10;
									break;
								case MSG_ID_NOT_INTERESTED:	// 3
									break;
								case MSG_ID_HAVE:			// 4
									break;
								case MSG_ID_BITFIELD:		// 5
									Bitfield peerField;
									peerField.decode(bufNew);
									const int* theirBitfield = reinterpret_cast<const int*>(bfield->buf());
									
									// do stuff with their bitfield
									
									// create our own bitfield
									int fileSize = mi.getLength();
									int pieceLength = mi.getPieceLength();
									int roundUp = (fileSize % pieceLength); // decide if need division truncated piece
									int n = fileSize / pieceLength + roundUp ? 1 : 0;
									int bitfieldSize = (n + 7)/8;
									uint8_t* ourBitfield;
									ourBitfield = new uint8_t[bitfieldSize];
									
									// we don't have any of the pieces yet
									for (int b = 0; b < bitfieldSize; b++) {
										ourBitfield[b] = 0;
									}
									
									// send it back
									
									break;
								case MSG_ID_REQUEST:		// 6
									// one request for one piece
									// on receiving a request, read data from file and generate the Piece msg
									
									// the "length" of request is set to "piecelength" in the torrent file
									break;
								case MSG_ID_PIECE:			// 7
									// on receiving the corresponding piece, verify the piece against the corresponding
									//		hash in "piece" in the torrent file
									
									// if verified
									//		write to file
									// else
									//		drop the piece and request it again
									
									// if piece is accepted
									//		send corresponding MSG_ID_HAVE msg  to all peers
									// if more than one peer exists
									//		requests should be balanced to each peers
									break;
								case MSG_ID_CANCEL:			// 8
									break;
								case MSG_ID_PORT:			// 9
									break;
								default:
									// do nothing
							}
						}
						
					}

					struct sockaddr_in clientAddr;
					socklen_t clientAddrLen = sizeof(clientAddr);
					if (getpeername(fd, (struct sockaddr *)&clientAddr, &clientAddrLen) == -1) 
					{
						perror("getpeername");
						return 7;
					}

					char ipstr[INET_ADDRSTRLEN] = {'\0'};
					inet_ntop(clientAddr.sin_family, &clientAddr.sin_addr, ipstr, sizeof(ipstr));
					std::cout << "receive data connection from " << ipstr << ":" << ntohs(clientAddr.sin_port) << ": " << buf << std::endl;
	


					// remove the socket from the socket set
					//FD_CLR(fd, &tmpFds);
				
				}
			}

		//IF < some number of connections ADD MORE PEER CONNECTIONS FROM TRACKER LIST
		}

	//std::cout << "keep going" << std::endl;
	}

return 0;
}

