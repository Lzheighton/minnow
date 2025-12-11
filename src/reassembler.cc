#include "reassembler.hh"
#include "debug.hh"
#include <cstdint>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  debug( "insert func was called: arg: first_index: {}, data: {}, is_last_substring: {}",
         first_index,
         data,
         is_last_substring );

  // 流结束位置
  if ( is_last_substring ) {
    // 记录endIndex为最后一个字节的下一个位置（end位置 + 1)
    end_index_ = first_index + data.size();
    have_got_end_index_ = true;
  }

  // Part 1：裁剪两端超长部分
  // 去除已写入部分
  if ( first_index + data.size() <= cur_index_ ) {
    if ( have_got_end_index_ && cur_index_ >= end_index_ ) {
      output_.writer().close();
    }
    return;
  }
  if ( first_index < cur_index_ ) {
    const uint64_t skip = cur_index_ - first_index;
    data = data.substr( skip );
    first_index = cur_index_;
  }

  // 去除超出容量部分
  const uint64_t max_end = cur_index_ + output_.writer().available_capacity();
  if ( first_index >= max_end ) {
    return;
  }
  if ( first_index + data.size() > max_end ) {
    data = data.substr( 0, max_end - first_index );
  }

  // Part 2：尝试写入内容
  if ( first_index == cur_index_ ) {
    // 数据连续，直接写入
    cur_index_ += data.size();
    output_.writer().push( move( data ) );

    // 尝试从缓冲区继续写入连续数据
    drain_buffer();

    // debug("curIndex_: {}, endIndex_: {}, getEndIndex: {}", curIndex_, endIndex_, getEndIndex);

    // 经过了插入操作，需要检查是否到达流结束，结束后结束writer向pipe写入
    if ( have_got_end_index_ && cur_index_ >= end_index_ ) {
      // debug("Reach the position of closing byte stream pipe");
      output_.writer().close();
    }
  } else {
    // 数据不连续，需要缓存并去重
    merge_into_buffer( first_index, data );
  }
}

// 从缓冲区尝试写入连续数据
void Reassembler::drain_buffer()
{
  while ( !str_buffer_.empty() ) {
    auto it = str_buffer_.begin();
    const uint64_t seg_start = it->first;

    // 不连续，停止
    if ( seg_start > cur_index_ ) {
      break;
    }

    string& segment = it->second;
    const uint64_t seg_end = seg_start + segment.size();

    // 整段已写过，删除
    if ( seg_end <= cur_index_ ) {
      str_buffer_.erase( it );
      continue;
    }

    // 部分重叠，截掉已写部分
    if ( seg_start < cur_index_ ) {
      const uint64_t skip = cur_index_ - seg_start;
      segment = segment.substr( skip );
    }

    // 写入并更新位置
    cur_index_ += segment.size();
    output_.writer().push( move( segment ) );
    str_buffer_.erase( it );
  }
}

// 处理缓存时的去重逻辑
void Reassembler::merge_into_buffer( uint64_t& first_index, string& data )
{
  if ( data.empty() ) {
    return;
  }

  uint64_t data_end = first_index + data.size();
  auto it = str_buffer_.lower_bound( first_index );

  // 检查前一个片段的重叠
  if ( it != str_buffer_.begin() ) {
    auto prev = std::prev( it );
    const uint64_t prev_end = prev->first + prev->second.size();

    if ( prev_end > first_index ) {
      if ( prev_end >= data_end ) {
        // 完全被覆盖，丢弃
        return;
      }
      // 截掉左侧重叠
      const uint64_t skip = prev_end - first_index;
      data = data.substr( skip );
      first_index = prev_end;
      data_end = first_index + data.size();
    }
  }

  // 检查后续片段的重叠
  while ( it != str_buffer_.end() && it->first < data_end ) {
    const uint64_t next_end = it->first + it->second.size();

    if ( next_end <= data_end ) {
      // 后续片段被完全覆盖，删除
      it = str_buffer_.erase( it );
    } else {
      // 部分覆盖，截断当前数据
      data = data.substr( 0, it->first - first_index );
      break;
    }
  }

  if ( !data.empty() ) {
    str_buffer_[first_index] = move( data );
  }
}

uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t pending_bytes = 0;

  for ( const auto& entity : str_buffer_ ) {
    pending_bytes += entity.second.size();
  }

  return pending_bytes;
}
