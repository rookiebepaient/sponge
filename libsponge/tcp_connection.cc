#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return tcpconn_ms_tick; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    // DUMMY_CODE(seg);
    // receiving seg
    //如果收到数据包，就将超时计时器记为0；
    tcpconn_ms_tick = 0;
    //判断是否需要发送ackno, length_in_sequence_space记录是载荷的大小，如果有SYN则加一，有FIN则加一
    bool _need_send_ack = seg.length_in_sequence_space();

    // rule 1 如果接收到了RST报文段，将输入输出字节流置为error(),并立即结束连接
    if (seg.header().rst) {
        set_rst_state(false);
        //结束连接
        return;
    }

    // rule 2 将报文交付给_receiver, TCPReceiver关注的字段 seqno, SYN, payload, FIN
    _receiver.segment_received(seg);

    // rule 3 如果有ACK位，交给TCPSender处理，TCPSender关注的字段 ackno, window_size
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        if (_need_send_ack and !_sender.segments_out().empty()) {
            _need_send_ack = false;
        }
    }

    // LINSTEN，当收到了对方SYN且己方处于连接关闭状态时
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        connect();
        return;
    }

    // CLOSE_WAIT
    if (_linger_after_streams_finish and _receiver.stream_out().input_ended() and
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED) {
        _linger_after_streams_finish = false;
    }

    // CLOSED
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV and
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED and !_linger_after_streams_finish) {
        _active = false;
        return;
    }

    // rule 5 对保活报文段进行回应,对等层实体可能选择发送一个 非法序列号
    //来试探你的TCP是否还处于活跃状态，如果活跃就发送一个空报文过去并告知自己的窗口大小
    //己方TCP应回应 保活报文段 ，这些报文段不占用任何的序列号
    if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0) and
        seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
    }

    // rule 4 只要接受的报文段占用了序列号，那么TCPConnection就要保证至少有一个报文予以回复
    //去反应 ackno 和 windowsize 的变化
    if (_need_send_ack) {
        _sender.send_empty_segment();
    }

    send_segments();
}

bool TCPConnection::active() const {
    if (_linger_after_streams_finish) {
        return true;
    }
    return _active;
}

size_t TCPConnection::write(const string &data) {
    // DUMMY_CODE(data);
    //将应用层的数据写入报文并发送
    size_t bytes_written = _sender.stream_in().write(data);
    _sender.fill_window();
    send_segments();
    return bytes_written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    tcpconn_ms_tick += ms_since_last_tick;
    //告知TCPsender 过去了过长的时间
    _sender.tick(ms_since_last_tick);
    //如果连续重传计数器大于了TCPCofig最大重传上限，
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        //则放弃连接，并发送一个RST报文段
        set_rst_state(true);
        return;
    }

    send_segments();

    //相对于客户端来看，如果处于time_wait状态并且超时，则可以静默关闭连接
    if (_linger_after_streams_finish and TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV and
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED and
        tcpconn_ms_tick >= 10 * _cfg.rt_timeout) {
        _active = false;
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segments();
}

void TCPConnection::connect() {
    // TCPSender有一个成员变量是 _segments_out,TCPConnection也有一个,但是两者并不是同一概念
    _sender.fill_window();
    _active = true;
    send_segments();
}

void TCPConnection::send_segments() {
    while (not _sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = static_cast<uint16_t>(_receiver.window_size());
        }
        _segments_out.push(seg);
    }
}

void TCPConnection::set_rst_state(bool _sent_rst) {
    if (_sent_rst) {
        TCPSegment _rst_segment;
        _rst_segment.header().rst = true;
        _segments_out.push(_rst_segment);
    }
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _linger_after_streams_finish = false;
    _active = false;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            set_rst_state(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
