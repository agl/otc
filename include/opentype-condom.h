#ifndef OPENTYPE_CONDOM_H_
#define OPENTYPE_CONDOM_H_

#include <stdint.h>
// For htons/ntohs
#include <arpa/inet.h>

// -----------------------------------------------------------------------------
// This is an interface for an abstract stream class which is used for writing
// the serialised results out.
// -----------------------------------------------------------------------------
class OTCStream {
 public:
  virtual ~OTCStream() { }

  virtual bool Write(const void *data, size_t length) = 0;
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
