#include "otc.h"
#include "maxp.h"

bool
otc_maxp_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypeMAXP *maxp = new OpenTypeMAXP;
  file->maxp = maxp;

  // http://www.microsoft.com/typography/otspec/maxp.htm
  //
  // We actually care rather little about the MAXP table since most of it
  // relates to the limits of the hinting code which we'll be removing anyway.

  uint32_t version;
  if (!table.ReadU32(&version))
    return failure();

  if (version >> 16 > 1)
    return failure();

  if (!table.ReadU16(&maxp->num_glyphs))
    return failure();

  if (version >> 16 == 1) {
    maxp->version_1 = true;
    if (!table.ReadU16(&maxp->max_points) ||
        !table.ReadU16(&maxp->max_contours) ||
        !table.ReadU16(&maxp->max_c_points) ||
        !table.ReadU16(&maxp->max_c_contours)) {
      return failure();
    }

    // Skip over the fields relating to hinting byte code
    table.Skip(14);
    if (!table.ReadU16(&maxp->max_c_components) ||
        !table.ReadU16(&maxp->max_c_recursion)) {
      return failure();
    }
  } else {
    maxp->version_1 = false;
  }

  return true;
}

bool
otc_maxp_should_serialise(OpenTypeFile *file) {
  return file->maxp;
}

bool
otc_maxp_serialise(OTCStream *out, OpenTypeFile *file) {
  const OpenTypeMAXP *maxp = file->maxp;

  if (!out->WriteU32(maxp->version_1 ? 0x00010000 : 0x00005000) ||
      !out->WriteU16(maxp->num_glyphs)) {
    return failure();
  }

  if (!maxp->version_1)
    return true;

  if (!out->WriteU16(maxp->max_points) ||
      !out->WriteU16(maxp->max_contours) ||
      !out->WriteU16(maxp->max_c_points) ||
      !out->WriteU16(maxp->max_c_contours) ||
      !out->WriteU16(1) ||  // max zones
      !out->WriteU16(0) ||  // max twilight points
      !out->WriteU16(0) ||  // max storage
      !out->WriteU16(0) ||  // max function defs
      !out->WriteU16(0) ||  // max instruction defs
      !out->WriteU16(0) ||  // max stack elements
      !out->WriteU16(0) ||  // max instruction byte count
      !out->WriteU16(maxp->max_c_components) ||
      !out->WriteU16(maxp->max_c_recursion)) {
    return failure();
  }

  return true;
}

void
otc_maxp_free(OpenTypeFile *file) {
  delete file->maxp;
}
