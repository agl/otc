#ifndef OTC_FILE_STREAM_H_
#define OTC_FILE_STREAM_H_

class FILEStream : public OTCStream {
 public:
  FILEStream(FILE *stream)
      : file_(stream) {
  }

  ~FILEStream() {
  }

  bool WriteRaw(const void *data, size_t length) {
    return fwrite(data, length, 1, file_) == 1;
  }

  void Seek(off_t position) {
    fseek(file_, position, SEEK_SET);
  }

  off_t Tell() const {
    return ftell(file_);
  }

 private:
  FILE* const file_;
};

#endif  // OTC_FILE_STREAM_H_
