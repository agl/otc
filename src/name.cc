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

  static const char *const strings[] = {
      "Derived font data",  // 0: copyright
      "OTC derived font",  // 1: the name the user sees
      "Unspecified", // 2: face weight
      "UniqueID",  // 3: unique id
      "OTC derivied font", // 4: human readable name
      "Version 0.0",  // 5: version
      "False",  // 6: postscript name
      NULL,  // 7: trademark data
      "OTC",  // 8: foundary
      "OTC",  // 9: designer
  };

  unsigned num_strings = 0;
  for (unsigned i = 0; i < sizeof(strings) / sizeof(char *); ++i) {
    if (strings[i])
      num_strings++;
  }

  if (!out->WriteU16(0) ||  // version
      !out->WriteU16(num_strings) ||  // count
      !out->WriteU16(6 + num_strings * 12)) {  // string data offset
    return failure();
  }

  unsigned current_offset = 0;
  for (unsigned i = 0; i < sizeof(strings) / sizeof(char *); ++i) {
    if (!strings[i])
      continue;

    const size_t len = strlen(strings[i]) * 2;
    if (!out->WriteU16(3) ||  // Windows
        !out->WriteU16(1) ||  // Roman
        !out->WriteU16(0x0409) ||  // US English
        !out->WriteU16(i) ||
        !out->WriteU16(len) ||
        !out->WriteU16(current_offset)) {
      return failure();
    }

    current_offset += len;
  }

  for (unsigned i = 0; i < sizeof(strings) / sizeof(char *); ++i) {
    if (!strings[i])
      continue;

    const size_t len = strlen(strings[i]);
    for (size_t j = 0; j < len; ++j) {
      uint16_t v = strings[i][j];
      if (!out->WriteU16(v))
        return failure();
    }
  }

  return true;
}

void
otc_name_free(OpenTypeFile *file) {
}
