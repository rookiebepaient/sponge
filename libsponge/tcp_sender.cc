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

uint64_t TCPSender::bytes_in_flight() const {
    return _bytes_in_flight;

    /*
    uint64_t result = 0;
    for (size_t i = 0; i < _seg_timer_pair.size(); i++) {
        result += _seg_timer_pair[i].first.length_in_sequence_space();
    }
    return result;
     */
}

void TCPSender::fill_window() {
    //当不知道peer的窗口大小时，默认对方的窗口大小为1
    size_t cur_receiver_window_size = _receiver_windowsize == 0 ? 1 : _receiver_windowsize;
    while (cur_receiver_window_size > _bytes_in_flight) {
        TCPSegment segment;
        // SYN位只在建立连接时才会置为1，在数据传输和关闭连接时用不到SYN位
        if (!_is_syn_sent) {
            segment.header().syn = true;
            _is_syn_sent = true;
        }
        //用next_seqno创建报文段, next_seqno为报文段首字节的字节流编号
        segment.header().seqno = next_seqno();

        //加入载荷信息
        string payload_str =
            _stream.read(min(TCPConfig::MAX_PAYLOAD_SIZE, (cur_receiver_window_size - _bytes_in_flight)));
        segment.payload() = Buffer(move(payload_str));

        if (_stream.eof() and !_is_fin_sent and
            (cur_receiver_window_size > _bytes_in_flight + segment.payload().size())) {
            segment.header().fin = true;
            //这里其实不是很严谨,感觉上不是很严谨
            _is_fin_sent = true;
        }

        // TCP报文段全部设置完毕，如果到此报文段还是没有任何信息则不需要发送
        if (segment.length_in_sequence_space() == 0) {
            break;
        }

        // 如果没有正在等待的数据包，则重设超时计时器
        if (_outstanding_segments.empty()) {
            _retx_timeout = _initial_retransmission_timeout;
            _retransmission_timer = 0;
        }

        //_retx_timeout = _initial_retransmission_timeout;
        //启动超时重传定时器
        if (_is_timer_ruuning) {
            _retx_timeout = _initial_retransmission_timeout;
            _retransmission_timer = 0;
            _is_timer_ruuning = true;
        }

        //发送报文段并将未确认的报文段持续追踪
        _segments_out.push(segment);
        _outstanding_segments.push_back(segment);

        //记录
        _bytes_in_flight += segment.length_in_sequence_space();
        _next_seqno += segment.length_in_sequence_space();

        //如果设置了fin，则不需要继续发送
        if (segment.header().fin) {
            break;
        }
    }

    /*
    size_t not_allowed_sent = _receiver_windowsize;   //not allowed to be sent
    size_t already_sent = bytes_in_flight();   //already sent but not acked,  ! ! ! maybe a bug here cause type range is
    not pair

    while (not_allowed_sent > already_sent) {
        TCPSegment syn_seg;

        // status 2 SYN_SENT, 将SYN位设置为1
        if (!_next_seqno) {
            syn_seg.header().syn = true;
            syn_seg.header().seqno = _isn;
        }
        else{
            syn_seg.header().seqno = next_seqno();
        }

        //set the payload
        string payload_str = _stream.read(min(TCPConfig::MAX_PAYLOAD_SIZE, (not_allowed_sent - already_sent -
    syn_seg.header().syn - syn_seg.header().fin))); syn_seg.payload() =  Buffer(move(payload_str));

        //status 3 SYN_ACKED
        if (_stream.eof() && !_eof_sign && (_receiver_windowsize > syn_seg.length_in_sequence_space())) {
            syn_seg.header().fin = true;
            _eof_sign = true;
        }

        // //if it's an empty seg,don't send and break the loop
        if (!syn_seg.length_in_sequence_space()) {
            break;
        }

        //sent the sent and make a copy
        _segments_out.push(syn_seg);
        _seg_timer_pair.emplace_back(make_pair(syn_seg, Timer {_retx_timeout}));   //sent and make a copy of already
    sent seg size_t temp_test_int = _segments_out.back().length_in_sequence_space(); DUMMY_CODE(temp_test_int);
        //recored and update the parameters
        already_sent += _segments_out.back().length_in_sequence_space();
        _next_seqno += _segments_out.back().length_in_sequence_space();
    }
    */
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    //将报文段中的32位ackno转换为64位ackno
    size_t _abs_ackno = unwrap(ackno, _isn, _next_seqno);
    //接受并更新远端的窗口大小
    _receiver_windowsize = window_size;
    //如果是无效的ack，则直接放弃
    if (_abs_ackno > _next_seqno) {
        return;
    }
    while (!_outstanding_segments.empty()) {
        TCPSegment _outstand_segment = _outstanding_segments.front();
        //如果是无效的ack，则直接放弃
        if (_abs_ackno >= (unwrap(_outstand_segment.header().seqno, _isn, _next_seqno) +
                           _outstand_segment.length_in_sequence_space())) {
            _outstanding_segments.pop_front();
            //及时修改_byte_in_filght的值
            _bytes_in_flight -= _outstand_segment.length_in_sequence_space();
            //收到了正确回应的ack，则代表发出的报文段成功到达
            //将RTO置为初始值，重启超时重传计时器，连续重传计数器记为0
            _retx_timeout = _initial_retransmission_timeout;
            _retransmission_timer = 0;
            _consecutive_retransmissions = 0;
        } else {
            break;
        }
    }
    //如果peer的窗口还可以接受报文段，则继续发送
    fill_window();

    /*
    uint64_t unwrap_ackno = unwrap(ackno, _isn, _next_seqno);
    _receiver_windowsize = window_size == 0 ? 1 : window_size;
    _is_zero_ws = window_size == 0 ? true : false;
    //size_t left = unwrap(_seg_timer_pair.front().first.length_in_sequence_space(), _isn, _next_seqno) + 1;
    size_t right = _next_seqno;
    if (unwrap_ackno <= right) {
        while ((!_seg_timer_pair.empty()) &&
                unwrap_ackno >= (unwrap(_seg_timer_pair.front().first.header().seqno, _isn, _next_seqno)
                + _seg_timer_pair.front().first.length_in_sequence_space())) {
            _seg_timer_pair.pop_front();
        }
        _consecutive_retransmissions = 0;
        if (_retx_timeout != _initial_retransmission_timeout) {
            _retx_timeout = _initial_retransmission_timeout;
            for (size_t i = 0; i < _seg_timer_pair.size(); i++) {
                _seg_timer_pair[i].second.set_rto(_retx_timeout);
            }
        }
        if (_receiver_windowsize) {
            fill_window();
        }
    }
    */
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _retransmission_timer += ms_since_last_tick;

    // tick() 被调用且重传计时器走完所有的时间
    if (_retransmission_timer >= _retx_timeout and !_outstanding_segments.empty()) {
        //重传最早(seqno最小的)未被确认的报文段
        _segments_out.push(_outstanding_segments.front());
        //如果接收方还可接受，但是报文段还是超时超时未接受到确认，则说明网络可能拥堵了
        if (_receiver_windowsize > 0) {
            ++_consecutive_retransmissions;
            _retx_timeout *= 2;
        }
        //将计时器重新置为0
        _retransmission_timer = 0;
    }
    /*
    //if tick is called and timer has expired
    _seg_timer_pair.front().second.change_rto(ms_since_last_tick);
    unsigned int cur_rto = _seg_timer_pair.front().second.get_rto();
    if (!cur_rto && !_seg_timer_pair.empty()) {
        _segments_out.push(_seg_timer_pair.front().first);
        // if receiver windowsize is not 0
        if (_receiver_windowsize) {
            _consecutive_retransmissions++;
            _retx_timeout = _is_zero_ws ? _retx_timeout : _retx_timeout << 1;
            for (size_t i = 0; i < _seg_timer_pair.size(); i++) {
                _seg_timer_pair[i].second.set_rto(_retx_timeout);
            }
        }
    }
    */
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment empty_seg;
    empty_seg.header().seqno = next_seqno();
    _segments_out.push(empty_seg);
}
