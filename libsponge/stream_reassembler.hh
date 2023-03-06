#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <deque>
#include <unordered_map>
#include <string_view>
#include <memory>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.
    ByteStream _output;            //!< The reassembled in-order byte stream
    size_t _capacity;              //!< The maximum number of bytes
    size_t _eof_index{0};          // eof的位置
    size_t _unassembled_bytes{0};  //没有被重组的字节数
    size_t _eof_sign{false};
    size_t _expected_index{0};         //期望到来的index
    std::deque<char> _stream_reassembler;  //流重组器
    std::deque<bool> _bitmap;              //标志该位置是否有数据
    /* struct StrStore {
      size_t begin_index;
      size_t end_index;
      std::string_view str;
      StrStore(size_t begin, size_t end, std::string_view sv)
        : begin_index(begin), end_index(end), str(sv) {}
    };
    std::unordered_map<int, std::shared_ptr<StrStore>> _sr{}; */
  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
