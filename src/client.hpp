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

#include "common.hpp"
#include "meta-info.hpp"
#include "tracker-response.hpp"
#include "msg/handshake.hpp"
#include "http/http-request.hpp"

#include <string>
#include <ctime>

#define PEERLEN 20
#define HANDSHAKELEN 68 
#define BUFSIZE 20000

namespace sbt {
struct cmpPeer  //comparator for peer info
{
    bool operator()(const PeerInfo& a, const PeerInfo& b) const 
    {
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

class Client
{
public:
  Client(const std::string& port, const std::string& torrent);
  
  msg::Handshake receiveHandShake(int fd);
  
  void sendHandShake(int fd);
  
  void receiveMessage(int fd);
  
  void setupTrackerRequest();
  
  void sendTrackerRequest();
  
  bool shouldUpdateTracker();
  
  int setupPeerListener(fd_set& tmpFds);
  
  int addPeer(fd_set& tmpFds);
  
private:
  
  std::string port;
  std::string torrent;
  
  //Peer Data
  std::map<int, int> socketStatus;
  std::map<PeerInfo, int> peerToFD;
  int setupPeerListener();
  bool isPeerConnected(PeerInfo pi); //won't add peer if not connected
  int getPeerStatus(PeerInfo pi);
  bool isFDPeer(int fd);
  int getFDStatus(int fd);
  void setFDStatus(int fd, int status);
  void addPeer(PeerInfo pi, int fd);
  void removePeer();
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
  MsgBase receiveMessage(int fd);
  HandShake clientHandShake;
  
  //file data
  int uploaded, downloaded;
  std::string event;
  
};

} // namespace sbt

#endif // SBT_CLIENT_HPP
