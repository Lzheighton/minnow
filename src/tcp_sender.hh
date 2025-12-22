#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <functional>

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) )
    , isn_( isn )
    , initial_RTO_ms_( initial_RTO_ms )
    , outstanding_segments()
    , current_RTO_( initial_RTO_ms )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  Reader& reader() { return input_.reader(); }

  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  // 状态量
  bool FIN_sent { false };            // FIN包是否已经发送
  uint64_t current_seqno_ { 0 };      // 当前的绝对序列号
  uint64_t sender_ackno_ { 0 };       // 接收端返回的ackno，receive更新
  uint16_t sender_window_size_ { 1 }; // 同上，window size，需要遵照F&Q初始化为1

  // 监听还在飞的数据段
  std::deque<std::pair<uint64_t, TCPSenderMessage>> outstanding_segments;

  // 计时器
  size_t current_RTO_ { 0 };                  // 当前RTO（指数退缩）
  size_t consecutive_retransimissions_ { 0 }; // 当前超时重传次数

  bool timer_running_ { false }; // 判断当前计时器是否正在运行
  size_t time_elapsed_ { 0 };    // 计时器启动后经过时间
};
