/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014,  Regents of the University of California
 *
 * This file is part of Simple BT.
 * See AUTHORS.md for complete list of Simple BT authors and contributors.
 *
 * NSL is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NSL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NSL, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * \author Yingdi Yu <yingdi@cs.ucla.edu>
 */

#ifndef SBT_CLIENT_HPP
#define SBT_CLIENT_HPP

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
#include <netdb.h>

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

#define PEERLEN 20
#define HANDSHAKELEN 68 
#define BUFSIZE 20000

namespace sbt {
namespace msg {

struct PeerFD {
  PeerInfo pi;
  int fd;
};

class Client
{
public:
  Client(const std::string& port1, const std::string& torrent);

  int addPeer(fd_set& tmpFds);

private:
  
  std::string port;
  std::string torrent;
  
  //Peer Data
  std::map<int, int> socketStatus;
  //int getFDofPeer(std::vector<PeerFD>& pfd); //deleted
  
  //int setupPeerListener();
  int setupPeerListener(fd_set& tmpFds);
  bool isFDPeer(int fd);
  int getFDStatus(int fd);
  void setFDStatus(int fd, int status);
  int addPeer();
  int listenerFD;
  
  int maxSockfd; //tommy
  
  //Tracker
  HttpRequest trRequest;
  void setupTrackerRequest();
  bool shouldUpdateTracker();
  void sendTrackerRequest();
  std::vector<PeerInfo> trackerPeers;
  int trInterval;
  time_t lastCheck;
  
  MetaInfo torrentInfo;
  std::string encodedPeer;
  
  //receiving data
  HandShake receiveHandShake(int fd);
  void sendHandShake(int fd);
  void receiveMessage(int fd);
  HandShake clientHandShake;
  
  //file data
  int uploaded, downloaded;
  std::string event;
  
  //Bitfield
  void processPeerBitfield(sbt::ConstBufferPtr buf);
  std::vector<bool> ourBitField;
  void Client::sendOurBitfield(int fd);
};

} // namespace sbt
} // namespace msg

#endif // SBT_CLIENT_HPP
