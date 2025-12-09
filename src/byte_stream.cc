#include "byte_stream.hh"
#include "debug.hh"
#include <algorithm>
#include <cstdint>
#include <string_view>

using namespace std;

// capacity构造
ByteStream::ByteStream( uint64_t capacity ) : impl_( make_shared<ByteStreamImpl>( capacity ) ) {}

// 拷贝构造
ByteStream::ByteStream( const ByteStream& other ) : impl_( make_shared<ByteStreamImpl>( *other.impl_ ) ) {}

// 拷贝赋值
ByteStream& ByteStream::operator=( const ByteStream& other )
{
  if ( this != &other ) {
    impl_ = std::make_shared<ByteStreamImpl>( *other.impl_ );
  }
  return *this;
}

// 将string类型data压入流，需要检查当前capacity
void Writer::push( string data )
{
  if ( data.empty() || impl_->end_input_ ) {
    return;
  }

  const uint64_t available = available_capacity();
  const uint64_t len_to_write = min( available, static_cast<uint64_t>( data.size() ) );

  if ( len_to_write == 0 ) {
    return;
  }

  if ( len_to_write < data.size() ) {
    // 传入data需要被resize函数截断
    data.resize( len_to_write );
  }

  impl_->buffer_.push_back( std::move( data ) );
  impl_->written_size_ += len_to_write;
  impl_->bytes_buffered_ += len_to_write;
}

void Writer::close()
{
  // 不是向deque中写入EOF文件结束符，而是修改流对象状态
  impl_->end_input_ = true;
}

bool Writer::is_closed() const
{
  return impl_->end_input_;
}

// 流的当前剩余可写入capacity，通过计算所得，不维护实际变量
uint64_t Writer::available_capacity() const
{
  return impl_->capacity_ - impl_->bytes_buffered_;
}

// 经过流字节数总量
uint64_t Writer::bytes_pushed() const
{
  return impl_->written_size_;
}

string_view Reader::peek() const
{
  if ( impl_->buffer_.empty() ) {
    return {};
  }

  const std::string& current_block = impl_->buffer_.front();
  const std::string_view sv( current_block );

  // 去掉已经读过偏移量长度
  return sv.substr( impl_->front_offset_ );
}

void Reader::pop( uint64_t len )
{
  debug( "pop function was called, arg len: {}", len );
  // 超出max长度时整体删除
  len = std::min( len, bytes_buffered() );

  // 在buffer中移除len长度的字节
  impl_->read_size_ += len;
  impl_->bytes_buffered_ -= len; // 确保读取后writer还能写入

  while ( len > 0 ) {
    const std::string& front_str = impl_->buffer_.front();
    const uint64_t current_str_size = front_str.size();
    const uint64_t remain_in_front = current_str_size - impl_->front_offset_;

    // 要 pop 的长度小于当前字符串剩余，直接移动 offset
    if ( len < remain_in_front ) {
      impl_->front_offset_ += len;
      len = 0;
    } else {
      // 要pop的长度大于当前字符串剩余
      impl_->buffer_.pop_front();
      impl_->front_offset_ = 0;
      len -= remain_in_front;
    }
  }
}

bool Reader::is_finished() const
{
  // 返回当前流状态：被主动关闭且读取为空
  return impl_->end_input_ && impl_->buffer_.empty();
}

uint64_t Reader::bytes_buffered() const
{
  return impl_->bytes_buffered_;
}

uint64_t Reader::bytes_popped() const
{
  return impl_->read_size_;
}
