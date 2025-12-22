#include "tcp_sender.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include <algorithm>
#include <cstdint>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // 通过计算得到当前还在飞数据量
  return current_seqno_ - sender_ackno_;
}

// How many consecutive retransmissions have happened?
uint64_t TCPSender::consecutive_retransmissions() const
{
  // 返回超时重传次数（超过阈值可视为接收端离线）
  return consecutive_retransimissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // 从出站字节流管道读取数据，在字节流管道还有新数据 + 接收方window有效情况下
  // 发送需要调用传入函数参数tansmit C++11 std::function

  // 在窗口长度为0时，默认1 byte数据
  const size_t current_window_size = ( sender_window_size_ == 0 ) ? 1 : sender_window_size_;

  while ( true ) {
    const size_t bytes_in_flight_ = current_seqno_ - sender_ackno_;

    if ( current_window_size <= bytes_in_flight_ ) {
      // 当前正在传输数据已可达到window上限，停止
      break;
    }

    size_t space_remaining = current_window_size - bytes_in_flight_;
    TCPSenderMessage senderMessage;

    if ( current_seqno_ == 0 ) {
      // SYN
      senderMessage.SYN = true;
      space_remaining--;
    }

    // 填充payload
    const size_t limit = min( TCPConfig::MAX_PAYLOAD_SIZE, space_remaining );

    // 通过peek获取实际数据（peek内部实现仅是部分连续数据，单个string，需要反复peek）
    while ( senderMessage.payload.size() < limit && input_.reader().bytes_buffered() > 0 ) {
      const string_view view = input_.reader().peek();

      if ( view.empty() ) {
        break;
      }

      const size_t needed = limit - senderMessage.payload.size();
      const size_t to_copy = min( view.size(), needed );

      senderMessage.payload.append( view.substr( 0, to_copy ) );
      input_.reader().pop( to_copy );
    }

    if ( input_.reader().is_finished() && space_remaining > senderMessage.sequence_length() && !FIN_sent ) {
      // FIN
      senderMessage.FIN = true;
      FIN_sent = true;
    }

    // 检查空包
    if ( senderMessage.sequence_length() == 0 ) {
      break;
    }

    // 检查错误
    if ( input_.has_error() ) {
      senderMessage.RST = true;
    }

    // 转换类型后填入message，加入到出站数据段监听队列
    senderMessage.seqno = Wrap32::wrap( current_seqno_, isn_ );
    outstanding_segments.emplace_back( current_seqno_, senderMessage );

    // message seqno需要填充payload后调用，且能自动处理SYN + FIN
    auto seqno_length_64 = senderMessage.sequence_length();
    current_seqno_ += seqno_length_64;

    transmit( senderMessage );

    // 发送消息后启动计时器
    if ( !timer_running_ ) {
      timer_running_ = true;
      time_elapsed_ = 0;
    }

    // 窗口长度为0时为探测包，发送完后需要打断循环
    if ( sender_window_size_ == 0 ) {
      break;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // 服务于TCPConnection，用于生成一个不携带数据，不占用序列号的空包，用于搭载ACK消息
  TCPSenderMessage empty_msg;

  empty_msg.seqno = Wrap32::wrap( current_seqno_, isn_ );

  empty_msg.SYN = false;
  empty_msg.FIN = false;
  empty_msg.payload = "";

  if ( input_.has_error() ) {
    empty_msg.RST = true;
  }

  return empty_msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // 接收来自接收方的feedback，ackno + window size，ackno之前的数据应当从字节流管道中删除
  sender_window_size_ = msg.window_size;

  // RST
  if ( msg.RST ) {
    input_.set_error();
    return;
  }

  // msg中ackno为optional
  if ( !msg.ackno.has_value() ) {
    return;
  }

  // 基于类中已保存ackno进行解包
  const uint64_t new_ack_64 = msg.ackno->unwrap( isn_, sender_ackno_ );

  if ( new_ack_64 > current_seqno_ ) {
    // 超出当前发送索引位置，非法ACK，忽略
    return;
  }

  if ( new_ack_64 > sender_ackno_ ) {
    sender_ackno_ = new_ack_64;

    // 重置RTO时间
    current_RTO_ = initial_RTO_ms_;

    // 重置超时重传次数
    consecutive_retransimissions_ = 0;

    // 重启计时器，当前时间归零
    time_elapsed_ = 0;
  }

  // 清理已抵达的监听数据段（通过不断弹出队头检查）
  while ( !outstanding_segments.empty() ) {
    auto& entry = outstanding_segments.front();
    const uint64_t seqno_end = entry.first + entry.second.sequence_length();

    if ( sender_ackno_ >= seqno_end ) {
      outstanding_segments.pop_front();
    } else {
      break;
    }
  }

  // 关闭当前计时器
  if ( outstanding_segments.empty() ) {
    timer_running_ = false;
    time_elapsed_ = 0;
  } else {
    timer_running_ = true;
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // 传入参数 ms_since_last_tick 为从上一个时间刻到现在的毫秒数差值
  if ( !timer_running_ ) {
    return;
  }

  time_elapsed_ += ms_since_last_tick;

  if ( time_elapsed_ >= current_RTO_ ) {
    // 骑手已超时
    if ( !outstanding_segments.empty() ) {
      // 不对每个segment进行追踪，而是维护队列中最早没被确认的包
      auto& segment = outstanding_segments.front();
      const TCPSenderMessage msg = segment.second;

      transmit( msg );
    }

    // RTO翻倍，仅在传回窗口大小不为0时才进行，窗口为0说明对应探测包
    if ( sender_window_size_ > 0 ) {
      current_RTO_ *= 2;
    }

    time_elapsed_ = 0;
    consecutive_retransimissions_++;
  }
}
