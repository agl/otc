#include "otc.h"
#include "maxp.h"
#include "hhea.h"
#include "hmtx.h"

bool
otc_hmtx_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);
  OpenTypeHMTX *hmtx = new OpenTypeHMTX;
  file->hmtx = hmtx;

  if (!file->hhea || !file->maxp)
    return failure();

  // |num_hmetrics| is a uint16_t, so it's bounded < 65536. This limits that
  // amount of memory that we'll allocate for this to a sane amount.
  const unsigned num_hmetrics = file->hhea->num_hmetrics;

  if (num_hmetrics > file->maxp->num_glyphs)
    return failure();
  const unsigned num_lsbs = file->maxp->num_glyphs - num_hmetrics;

  for (unsigned i = 0; i < num_hmetrics; ++i) {
    uint16_t adv;
    int16_t lsb;
    if (!table.ReadU16(&adv) ||
        !table.ReadS16(&lsb)) {
      return failure();
    }

    if (adv > file->hhea->adv_width_max)
      return failure();
    if (lsb < file->hhea->min_lsb)
      return failure();

    hmtx->metrics.push_back(std::make_pair(adv, lsb));
  }

  for (unsigned i = 0; i < num_lsbs; ++i) {
    int16_t lsb;
    if (!table.ReadS16(&lsb))
      return failure();

    if (lsb < file->hhea->min_lsb)
      return failure();

    hmtx->lsbs.push_back(lsb);
  }

  return true;
}

bool
otc_hmtx_should_serialise(OpenTypeFile *file) {
  return file->hmtx;
}

bool
otc_hmtx_serialise(OTCStream *out, OpenTypeFile *file) {
  const OpenTypeHMTX *hmtx = file->hmtx;

  for (unsigned i = 0; i < hmtx->metrics.size(); ++i) {
    if (!out->WriteU16(hmtx->metrics[i].first) ||
        !out->WriteS16(hmtx->metrics[i].second)) {
      return failure();
    }
  }

  for (unsigned i = 0; i < hmtx->lsbs.size(); ++i) {
    if (!out->WriteS16(hmtx->lsbs[i]))
      return failure();
  }

  return true;
}

void
otc_hmtx_free(OpenTypeFile *file) {
  delete file->hmtx;
}
