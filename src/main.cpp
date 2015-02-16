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
#include <cstdlib>
#include <cstring>
#include <climits>
#include <ctime>
#include <netdb.h>
#include <netinet/in.h>
#include <map>

#include "client.hpp"
#include "common.hpp"
#include "meta-info.hpp"
#include "tracker-response.hpp"
#include "http/http-response.hpp"
#include "http/http-request.hpp"
#include "http/url-encoding.hpp"
#include "util/bencoding.hpp"
#include "util/buffer-stream.hpp"
#include "msg/handshake.hpp"
#include "msg/msg-base.hpp"

using namespace sbt;
using namespace msg;

#define PEERLEN 20
#define HANDSHAKELEN 68 
#define BUFSIZE 20000


int
main(int argc, char** argv)
{

	// Check command line arguments.
	if (argc != 3)
	{
		std::cerr << "Usage: simple-bt <port> <torrent_file>\n";
		return 1;
	}	 

	Client c(argv[1], argv[2]);

	return 0;
}

