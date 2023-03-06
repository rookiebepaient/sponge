#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    //以下是使用 BufferList的优化版本
    if (_end_input) {
        return 0;
    }
    size_t write_size = min(data.size(), _capacity - _buffer.size());
    _bytes_written += write_size;
    _buffer.append(BufferList(move(string().assign(data.begin(), data.begin() + write_size))));
    return write_size;
    /*
    size_t dataSize = data.size();
    size_t bytesWritting = 0;
    if (_end_input) {
        return 0;
    }
    for(size_t i = 0; i < dataSize; i++) {
        if (remaining_capacity() != 0) {
            _byte_stream.push(data[i]);
            _bytes_written++;
            bytesWritting++;
        }
        else {
            break;
        }
    }
    return bytesWritting;
    */
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t peek_out_size = min(len, _buffer.size());
    string s = _buffer.concatenate();
    return string().assign(s.begin(), s.begin() + peek_out_size);

    /*
    queue<unsigned char> temp = _byte_stream;
    string peekOutput = "";
    if (len > temp.size()) {
        return peekOutput;
    }
    for(size_t i = 0; i < len; i++) {
        peekOutput += temp.front();
        temp.pop();
    }
    return peekOutput;
    */
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t pop_size = min(len, _buffer.size());
    _bytes_read += pop_size;
    _buffer.remove_prefix(pop_size);
    return;
    /*
    if (len > _byte_stream.size()) {
        return;
    }
    read(len);
    */
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    const auto result = peek_output(len);
    pop_output(len);
    return result;

    /*
    string readStr = "";
    //new change here 2022/11/18 if len > size ,then should read bytes all
    size_t read_size = min(len, _byte_stream.size());
    for(size_t i = 0; i < read_size; i++) {
        readStr += _byte_stream.front();
        _byte_stream.pop();
        _bytes_read++;
    }
    return readStr;
    */
}

void ByteStream::end_input() { _end_input = true; }

bool ByteStream::input_ended() const { return _end_input; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.size() == 0; }

bool ByteStream::eof() const { return buffer_empty() and input_ended(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer.size(); }
