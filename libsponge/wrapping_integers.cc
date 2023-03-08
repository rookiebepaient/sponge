#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // 从64位字节流索引号转换成为32位TCP序列号
    // 32位无符号整型最大值16进制表示为 0xFFFF FFFF
    // 32位有符号整型最大值16进制表示为 0x7FFF FFFF
    // 将64位无符号整型转换为32位无符号整型，如果原64位数超过32位无符号整型最大值
    // 那么32位无符号整型会取32位无符号整型最大值
    return isn + static_cast<uint32_t>(n);
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    //DUMMY_CODE(n, isn, checkpoint);
    WrappingInt32 chpoint_int32 = wrap(checkpoint, isn);  //求出checkpoint在32位下的位置
    int32_t offset = n - chpoint_int32;
    int64_t abs_seqno = checkpoint + offset;
    if (abs_seqno < 0) {
        abs_seqno += (1ul << 32);
    }
    return abs_seqno;
}
