#include "wrapping_integers.hh"
#include <cmath>
#include <cstdint>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // debug( "unimplemented wrap( {}, {} ) called", n, zero_point.raw_value_ );
  // 转换 absolute seqno -> seqno

  // 从64位转换到32位，2 ^ 32后循环，static_cast直接高位截断，取低32位
  auto n_32 = static_cast<uint32_t>( n );

  return Wrap32 { n_32 + zero_point.raw_value_ };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // 转换类中字段 seqno -> absolute seqno

  // 1. 取checkpoint低位，即强制类型转换至32位（映射到当前周期的读数）
  auto checkpoint_32 = static_cast<uint32_t>( checkpoint );

  // 2. 取当前值相对zero_point偏移量
  auto seqno_offset = raw_value_ - zero_point.raw_value_;

  // 3. 两个偏移量的无符号距离
  const uint32_t diff = seqno_offset - checkpoint_32;

  // 4. 将计算距离解释为周期中的最近距离，转换为有符号整数实现
  auto distance = static_cast<int32_t>( diff );

  auto result = static_cast<int64_t>( checkpoint + distance );

  // 必须向前的例外情况
  if ( result < 0 ) {
    return result + ( 1ULL << 32 );
  }

  return static_cast<uint64_t>( result );
}
