#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

/**
 *  \brief 当前 TCPReceiver 大体上有三种状态， 分别是
 *      1. LISTEN，此时 SYN 包尚未抵达。可以通过 _syn 标志位来判断是否在当前状态
 *      2. SYN_RECV, 此时 SYN 抵达。只能判断当前不在 1、3状态时才能确定在当前状态
 *      3. FIN_RECV, 此时 FIN 抵达。可以通过 ByteStream end_input 来判断是否在当前状态
 */

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader &header = seg.header();
    if (!_syn) {
        // SYN 包之前的数据包必须全部丢弃
        // 即未经过三次握手，不接受传来的报文段，防止接受旧数据
        if (!header.syn) {
            return;
        }
        // isn记录的是对等方发送过来的isn，为后续的ackno计算做准备
        _isn = header.seqno;
        _syn = true;
    }
    // 先得到距离seqno最近一次报文段的 abs_seqno 再unwarp得到这次报文段对应的 abs_seqno
    uint64_t abs_ackno = _reassembler.stream_out().bytes_written() + 1;
    uint64_t curr_abs_seqno = unwrap(header.seqno, _isn, abs_ackno);

    // 绝对序列号 - 1 = 流索引号，SYN 包中的 payload 不能被丢弃
    uint64_t stream_index = curr_abs_seqno - 1 + header.syn;
    _reassembler.push_substring(seg.payload().copy(), stream_index, header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    WrappingInt32 ackno = _isn + stream_out().bytes_written();
    if (_syn) {
        // 如果不在 LISTEN 状态，则 ackno 还需要加上一个 SYN 标志的长度
        ackno = ackno + 1;
        // 如果当前处于 FIN_RECV 状态，则还需要加上 FIN 标志长度
        if (stream_out().input_ended()) {
            ackno = ackno + 1;
        }
        return ackno;
    }
    return {};
}

// reassembler 可存储的中尚未整合的报文段的索引范围
size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
