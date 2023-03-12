#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    // 当不知道peer的窗口大小时，默认对方的窗口大小为1
    size_t cur_receiver_window_size = _receiver_windowsize == 0 ? 1 : _receiver_windowsize;
    while (cur_receiver_window_size > _bytes_in_flight) {
        TCPSegment segment;
        // SYN位只在建立连接时才会置为1，在数据传输和关闭连接时用不到SYN位
        if (!_is_syn_sent) {
            segment.header().syn = true;
            _is_syn_sent = true;
        }
        // 用next_seqno创建报文段, next_seqno为报文段首字节的字节流编号
        segment.header().seqno = next_seqno();

        // 加入载荷信息
        string payload_str =
            _stream.read(min(TCPConfig::MAX_PAYLOAD_SIZE, (cur_receiver_window_size - _bytes_in_flight)));
        segment.payload() = Buffer(move(payload_str));

        /*
         * 同时满足三个条件才能发送带有FIN的报文
         * 从来没发送过 FIN。这是为了防止发送方在发送 FIN 包并接收到 FIN ack 包之后，循环用 FIN 包填充发送窗口的情况。
         * 输入字节流处于 EOF
         * window 减去 payload 大小后，仍然可以存放下 FIN
         */
        if (_stream.eof() and !_is_fin_sent and
            (cur_receiver_window_size > _bytes_in_flight + segment.payload().size())) {
            segment.header().fin = true;
            _is_fin_sent = true;
        }

        // TCP报文段全部设置完毕，如果到此报文段还是没有任何信息则不需要发送
        if (segment.length_in_sequence_space() == 0) {
            break;
        }

        // 如果没有正在等待确认回复的报文段，则重设超时计时器，超时重传时间也重置为初始值
        if (_outstanding_segments.empty()) {
            _retx_timeout = _initial_retransmission_timeout;
            _retransmission_timer = 0;
        }

        // 每一次发送都要检测，如果超时重传计时器没有启动，则启动
        if (!_is_timer_runing) {
            _retx_timeout = _initial_retransmission_timeout;
            _retransmission_timer = 0;
            _is_timer_runing = true;
        }

        // 发送报文段并将未确认的报文段持续追踪
        _segments_out.emplace(segment);
        _outstanding_segments.emplace(segment);

        // 记录有多少字节正在传输，下一个序列号应该是多少
        _bytes_in_flight += segment.length_in_sequence_space();
        _next_seqno += segment.length_in_sequence_space();

        // 如果设置了fin，代表这是一个 FIN报文段 则不需要继续发送
        if (segment.header().fin) {
            break;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // 将报文段中的32位ackno转换为64位绝对ackno
    size_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    // 接受并更新远端的窗口大小
    _receiver_windowsize = window_size;
    // 如果是无效的ack，则直接放弃
    if (abs_ackno > _next_seqno) {
        return;
    }
    while (!_outstanding_segments.empty()) {
        TCPSegment _outstand_segment = _outstanding_segments.front();
        // 正确的ackno应该是 大于等于 等待确认队列中最早报文段的序列号 + 报文段的长度
        if (abs_ackno >= (unwrap(_outstand_segment.header().seqno, _isn, _next_seqno) +
                           _outstand_segment.length_in_sequence_space())) {
            _outstanding_segments.pop();
            // 及时修改_byte_in_filght的值
            _bytes_in_flight -= _outstand_segment.length_in_sequence_space();
            // 收到了正确回应的ack，则代表发出的报文段成功到达
            // 将RTO置为初始值，重启超时重传计时器，连续重传计数器记为0
            _retx_timeout = _initial_retransmission_timeout;
            _retransmission_timer = 0;
            _consecutive_retransmissions = 0;
        } else {
            break;
        }
    }
    // 如果peer的窗口还可以接受报文段，则继续发送
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _retransmission_timer += ms_since_last_tick;
    // tick() 被调用且重传计时器走完所有的时间
    if (_retransmission_timer >= _retx_timeout and !_outstanding_segments.empty()) {
        // 重传最早(seqno最小的)未被确认的报文段
        _segments_out.emplace(_outstanding_segments.front());
        // 如果接收方还可接受，但是报文段还是超时超时未接受到确认，则说明网络可能拥堵了
        if (_receiver_windowsize > 0) {
            // 增加超时重传次数 并且将 超时重传时间倍增
            ++_consecutive_retransmissions;
            _retx_timeout *= 2;
        }
        // 将计时器重新置为0
        _retransmission_timer = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment empty_seg;
    empty_seg.header().seqno = next_seqno();
    _segments_out.emplace(empty_seg);
}
