#include "otc.h"

bool
otc_name_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  return true;
}

bool
otc_name_should_serialise(OpenTypeFile *file) {
  return true;
}

bool
otc_name_serialise(OTCStream *out, OpenTypeFile *file) {
  // NAME is a required table, but we don't want anything to do with it. Thus,
  // we don't bother parsing it and we just serialise an empty name table.

  if (!out->WriteU16(0) ||  // version
      !out->WriteU16(0) ||  // count
      !out->WriteU16(4)) {  // string data offset
    return failure();
  }

  return true;
}

void
otc_name_free(OpenTypeFile *file) {
}
