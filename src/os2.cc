#include "otc.h"
#include "os2.h"

bool
otc_os2_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  // There are a lot of members in the OS/2 table, most of which we don't care
  // about. Because of this, we record the pointer and length for the table and
  // just write that out when it comes time to serialise it again.

  file->os2 = new OpenTypeOS2;
  file->os2->data = data;
  file->os2->length = length;

  return true;
}

bool
otc_os2_should_serialise(OpenTypeFile *file) {
  return file->os2;
}

bool
otc_os2_serialise(OTCStream *out, OpenTypeFile *file) {
  const OpenTypeOS2 *os2 = file->os2;

  if (!out->Write(os2->data, os2->length))
    return failure();


  return true;
}

void
otc_os2_free(OpenTypeFile *file) {
  delete file->os2;
}
