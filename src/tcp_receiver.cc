#include "tcp_receiver.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <optional>

using namespace std;

void TCPReceiver::receive( const TCPSenderMessage& message )
{
  if ( message.RST ) {
    // 连接错误，将字节流标记为错误状态，终止处理
    reassembler_.reader().set_error();
    return;
  }

  // 接收发送方信息，处理payload，确定stream index，插入到重组器中
  if ( message.SYN ) {
    // 首条报文，记录起始isn
    isn_ = message.seqno;
    isn_flag_ = true;
  }

  if ( !isn_flag_ ) {
    // 过滤还未SYN握手的数据报
    return;
  }

  // checkpoint_为字节流下一个期待的数据索引
  const uint64_t checkpoint_ = reassembler_.writer().bytes_pushed() + 1;

  // seqno -> absolute seqno -> stream index
  const uint64_t abs_seqno = message.seqno.unwrap( isn_, checkpoint_ );
  const uint64_t stream_index = abs_seqno - 1 + ( message.SYN ? 1 : 0 );

  // 传入FIN，自动处理末尾数据状态（字节流管道由重组器关闭）
  reassembler_.insert( stream_index, message.payload, message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  // 从重组器中获取feedback
  // ReceiverMessage：ackno, window size, RST
  TCPReceiverMessage feedback;
  feedback.RST = reassembler_.writer().has_error();

  uint64_t abs_ackno = 0;

  if ( !isn_flag_ ) {
    // 尚未建立连接
    feedback.ackno = nullopt;
  } else {
    // 已建立连接，SYN握手，FIN挥手
    abs_ackno = reassembler_.writer().bytes_pushed() + 1;
    if ( reassembler_.writer().is_closed() ) {
      abs_ackno += 1;
    }

    // absolute_seqno -> seqno
    feedback.ackno = Wrap32::wrap( abs_ackno, isn_ );
  }

  // 窗口大小，来自字节流管道，需要检查是否超出65536
  const uint64_t pipe_capacity = reassembler_.writer().available_capacity();
  feedback.window_size = ( pipe_capacity > UINT16_MAX ) ? UINT16_MAX : static_cast<uint16_t>( pipe_capacity );

  return feedback;
}
