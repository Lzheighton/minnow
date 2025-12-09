#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <deque>
#include<memory>

class Reader;
class Writer;

class ByteStream
{
public:
  explicit ByteStream( uint64_t capacity );
  ByteStream(const ByteStream& other);
  ByteStream( ByteStream&& other ) noexcept = default;
  ~ByteStream() = default;
  ByteStream& operator=(const ByteStream& other);
  ByteStream& operator=( ByteStream&& other ) noexcept = default;

  // Helper functions (provided) to access the ByteStream's Reader and Writer interfaces
  Reader& reader();
  const Reader& reader() const;
  Writer& writer();
  const Writer& writer() const;

  void set_error() { impl_->error_ = true; };       // Signal that the stream suffered an error.
  bool has_error() const { return impl_->error_; }; // Has the stream had an error?

protected:
  // 共享状态模式，所有状态被打包进一个结构体
  struct ByteStreamImpl {
    uint64_t capacity_;

    bool error_ {false};
    bool end_input_ {false};

    // 记录两侧处理的size大小
    uint64_t written_size_ {0};
    uint64_t read_size_ {0};

    // deque，维护两端
    std::deque<std::string> buffer_;
    uint64_t front_offset_ {0};   // 记录首个分块的读取偏移量
    uint64_t bytes_buffered_ {0}; // 记录当前buffer里的字节总数(string作为分块掩盖了size)

    explicit ByteStreamImpl(uint64_t _capacity) : capacity_(_capacity), buffer_() {}
  };

  std::shared_ptr<ByteStreamImpl> impl_;
};

class Writer : public ByteStream
{
public:
  void push( std::string data ); // Push data to stream, but only as much as available capacity allows.
  void close();                  // Signal that the stream has reached its ending. Nothing more will be written.

  bool is_closed() const;              // Has the stream been closed?
  uint64_t available_capacity() const; // How many bytes can be pushed to the stream right now?
  uint64_t bytes_pushed() const;       // Total number of bytes cumulatively pushed to the stream
};

class Reader : public ByteStream
{
public:
  std::string_view peek() const; // Peek at the next bytes in the buffer -- ideally as many as possible.
  void pop( uint64_t len );      // Remove `len` bytes from the buffer.

  bool is_finished() const;        // Is the stream finished (closed and fully popped)?
  uint64_t bytes_buffered() const; // Number of bytes currently buffered (pushed and not popped)
  uint64_t bytes_popped() const;   // Total number of bytes cumulatively popped from stream
};

/*
 * read: A (provided) helper function thats peeks and pops up to `max_len` bytes
 * from a ByteStream Reader into a string;
 */
void read( Reader& reader, uint64_t max_len, std::string& out );
