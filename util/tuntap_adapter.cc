#include "tuntap_adapter.hh"
#include "helpers.hh"

using namespace std;

optional<TCPMessage> TCPOverIPv4OverTunFdAdapter::read()
{
  // 文件描述符传参方式，strs为string数组，1号位和2号位分别为IP和TCP header
  vector<string> strs( 3 );
  strs[0].resize( IPv4Header::LENGTH );
  strs[1].resize( TCPSegment::HEADER_LENGTH );

  _tun.read( strs );

  InternetDatagram ip_dgram;
  if ( parse( ip_dgram, move( strs ) ) ) {
    return unwrap_tcp_in_ip( move( ip_dgram ) );
  }
  return {};
}

void TCPOverIPv4OverTunFdAdapter::write( const TCPMessage& seg )
{
  _tun.write( serialize( wrap_tcp_in_ip( seg ) ) );
}

//! Specialize LossyFdAdapter to TCPOverIPv4OverTunFdAdapter
template class LossyFdAdapter<TCPOverIPv4OverTunFdAdapter>;
