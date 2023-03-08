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
    // DUMMY_CODE(seg);
    /* 原始自己写的版本，测试样例全部通过
    uint64_t abs_seqno; //uint64 类型, ackno 是uint32类型
    uint64_t checkpoint = stream_out().bytes_written();    // bytes_written() 得到的是abs seqno;
    //WrappingInt32 _ackno;

    TCPHeader tcp_header = seg.header();
    Buffer payload = seg.payload();

    if (tcp_header.syn) {
        _syn = true;
        _isn = tcp_header.seqno;    //isn 是 seqno, uint32类型
        //_reassembler.push_substring(payload.copy(), 0, tcp_header.fin);
    }

    if (_syn && (!tcp_header.syn)) {
        abs_seqno = unwrap(tcp_header.seqno, _isn, checkpoint);
        uint64_t stream_index = abs_seqno - 1;
        if (tcp_header.seqno != _isn) {
            _reassembler.push_substring(payload.copy(), stream_index, tcp_header.fin);  //这个index需要的是stream index
        }
        // if (tcp_header.seqno == _ackno) {
        //     //bytes_written()是累加的.但需要的是每次已读取的字节数
        //     //现在的问题是,_ackno
    在乱序到来的字节流被重新整合后,会发生错误的情况.错误的情况就是,在正确的到来一个seg后,
        //     //_ackno被正确响应,当下一个是乱序到来的seg时,_ackno
    依然是上一个被正确响应的,再来一个正确顺序的seg时,此时响应的_ackno将会是
        //     _ackno = tcp_header.seqno + stream_out().buffer_size();
        // }

    }
    */

    const TCPHeader &header = seg.header();
    if (!_syn) {
        // 注意 SYN 包之前的数据包必须全部丢弃
        if (!header.syn)
            return;
        _isn = header.seqno;
        _syn = true;
    }
    uint64_t abs_ackno = _reassembler.stream_out().bytes_written() + 1;
    uint64_t curr_abs_seqno = unwrap(header.seqno, _isn, abs_ackno);

    //! NOTE: SYN 包中的 payload 不能被丢弃
    //! NOTE: reassember 足够鲁棒以至于无需进行任何 seqno 过滤操作
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

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
