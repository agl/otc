#ifndef OTC_H_
#define OTC_H_

#include <stdint.h>
#include <string.h>

#include "opentype-condom.h"

#define OTC_DEBUG

#if defined(OTC_DEBUG)
#include <stdlib.h>
#endif

static bool
failure() {
#if defined(OTC_DEBUG)
  abort();
#endif
  return false;
}

// -----------------------------------------------------------------------------
// Buffer helper class
//
// This class perform some trival buffer operations while checking for
// out-of-bounds errors. As a family they return false if anything is amiss,
// updating the current offset otherwise.
// -----------------------------------------------------------------------------
class Buffer {
 public:
  Buffer(const uint8_t *buffer, size_t length)
      : buffer_(buffer),
        length_(length),
        offset_(0) { }

  bool Skip(size_t n_bytes) {
    if (offset_ + n_bytes > length_)
      return failure();
    offset_ += n_bytes;
    return true;
  }

  bool ReadU8(uint8_t *value) {
    if (offset_ + 1 > length_)
      return failure();
    *value = buffer_[offset_];
    offset_++;
    return true;
  }

  bool ReadU16(uint16_t *value) {
    if (offset_ + 2 > length_)
      return failure();
    memcpy(value, buffer_ + offset_, sizeof(uint16_t));
    *value = ntohs(*value);
    offset_ += 2;
    return true;
  }

  bool ReadS16(int16_t *value) {
    return ReadU16(reinterpret_cast<uint16_t*>(value));
  }

  bool ReadU32(uint32_t *value) {
    if (offset_ + 4 > length_)
      return failure();
    memcpy(value, buffer_ + offset_, sizeof(uint32_t));
    *value = ntohl(*value);
    offset_ += 4;
    return true;
  }

  bool ReadTag(uint32_t *value) {
    if (offset_ + 4 > length_)
      return failure();
    memcpy(value, buffer_ + offset_, sizeof(uint32_t));
    offset_ += 4;
    return true;
  }

  bool ReadR64(uint64_t *value) {
    if (offset_ + 8 > length_)
      return failure();
    memcpy(value, buffer_ + offset_, sizeof(uint64_t));
    offset_ += 8;
    return true;
  }

  size_t offset() const { return offset_; }

  void set_offset(size_t newoffset) { offset_ = newoffset; }

private:
  const uint8_t *const buffer_;
  const size_t length_;
  size_t offset_;
};

#define FOR_EACH_TABLE_TYPE \
  F(cmap, CMAP) \
  F(head, HEAD) \
  F(hhea, HHEA) \
  F(hmtx, HMTX) \
  F(maxp, MAXP) \
  F(name, NAME) \
  F(os2, OS2) \
  F(post, POST) \
  F(loca, LOCA) \
  F(glyf, GLYF)

#define F(name, capname) struct OpenType##capname;
FOR_EACH_TABLE_TYPE
#undef F

// http://www.microsoft.com/typography/otspec/otff.htm
struct OpenTypeFile {
  OpenTypeFile() {
#define F(name, capname) name = NULL;
    FOR_EACH_TABLE_TYPE
#undef F
  }
  uint32_t version;
  uint16_t num_tables;
  uint16_t search_range;
  uint16_t entry_selector;
  uint16_t range_shift;

#define F(name, capname) OpenType##capname *name;
FOR_EACH_TABLE_TYPE
#undef F
};

#endif  // OTC_H_
