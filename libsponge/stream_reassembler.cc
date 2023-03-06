#include "stream_reassembler.hh"

//#include <algorithm>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _stream_reassembler(capacity), _bitmap(capacity, false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (_unassembled_bytes + _output.buffer_size() >= _capacity) {
        return;
    }

    size_t begin_index = index;
    size_t end_index = index + data.size();

    size_t first_unassembled = _output.bytes_written();         // 第一个没有被整合的字节索引
    size_t first_unacceptable = _capacity + first_unassembled;  // 第一个不能进去字节重组器的
    // 第一个不可以接受的index是容量加上已写入的字节数，因为已写入的字节数代表着可以被读取或者已经被读取。则可以为即将到来的子串腾出空间来缓冲
    // 如果传进来的字符索引小于第一个没被整合的字符索引，说明有重合的字符
    if (begin_index < first_unassembled) {
        begin_index = first_unassembled;
    }
    // 如果结束字符索引大于第一个不可接受字符的索引，说明当前的字符超过cap了，需要先截取掉
    if (end_index > first_unacceptable) {
        end_index = first_unacceptable;
    }
    // 如果这个字符是eof，标记eof
    if (eof) {
        _eof_index = end_index;
        _eof_sign = true;
    }

    /* _sr[begin_index] = make_shared<StrStore>(begin_index, end_index, 
                string_view(data.substr(begin_index - index, end_index - begin_index)));
    _unassembled_bytes += (end_index - begin_index); */
    for (size_t i = begin_index; i < end_index; i++) {
        if (!_bitmap[i - first_unassembled]) {
            _bitmap[i - first_unassembled] = true;
            _stream_reassembler[i - first_unassembled] = data[i - index];
            _unassembled_bytes++;
        }
    }

    string temp;
    while (_bitmap.front()) {
        //temp += _stream_reassembler.front();
        temp.push_back(_stream_reassembler.front());
        _bitmap.pop_front();
        _stream_reassembler.pop_front();
        _stream_reassembler.emplace_back('\0');
        _bitmap.emplace_back(false);
    }
    // size_t idx = begin_index;
    // while (_sr.count(idx) && idx == _expected_index) {
    //     size_t write_res = _output.write(_sr.at(idx)->str.data());
    //     _unassembled_bytes -= write_res;
    //     size_t tmp = idx;
    //     idx = idx + _sr.at(idx)->end_index - _sr.at(idx)->begin_index;                     //字符串size()的时间复杂度也是 O(n)
    //     _sr.erase(tmp);
    // }
    // _expected_index = _output.bytes_written();
    if (!temp.empty()) {
        _output.write(temp);
        _unassembled_bytes -= temp.size();
    }

    if (_output.bytes_written() == _eof_index && _eof_sign && _unassembled_bytes == 0) {
        _output.end_input();
    }
}
size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }