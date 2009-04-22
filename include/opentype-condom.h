#ifndef OPENTYPE_CONDOM_H_
#define OPENTYPE_CONDOM_H_

#include <stdint.h>
#include <assert.h>
#include <string.h>

#include <arpa/inet.h>  // For htons/ntohs
#include <algorithm>  // For stl::min

// -----------------------------------------------------------------------------
// This is an interface for an abstract stream class which is used for writing
// the serialised results out.
// -----------------------------------------------------------------------------
class OTCStream {
 public:
  OTCStream() {
    ResetChecksum();
  }

  virtual ~OTCStream() { }

  // This should be implemented to perform the actual write.
  virtual bool WriteRaw(const void *data, size_t length) = 0;

  bool Write(const void *data, size_t length) {
    const size_t orig_length = length;

    size_t offset = 0;
    if (chksum_buffer_offset_) {
      const size_t l =
        std::min(length, static_cast<size_t>(4) - chksum_buffer_offset_);
      memcpy(chksum_buffer_ + chksum_buffer_offset_, data, l);
      chksum_buffer_offset_ += l;
      offset += l;
      length -= l;
    }

    if (chksum_buffer_offset_ == 4) {
      chksum_ += ntohl(*reinterpret_cast<const uint32_t*>(chksum_buffer_));
      chksum_buffer_offset_ = 0;
    }

    while (length >= 4) {
      chksum_ += ntohl(*reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(data) + offset));
      length -= 4;
      offset += 4;
    }

    if (length) {
      assert(chksum_buffer_offset_ == 0);
      memcpy(chksum_buffer_, reinterpret_cast<const uint8_t*>(data) + offset, length);
      chksum_buffer_offset_ = length;
    }

    return WriteRaw(data, orig_length);
  }

  virtual void Seek(off_t position) = 0;
  virtual off_t Tell() const = 0;

  virtual void Pad(size_t bytes) {
    static const uint32_t zero = 0;
    while (bytes >= 4) {
      WriteTag(zero);
      bytes -= 4;
    }
    while (bytes) {
      static const uint8_t zero = 0;
      Write(&zero, 1);
      bytes--;
    }
  }

  bool WriteU16(uint16_t v) {
    v = htons(v);
    return Write(&v, sizeof(v));
  }

  bool WriteS16(int16_t v) {
    v = htons(v);
    return Write(&v, sizeof(v));
  }

  bool WriteU32(uint32_t v) {
    v = htonl(v);
    return Write(&v, sizeof(v));
  }

  bool WriteR64(uint64_t v) {
    return Write(&v, sizeof(v));
  }

  bool WriteTag(uint32_t v) {
    return Write(&v, sizeof(v));
  }

  void ResetChecksum() {
    chksum_ = 0;
    chksum_buffer_offset_ = 0;
  }

  uint32_t chksum() const {
    assert(chksum_buffer_offset_ == 0);
    return chksum_;
  }

  struct ChecksumState {
    uint32_t chksum;
    uint8_t chksum_buffer[4];
    unsigned chksum_buffer_offset;
  };

  ChecksumState SaveChecksumState() const {
    ChecksumState s;
    s.chksum = chksum_;
    s.chksum_buffer_offset = chksum_buffer_offset_;
    memcpy(s.chksum_buffer, chksum_buffer_, 4);

    return s;
  }

  void RestoreChecksum(const ChecksumState &s) {
    assert(chksum_buffer_offset_ == 0);
    chksum_ += s.chksum;
    chksum_buffer_offset_ = s.chksum_buffer_offset;
    memcpy(chksum_buffer_, s.chksum_buffer, 4);
  }

 protected:
  uint32_t chksum_;
  uint8_t chksum_buffer_[4];
  unsigned chksum_buffer_offset_;
};

// -----------------------------------------------------------------------------
// Process a given OpenType file and write out a sanitised version
//   output: a pointer to an object implementing the OTCStream interface. The
//     sanitisied output will be written to this. In the even of a failure,
//     partial output may have been written.
//   input: the OpenType file
//   length: the size, in bytes, of |input|
// -----------------------------------------------------------------------------
bool otc_process(OTCStream *output, const uint8_t *input, size_t length);

#endif  // OPENTYPE_CONDOM_H_
