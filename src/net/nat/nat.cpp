// This file is a part of the IncludeOS unikernel - www.includeos.org
//
// Copyright 2017 Oslo and Akershus University College of Applied Sciences
// and Alfred Bratterud
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <net/nat/nat.hpp>
#include <net/inet_common.hpp> // checksum_adjust
#include <net/tcp/packet.hpp>
#include <net/ip4/packet_udp.hpp>

namespace net {
namespace nat {

inline void recalc_ip_checksum(IP4::IP_packet& pkt, ip4::Addr old_addr, ip4::Addr new_addr);
inline void recalc_tcp_checksum(tcp::Packet& pkt, Socket osock, Socket nsock);
inline void recalc_udp_checksum(PacketUDP& pkt, Socket osock, Socket nsock);

void snat(IP4::IP_packet& pkt, Socket src_socket)
{
  switch(pkt.ip_protocol())
  {
    case Protocol::TCP:
      tcp_snat(pkt, src_socket);
      return;

    case Protocol::UDP:
      udp_snat(pkt, src_socket);
      return;

    default:
      return;
  }
}

void dnat(IP4::IP_packet& pkt, Socket dst_socket)
{
  switch(pkt.ip_protocol())
  {
    case Protocol::TCP:
      tcp_dnat(pkt, dst_socket);
      return;

    case Protocol::UDP:
      udp_dnat(pkt, dst_socket);
      return;

    default:
      return;
  }
}

void dnat(IP4::IP_packet& pkt, const ip4::Addr new_addr)
{
  switch(pkt.ip_protocol())
  {
    case Protocol::TCP:
      tcp_dnat(pkt, new_addr);
      return;

    case Protocol::UDP:
      udp_dnat(pkt, new_addr);
      return;

    case Protocol::ICMPv4:
      icmp_dnat(pkt, new_addr);
      return;

    default:
      return;
  }
}

void dnat(IP4::IP_packet& pkt, const uint16_t new_port)
{
  switch(pkt.ip_protocol())
  {
    case Protocol::TCP:
      tcp_dnat(pkt, new_port);
      return;

    case Protocol::UDP:
      udp_dnat(pkt, new_port);
      return;

    default:
      return;
  }
}

void tcp_snat(IP4::IP_packet& p, Socket new_sock)
{
  Expects(p.ip_protocol() == Protocol::TCP);
  auto& pkt = static_cast<tcp::Packet&>(p);
  // Recalc checksum
  recalc_tcp_checksum(pkt, pkt.source(), new_sock);
  // Set the value
  pkt.set_source(new_sock);
}

void tcp_dnat(IP4::IP_packet& p, Socket new_sock)
{
  Expects(p.ip_protocol() == Protocol::TCP);
  auto& pkt = static_cast<tcp::Packet&>(p);
  // Recalc checksum
  recalc_tcp_checksum(pkt, pkt.destination(), new_sock);

  // change destination socket
  pkt.set_destination(new_sock);
}

void tcp_dnat(IP4::IP_packet& p, const ip4::Addr new_addr)
{
  Expects(p.ip_protocol() == Protocol::TCP);
  auto& pkt = static_cast<tcp::Packet&>(p);

  // recalc IP checksum
  auto old_addr = pkt.ip_dst();
  recalc_ip_checksum(pkt, old_addr, new_addr);

  // recalc tcp address part
  auto tcp_sum = pkt.tcp_checksum();
  checksum_adjust(&tcp_sum, &old_addr, &new_addr);
  pkt.set_checksum(tcp_sum);

  // change destination address
  pkt.set_ip_dst(new_addr);
}

void tcp_dnat(IP4::IP_packet& p, const uint16_t new_port)
{
  Expects(p.ip_protocol() == Protocol::TCP);
  auto& pkt = static_cast<tcp::Packet&>(p);

  // recalc tcp port
  auto tcp_sum = pkt.tcp_checksum();
  auto old_port = htons(pkt.dst_port());
  auto tmp_port = htons(new_port);
  checksum_adjust<uint16_t>(&tcp_sum, &old_port, &tmp_port);
  pkt.set_checksum(tcp_sum);

  // change destination port
  pkt.set_dst_port(new_port);
}

void udp_snat(IP4::IP_packet& p, Socket new_sock)
{
  Expects(p.ip_protocol() == Protocol::UDP);
  auto& pkt = static_cast<PacketUDP&>(p);
  auto old_sock = Socket{pkt.ip_src(), pkt.src_port()};
  // Recalc checksum
  recalc_udp_checksum(pkt, old_sock, new_sock);
  // Set the value
  pkt.set_ip_src(new_sock.address());
  pkt.set_src_port(new_sock.port());
}

void udp_dnat(IP4::IP_packet& p, Socket new_sock)
{
  Expects(p.ip_protocol() == Protocol::UDP);
  auto& pkt = static_cast<PacketUDP&>(p);
  auto old_sock = Socket{pkt.ip_dst(), pkt.dst_port()};

  // Recalc checksum
  recalc_udp_checksum(pkt, old_sock, new_sock);

  // change destination
  pkt.set_ip_dst(new_sock.address());
  pkt.set_dst_port(new_sock.port());
}

void udp_dnat(IP4::IP_packet& p, const ip4::Addr new_addr)
{
  Expects(p.ip_protocol() == Protocol::UDP);
  auto& pkt = static_cast<PacketUDP&>(p);

  // recalc IP checksum
  auto old_addr = pkt.ip_dst();
  recalc_ip_checksum(pkt, old_addr, new_addr);

  // TODO: recalc UDP checksum (psuedo header change)

  // change destination address
  pkt.set_ip_dst(new_addr);
}

void udp_dnat(IP4::IP_packet& p, const uint16_t port)
{
  Expects(p.ip_protocol() == Protocol::UDP);
  auto& pkt = static_cast<PacketUDP&>(p);

  // TODO: recalc UDP checksum

  // change destination port
  pkt.set_dst_port(port);
}

void icmp_snat(IP4::IP_packet& pkt, const ip4::Addr addr)
{
  // recalc IP checksum
  recalc_ip_checksum(pkt, pkt.ip_src(), addr);
  pkt.set_ip_src(addr);
}

void icmp_dnat(IP4::IP_packet& pkt, const ip4::Addr addr)
{
  // recalc IP checksum
  recalc_ip_checksum(pkt, pkt.ip_dst(), addr);
  pkt.set_ip_dst(addr);
}

inline void recalc_ip_checksum(IP4::IP_packet& pkt, ip4::Addr old_addr, ip4::Addr new_addr)
{
  auto ip_sum = pkt.ip_checksum();
  checksum_adjust(&ip_sum, &old_addr, &new_addr);
  pkt.set_ip_checksum(ip_sum);
}

inline void recalc_tcp_checksum(tcp::Packet& pkt, Socket osock, Socket nsock)
{
  auto old_addr = osock.address();
  auto new_addr = nsock.address();

  // recalc IP checksum
  recalc_ip_checksum(pkt, old_addr, new_addr);

  auto tcp_sum = pkt.tcp_checksum();
  // recalc tcp address part
  checksum_adjust(&tcp_sum, &old_addr, &new_addr);
  // recalc tcp port part
  auto old_port = htons(osock.port());
  auto new_port = htons(nsock.port());
  checksum_adjust<uint16_t>(&tcp_sum, &old_port, &new_port);
  // set the new sum
  pkt.set_checksum(tcp_sum);
}

inline void recalc_udp_checksum(PacketUDP& pkt, Socket osock, Socket nsock)
{
  auto old_addr = osock.address();
  auto new_addr = nsock.address();

  // recalc IP checksum
  recalc_ip_checksum(pkt, old_addr, new_addr);

  // TODO: recalc UDP (not in use)
}

}
}
