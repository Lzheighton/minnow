#pragma once

#include "lossy_fd_adapter.hh"
#include "tcp_over_ip.hh"
#include "tcp_segment.hh"
#include "tun.hh"

#include <optional>
#include <utility>

// C++ 20 模板参数检查，类型T的实例化对象a必须拥有成员write和read两个成员函数（限定函数传参与返回值类型）
template<class T>
concept TCPDatagramAdapter = requires( T a, TCPMessage seg ) {
  { a.write( seg ) } -> std::same_as<void>;

  { a.read() } -> std::same_as<std::optional<TCPMessage>>;
};

//! \brief A FD adapter for IPv4 datagrams read from and written to a TUN device
class TCPOverIPv4OverTunFdAdapter : public TCPOverIPv4Adapter
{
private:
  TunFD _tun;

public:
  //! Construct from a TunFD
  explicit TCPOverIPv4OverTunFdAdapter( TunFD&& tun ) : _tun( std::move( tun ) ) {}

  //! 尝试读取和解析IPv4数据报（包含与当前连接相关的TCPseg）
  std::optional<TCPMessage> read();

  //! 接收TCPseg，创建一个IPv4数据报并写进TUN虚拟网卡
  void write( const TCPMessage& seg );

  /* 
  接口的另一种设计方式：
  C++ 类型转换运算符，允许对象显式转换成目标类型，以便获取成员变量_tun TUN句柄
  explicit 用于防止隐式转换，适配器只是接口而不是具体的句柄本身

  使用示例：
  TCPOverIPv4OverTunFdAdaptor adp{TunFD{"tun144"}};
  TunFD& tun = static_cast<TumFD&>(adp);
  */
  explicit operator TunFD&() { return _tun; }

  //! 同理，const版本
  explicit operator const TunFD&() const { return _tun; }

  //! 同理，降级为Linux文件描述符
  FileDescriptor& fd() { return _tun; }
};

// 编译期断言处理，泛型检查，使用前面concept定义
static_assert( TCPDatagramAdapter<TCPOverIPv4OverTunFdAdapter> );
static_assert( TCPDatagramAdapter<LossyFdAdapter<TCPOverIPv4OverTunFdAdapter>> );
