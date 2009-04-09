#include "otc.h"
#include "head.h"

bool
otc_head_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);
  file->head = new OpenTypeHEAD;

  // http://www.microsoft.com/typography/otspec/head.htm

  uint32_t version;
  if (!table.ReadU32(&version) ||
      !table.ReadU32(&file->head->revision)) {
    return failure();
  }

  if (version >> 16 != 1)
    return failure();

  // Skip the checksum adjustment
  table.Skip(4);

  uint32_t magic;
  if (!table.ReadTag(&magic) ||
      memcmp(&magic, "\x5F\x0F\x3C\xF5", 4)) {
    return failure();
  }

  if (!table.ReadU16(&file->head->flags))
    return failure();

  // We allow bits 0..4, 11..13
  file->head->flags &= 0x381f;

  if (!table.ReadU16(&file->head->ppem))
    return failure();

  // ppem must be in range and a power of two
  if (file->head->ppem < 16 ||
      file->head->ppem > 16384 ||
      ((file->head->ppem - 1) & file->head->ppem)) {
    return failure();
  }

  if (!table.ReadR64(&file->head->created) ||
      !table.ReadR64(&file->head->modified)) {
    return failure();
  }

  if (!table.ReadS16(&file->head->xmin) ||
      !table.ReadS16(&file->head->ymin) ||
      !table.ReadS16(&file->head->xmax) ||
      !table.ReadS16(&file->head->ymax)) {
    return failure();
  }

  if (!table.ReadU16(&file->head->mac_style))
    return failure();

  // We allow bits 0..6
  file->head->mac_style &= 0x3f;

  if (!table.ReadU16(&file->head->min_ppem))
    return failure();

  // We don't care about the font direction hint
  table.Skip(2);

  if (!table.ReadS16(&file->head->index_to_loc_format))
    return failure();
  if (file->head->index_to_loc_format > 1)
    return failure();

  int16_t glyph_data_format;
  if (!table.ReadS16(&glyph_data_format) ||
      glyph_data_format) {
    return failure();
  }

  return true;
}

bool
otc_head_should_serialise(OpenTypeFile *file) {
  return file->head;
}

bool
otc_head_serialise(OTCStream *out, OpenTypeFile *file) {
  if (!out->WriteU32(0x00010000) ||
      !out->WriteU32(file->head->revision) ||
      !out->WriteU32(0) ||  // check sum not filled in yet
      !out->WriteU32(0x5F0F3CF5) ||
      !out->WriteU16(file->head->flags) ||
      !out->WriteU16(file->head->ppem) ||
      !out->WriteR64(file->head->created) ||
      !out->WriteR64(file->head->modified) ||
      !out->WriteS16(file->head->xmin) ||
      !out->WriteS16(file->head->ymin) ||
      !out->WriteS16(file->head->xmax) ||
      !out->WriteS16(file->head->ymax) ||
      !out->WriteU16(file->head->mac_style) ||
      !out->WriteU16(file->head->min_ppem) ||
      !out->WriteS16(2) ||
      !out->WriteS16(file->head->index_to_loc_format) ||
      !out->WriteS16(0)) {
    return failure();
  }

  return true;
}

void
otc_head_free(OpenTypeFile *file) {
  delete file->head;
}
