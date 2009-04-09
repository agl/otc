#include "otc.h"
#include "loca.h"
#include "maxp.h"
#include "head.h"

bool
otc_loca_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  // http://www.microsoft.com/typography/otspec/loca.htm

  // We can't do anything useful in validating this data except to ensure that
  // the values are monotonically increasing.

  OpenTypeLOCA *loca = new OpenTypeLOCA;
  file->loca = loca;

  if (!file->maxp || !file->head)
    return failure();

  const unsigned num_glyphs = file->maxp->num_glyphs;
  unsigned last_offset = 0;
  loca->offsets.resize(num_glyphs + 1);

  if (file->head->index_to_loc_format == 0) {
    // Note that the <= here (and below) is correct. There is one more offset
    // than the number of glyphs in order to give the length of the final
    // glyph.
    for (unsigned i = 0; i <= num_glyphs; ++i) {
      uint16_t offset;
      if (!table.ReadU16(&offset))
        return failure();
      if (offset < last_offset)
        return failure();
      last_offset = offset;
      loca->offsets[i] = offset * 2;
    }
  } else {
    for (unsigned i = 0; i <= num_glyphs; ++i) {
      uint32_t offset;
      if (!table.ReadU32(&offset))
        return failure();
      if (offset < last_offset)
        return failure();
      last_offset = offset;
      loca->offsets[i] = offset;
    }
  }

  return true;
}

bool
otc_loca_should_serialise(OpenTypeFile *file) {
  return file->loca;
}

bool
otc_loca_serialise(OTCStream *out, OpenTypeFile *file) {
  const OpenTypeLOCA *loca = file->loca;
  const OpenTypeMAXP *maxp = file->maxp;
  const OpenTypeHEAD *head = file->head;

  if (!maxp || !head)
    return failure();

  if (head->index_to_loc_format == 0) {
    for (unsigned i = 0; i < loca->offsets.size(); ++i) {
      if (!out->WriteU16(loca->offsets[i] >> 1))
        return failure();
    }
  } else {
    for (unsigned i = 0; i < loca->offsets.size(); ++i) {
      if (!out->WriteU32(loca->offsets[i]))
        return failure();
    }
  }

  return true;
}

void
otc_loca_free(OpenTypeFile *file) {
  delete file->loca;
}
