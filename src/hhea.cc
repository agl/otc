#include "otc.h"
#include "maxp.h"
#include "hhea.h"

bool
otc_hhea_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);
  OpenTypeHHEA *hhea = new OpenTypeHHEA;
  file->hhea = hhea;

  uint32_t version;
  if (!table.ReadU32(&version))
    return failure();
  if (version >> 16 != 1)
    return failure();

  if (!table.ReadS16(&hhea->ascent) ||
      !table.ReadS16(&hhea->descent) ||
      !table.ReadS16(&hhea->linegap) ||
      !table.ReadU16(&hhea->adv_width_max) ||
      !table.ReadS16(&hhea->min_lsb) ||
      !table.ReadS16(&hhea->min_rsb) ||
      !table.ReadS16(&hhea->x_max_extent) ||
      !table.ReadS16(&hhea->caret_slope_rise) ||
      !table.ReadS16(&hhea->caret_slope_run) ||
      !table.ReadS16(&hhea->caret_offset)) {
    return failure();
  }

  if (hhea->linegap < 0)
    hhea->linegap = 0;

  // skip the reserved bytes
  table.Skip(8);

  int16_t data_format;
  if (!table.ReadS16(&data_format))
    return failure();
  if (data_format)
    return failure();

  if (!table.ReadU16(&hhea->num_hmetrics))
    return failure();

  if (!file->maxp)
    return failure();

  if (hhea->num_hmetrics > file->maxp->num_glyphs)
    return failure();

  return true;
}

bool
otc_hhea_should_serialise(OpenTypeFile *file) {
  return file->hhea;
}

bool
otc_hhea_serialise(OTCStream *out, OpenTypeFile *file) {
  const OpenTypeHHEA *hhea = file->hhea;

  if (!out->WriteU32(0x00010000) ||
      !out->WriteS16(hhea->ascent) ||
      !out->WriteS16(hhea->descent) ||
      !out->WriteS16(hhea->linegap) ||
      !out->WriteU16(hhea->adv_width_max) ||
      !out->WriteS16(hhea->min_lsb) ||
      !out->WriteS16(hhea->min_rsb) ||
      !out->WriteS16(hhea->x_max_extent) ||
      !out->WriteS16(hhea->caret_slope_rise) ||
      !out->WriteS16(hhea->caret_slope_run) ||
      !out->WriteS16(hhea->caret_offset) ||
      !out->WriteR64(0) ||
      !out->WriteS16(0) ||
      !out->WriteU16(hhea->num_hmetrics)) {
    return failure();
  }

  return true;
}

void
otc_hhea_free(OpenTypeFile *file) {
  delete file->hhea;
}
