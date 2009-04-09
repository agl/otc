#include "otc.h"
#include "loca.h"
#include "glyf.h"
#include "maxp.h"

bool
otc_glyf_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  // http://www.microsoft.com/typography/otspec/glyf.htm

  // The GLYF table is pretty complicated. Thankfully, we can skip most of the
  // complexity. For simple glyphs, we just want to remove the hinting
  // bytecode. For composite glyphs, we can pass it directly since we'll
  // already have removed the hinting code from the individual components.

  if (!file->maxp || !file->loca)
    return failure();

  OpenTypeGLYF *glyf = new OpenTypeGLYF;
  file->glyf = glyf;

  const unsigned num_glyphs = file->maxp->num_glyphs;
  std::vector<uint32_t> &offsets = file->loca->offsets;

  if (offsets.size() != num_glyphs + 1)
    return failure();

  std::vector<uint32_t> resulting_offsets(num_glyphs + 1);
  uint32_t current_offset = 0;

  for (unsigned i = 0; i < num_glyphs; ++i) {
    const unsigned gly_offset = offsets[i];
    // The LOCA parser checks that these values are monotonic
    const unsigned gly_length = offsets[i + 1] - offsets[i];
    if (!gly_length) {
      // this glyph has no outline (e.g. the space charactor)
      resulting_offsets[i] = current_offset;
      continue;
    }

    if (gly_offset >= length)
      return failure();
    // Since these are unsigned types, the compiler is not allowed to assume
    // that they never overflow.
    if (gly_offset + gly_length < gly_offset)
      return failure();
    if (gly_offset + gly_length > length)
      return failure();

    table.set_offset(gly_offset);
    int16_t num_contours, xmin, ymin, xmax, ymax;
    if (!table.ReadS16(&num_contours) ||
        !table.ReadS16(&xmin) ||
        !table.ReadS16(&ymin) ||
        !table.ReadS16(&xmax) ||
        !table.ReadS16(&ymax)) {
      return failure();
    }

    if (xmin > xmax || ymin > ymax)
      return failure();

    unsigned size_reduction = 0;

    if (num_contours >= 0) {
      // this is a simple glyph and might contain bytecode

      // skip the end-points array
      table.Skip(num_contours * 2);
      uint16_t bytecode_length;
      if (!table.ReadU16(&bytecode_length))
        return failure();

      // when we remove the bytecode we need to shorten the glyph by this
      // amount.
      size_reduction = bytecode_length;

      // enqueue three vectors: the glyph data up to the bytecode length, then
      // a pointer to a static uint16_t 0 to overwrite the length, followed by
      // the rest of the glyph.
      const unsigned gly_header_length = 10 + num_contours * 2 + 2;
      glyf->iov.push_back(std::make_pair(data + gly_offset, gly_header_length - 2));
      glyf->iov.push_back(std::make_pair((const uint8_t*) "\x00\x00", 2));
      if (gly_length < (gly_header_length + bytecode_length))
        return failure();
      glyf->iov.push_back(std::make_pair(data + gly_offset + gly_header_length + bytecode_length,
                                         gly_length - (gly_header_length + bytecode_length)));
    } else {
      // it's a composite glyph without any bytecode. Enqueue the whole thing
      glyf->iov.push_back(std::make_pair(data + gly_offset, gly_length));
    }

    resulting_offsets[i] = current_offset;
    if (size_reduction > gly_length)
      return failure();
    unsigned new_size = gly_length - size_reduction;
    if (new_size < 14)
      return failure();
    // glyphs must be four byte aligned
    const unsigned padding = (4 - (new_size & 3)) % 4;
    if (padding) {
      glyf->iov.push_back(std::make_pair((const uint8_t*) "\x00\x00\x00\x00", padding));
      new_size += padding;
    }
    current_offset += new_size;
  }
  resulting_offsets[num_glyphs] = current_offset;

  file->loca->offsets = resulting_offsets;

  return true;
}

bool
otc_glyf_should_serialise(OpenTypeFile *file) {
  return file->glyf;
}

bool
otc_glyf_serialise(OTCStream *out, OpenTypeFile *file) {
  const OpenTypeGLYF *glyf = file->glyf;

  for (unsigned i = 0; i < glyf->iov.size(); ++i) {
    if (!out->Write(glyf->iov[i].first, glyf->iov[i].second))
      return failure();
  }

  return true;
}

void
otc_glyf_free(OpenTypeFile *file) {
  delete file->glyf;
}
