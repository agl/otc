#include <vector>

#include "otc.h"
#include "cmap.h"
#include "maxp.h"

struct CMAPSubtableHeader {
  uint16_t platform;
  uint16_t encoding;
  uint32_t offset;
  uint16_t format;
  uint32_t length;
};

struct Subtable314Range {
  uint16_t start_range;
  uint16_t end_range;
  int16_t id_delta;
  uint16_t id_range_offset;
  uint32_t id_range_offset_offset;
};

// The maximum number of groups in format 12 or 13 subtables. Set so that we'll
// allocate, at most, 8MB of memory when parsing these.
static const unsigned kMaxCMAPGroups = 699050;

static bool
parse_314(OpenTypeFile *file, const uint8_t *data, size_t length, uint16_t num_glyphs) {
  Buffer subtable(data, length);

  // 3.1.4 subtables are complex and, rather than expanding the whole thing and
  // recompacting it, we valid it and include it verbatim in the ouput.

  subtable.Skip(4);
  uint16_t language;
  if (!subtable.ReadU16(&language))
    return failure();
  if (language)
    return failure();

  uint16_t segcountx2, search_range, entry_selector, range_shift;
  if (!subtable.ReadU16(&segcountx2) ||
      !subtable.ReadU16(&search_range) ||
      !subtable.ReadU16(&entry_selector) ||
      !subtable.ReadU16(&range_shift)) {
    return failure();
  }

  if (segcountx2 & 1 || search_range & 1)
    return failure();
  const uint16_t segcount = segcountx2 >> 1;
  // There must be at least one segment according the spec.
  if (segcount < 1)
    return failure();

  // log2segcount is the maximal x s.t. 2^x < segcount
  unsigned log2segcount = 0;
  while (1u << (log2segcount + 1) < segcount)
    log2segcount++;

  const uint16_t expected_search_range = 2 * 1 << log2segcount;
  if (expected_search_range != search_range)
    return failure();

  if (entry_selector != log2segcount)
    return failure();

  const uint16_t expected_range_shift = segcountx2 - search_range;
  if (range_shift != expected_range_shift)
    return failure();

  std::vector<Subtable314Range> ranges(segcount);

  for (unsigned i = 0; i < segcount; ++i) {
    if (!subtable.ReadU16(&ranges[i].end_range))
      return failure();
  }

  uint16_t padding;
  if (!subtable.ReadU16(&padding))
    return failure();
  if (padding)
    return failure();

  for (unsigned i = 0; i < segcount; ++i) {
    if (!subtable.ReadU16(&ranges[i].start_range))
      return failure();
  }
  for (unsigned i = 0; i < segcount; ++i) {
    if (!subtable.ReadS16(&ranges[i].id_delta))
      return failure();
  }
  for (unsigned i = 0; i < segcount; ++i) {
    ranges[i].id_range_offset_offset = subtable.offset();
    if (!subtable.ReadU16(&ranges[i].id_range_offset))
      return failure();
    if (ranges[i].id_range_offset & 1)
      return failure();
  }

  // ranges must be ascending order, based on the end_code. Ranges may not
  // overlap.
  for (unsigned i = 1; i < segcount; ++i) {
    if (ranges[i].end_range <= ranges[i - 1].end_range)
      return failure();
    if (ranges[i].start_range <= ranges[i - 1].end_range)
      return failure();
  }

  // The last range must end at 0xffff
  if (ranges[segcount - 1].end_range != 0xffff)
    return failure();

  // A format 4 CMAP subtable is complex. To be safe we simulate a lookup of
  // each code-point defined in the table and make sure that they are all valid
  // glyphs and that we don't access anything out-of-bounds.
  for (unsigned i = 1; i < segcount; ++i) {
    for (unsigned cp = ranges[i].start_range; cp <= ranges[i].end_range; ++cp) {
      const uint16_t code_point = cp;
      if (ranges[i].id_range_offset == 0) {
        // this is explictly allowed to overflow in the spec
        const uint16_t glyph = code_point + ranges[i].id_delta;
        if (glyph >= num_glyphs)
          return failure();
      } else {
        const uint16_t range_delta = code_point - ranges[i].start_range;
        // this might seem odd, but it's true. The offset is relative to the
        // location of the offset value itself.
        const uint32_t glyph_id_offset = ranges[i].id_range_offset_offset +
                                         ranges[i].id_range_offset +
                                         range_delta * 2;
        // We need to be able to access a 16-bit value from this offset
        if (glyph_id_offset + 1 >= length)
          return failure();
        uint16_t glyph;
        memcpy(&glyph, data + glyph_id_offset, 2);
        glyph = ntohs(glyph);
        if (glyph >= num_glyphs)
          return failure();
      }
    }
  }

  // We accept the table.
  file->cmap->subtable_314_data = data;
  file->cmap->subtable_314_length = length;

  return true;
}

static bool
parse_31012(OpenTypeFile *file, const uint8_t *data, size_t length, uint16_t num_glyphs) {
  Buffer subtable(data, length);

  // Format 12 tables are simple. We parse these and fully serialise them
  // later.

  subtable.Skip(8);
  uint32_t language;
  if (!subtable.ReadU32(&language))
    return failure();
  if (language)
    return failure();

  uint32_t num_groups;
  if (!subtable.ReadU32(&num_groups))
    return failure();
  // There are 12 bytes of data per group. In order to keep some sanity, we'll
  // only allow ourselves to allocate 8MB of memory here. That means that
  // we'll allow, at most, 64 * 1024 * 1024 / 12 groups. Note that this is
  // still far in excess of the number of Unicode code-points currently
  // allocated.
  if (num_groups == 0 || num_groups > kMaxCMAPGroups)
    return failure();

  std::vector<OpenTypeCMAPSubtableRange> &groups = file->cmap->subtable_31012;
  groups.resize(num_groups);

  for (unsigned i = 0; i < num_groups; ++i) {
    if (!subtable.ReadU32(&groups[i].start_range) ||
        !subtable.ReadU32(&groups[i].end_range) ||
        !subtable.ReadU32(&groups[i].start_glyph_id)) {
      return failure();
    }

    // We conservatively limit all of the values to 2^30 which is vastly larger
    // than the number of Unicode code-points defined and might protect some
    // parsers from overflows
    if (groups[i].start_range > 0x40000000 ||
        groups[i].end_range > 0x40000000 ||
        groups[i].start_glyph_id > 0x40000000) {
      return failure();
    }

    // Also we assert that the glyph value is within range. Because the range
    // limits, above, we don't need to worry about overflow.
    if (groups[i].end_range + groups[i].start_glyph_id > num_glyphs)
      return failure();
  }

  // the groups must be sorted by start code and may not overlap
  for (unsigned i = 1; i < num_groups; ++i) {
    if (groups[i].start_range <= groups[i - 1].start_range)
      return failure();
    if (groups[i].start_range <= groups[i - 1].end_range)
      return failure();
  }

  return true;
}

static bool
parse_31013(OpenTypeFile *file, const uint8_t *data, size_t length, uint16_t num_glyphs) {
  Buffer subtable(data, length);

  // Format 13 tables are simple. We parse these and fully serialise them
  // later.

  subtable.Skip(8);
  uint16_t language;
  if (!subtable.ReadU16(&language))
    return failure();
  if (language)
    return failure();

  uint32_t num_groups;
  if (!subtable.ReadU32(&num_groups))
    return failure();

  // We limit the number of groups in the same way as in 3.10.12 tables. See
  // the comment there in
  if (num_groups == 0 || num_groups > kMaxCMAPGroups)
    return failure();

  std::vector<OpenTypeCMAPSubtableRange> &groups = file->cmap->subtable_31013;
  groups.resize(num_groups);

  for (unsigned i = 0; i < num_groups; ++i) {
    if (!subtable.ReadU32(&groups[i].start_range) ||
        !subtable.ReadU32(&groups[i].end_range) ||
        !subtable.ReadU32(&groups[i].start_glyph_id)) {
      return failure();
    }

    // We conservatively limit all of the values to 2^30 which is vastly larger
    // than the number of Unicode code-points defined and might protect some
    // parsers from overflows
    if (groups[i].start_range > 0x40000000 ||
        groups[i].end_range > 0x40000000 ||
        groups[i].start_glyph_id > 0x40000000) {
      return failure();
    }

    if (groups[i].start_glyph_id >= num_glyphs)
      return failure();
  }

  // the groups must be sorted by start code and may not overlap
  for (unsigned i = 1; i < num_groups; ++i) {
    if (groups[i].start_range <= groups[i - 1].start_range)
      return failure();
    if (groups[i].start_range <= groups[i - 1].end_range)
      return failure();
  }

  return true;
}

bool
otc_cmap_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);
  file->cmap = new OpenTypeCMAP;

  // http://www.microsoft.com/typography/otspec/cmap.htm
  // The charactor map table defines the mapping from code-point to glyph index
  // in the font. We only support Unicode (BMP and non-BMP) formats.

  uint16_t version, num_tables;
  if (!table.ReadU16(&version) ||
      !table.ReadU16(&num_tables))
    return failure();

  if (version != 0)
    return failure();

  std::vector<CMAPSubtableHeader> subtable_headers;

  // read the subtable headers
  for (unsigned i = 0; i < num_tables; ++i) {
    CMAPSubtableHeader subt;

    if (!table.ReadU16(&subt.platform) ||
        !table.ReadU16(&subt.encoding) ||
        !table.ReadU32(&subt.offset))
      return failure();

    subtable_headers.push_back(subt);
  }

  const size_t data_offset = table.offset();

  // make sure that all the offsets are valid.
  for (unsigned i = 0; i < num_tables; ++i) {
    if (subtable_headers[i].offset > 1024 * 1024 * 1024)
      return failure();
    if (subtable_headers[i].offset < data_offset ||
        subtable_headers[i].offset >= length)
      return failure();
  }

  // the format of the table is the first couple of bytes in the table. The
  // length of the table is in a format specific format afterwards.
  for (unsigned i = 0; i < num_tables; ++i) {
    table.set_offset(subtable_headers[i].offset);
    if (!table.ReadU16(&subtable_headers[i].format))
      return failure();

    if (subtable_headers[i].format == 4) {
      uint16_t length;
      if (!table.ReadU16(&length))
        return failure();
      subtable_headers[i].length = length;
    } else if (subtable_headers[i].format == 12 ||
               subtable_headers[i].format == 13) {
      table.Skip(2);
      if (!table.ReadU32(&subtable_headers[i].length))
        return failure();
    } else {
      subtable_headers[i].length = 0;
    }
  }

  // Now, verify that all the lengths are sane
  for (unsigned i = 0; i < num_tables; ++i) {
    if (!subtable_headers[i].length)
      continue;
    if (subtable_headers[i].length > 1024 * 1024 * 1024)
      return failure();
    // We know that both the offset and length are < 1GB, so the following
    // addition doesn't overflow
    const uint32_t end_byte = subtable_headers[i].offset + subtable_headers[i].length;
    if (end_byte > length)
      return failure();
  }

  // we grab the number of glyphs in the file from the maxp table to make sure
  // that the character map isn't referencing anything beyound this range.
  if (!file->maxp)
    return failure();
  const uint16_t num_glyphs = file->maxp->num_glyphs;

  // We only support a subset of the possible character map tables. Microsoft
  // 'strongly recommends' that everyone supports the Unicode BMP table with
  // the UCS-4 table for non-BMP glyphs. We'll pass the following subtables:
  //   Platform ID   Encoding ID  Format
  //   3             1            4       (Unicode BMP)
  //   3             10           12      (Unicode UCS-4)
  //   3             10           13      (UCS-4 Fallback mapping)

  for (unsigned i = 0; i < num_tables; ++i) {
    if (subtable_headers[i].platform != 3)
      continue;
    if (subtable_headers[i].encoding == 1) {
      if (subtable_headers[i].format == 4) {
        if (!parse_314(file, data + subtable_headers[i].offset, subtable_headers[i].length, num_glyphs))
          return failure();
      }
    } else if (subtable_headers[i].encoding == 10) {
      if (subtable_headers[i].format == 12) {
        if (!parse_31012(file, data + subtable_headers[i].offset, subtable_headers[i].length, num_glyphs))
          return failure();
      } else if (subtable_headers[i].format == 13) {
        if (!parse_31013(file, data + subtable_headers[i].offset, subtable_headers[i].length, num_glyphs))
          return failure();
      }
    }
  }

  return true;
}

bool
otc_cmap_should_serialise(OpenTypeFile *file) {
  return file->cmap;
}

bool
otc_cmap_serialise(OTCStream *out, OpenTypeFile *file) {
  const bool have_314 = file->cmap->subtable_314_data;
  const bool have_31012 = file->cmap->subtable_31012.size();
  const bool have_31013 = file->cmap->subtable_31013.size();
  const unsigned num_subtables = static_cast<unsigned>(have_314) +
                                 static_cast<unsigned>(have_31012) +
                                 static_cast<unsigned>(have_31013);
  const off_t table_start = out->Tell();

  if (!out->WriteU16(0) ||
      !out->WriteU16(num_subtables)) {
    return failure();
  }

  const off_t record_offset = out->Tell();
  out->Pad(num_subtables * 8);

  const off_t offset_314 = out->Tell();
  if (have_314) {
    if (!out->Write(file->cmap->subtable_314_data, file->cmap->subtable_314_length))
      return failure();
  }

  const off_t offset_31012 = out->Tell();
  if (have_31012) {
    std::vector<OpenTypeCMAPSubtableRange> &groups = file->cmap->subtable_31012;
    const unsigned num_groups = groups.size();
    if (!out->WriteU16(12) ||
        !out->WriteU16(0) ||
        !out->WriteU32(num_groups * 12 + 14) ||
        !out->WriteU32(0) ||
        !out->WriteU32(num_groups)) {
      return failure();
    }

    for (unsigned i = 0; i < num_groups; ++i) {
      if (!out->WriteU32(groups[i].start_range) ||
          !out->WriteU32(groups[i].end_range) ||
          !out->WriteU32(groups[i].start_glyph_id)) {
        return failure();
      }
    }
  }

  const off_t offset_31013 = out->Tell();
  if (have_31013) {
    std::vector<OpenTypeCMAPSubtableRange> &groups = file->cmap->subtable_31013;
    const unsigned num_groups = groups.size();
    if (!out->WriteU16(13) ||
        !out->WriteU16(0) ||
        !out->WriteU32(num_groups * 12 + 14) ||
        !out->WriteU32(0) ||
        !out->WriteU32(num_groups)) {
      return failure();
    }

    for (unsigned i = 0; i < num_groups; ++i) {
      if (!out->WriteU32(groups[i].start_range) ||
          !out->WriteU32(groups[i].end_range) ||
          !out->WriteU32(groups[i].start_glyph_id)) {
        return failure();
      }
    }
  }

  const off_t table_end = out->Tell();
  // We might have hanging bytes from the above's checksum which the OTCStream
  // then merges into the table of offsets.
  OTCStream::ChecksumState saved_checksum = out->SaveChecksumState();
  out->ResetChecksum();

  // Now seek back and write the table of offsets
  out->Seek(record_offset);
  if (have_314) {
    if (!out->WriteU16(3) ||
        !out->WriteU16(1) ||
        !out->WriteU32(offset_314 - table_start)) {
      return failure();
    }
  }

  if (have_31012) {
    if (!out->WriteU16(3) ||
        !out->WriteU16(10) ||
        !out->WriteU32(offset_31012 - table_start)) {
      return failure();
    }
  }

  if (have_31013) {
    if (!out->WriteU16(3) ||
        !out->WriteU16(10) ||
        !out->WriteU32(offset_31013 - table_start)) {
      return failure();
    }
  }

  out->Seek(table_end);
  out->RestoreChecksum(saved_checksum);
  return true;
}

void
otc_cmap_free(OpenTypeFile *file) {
  delete file->cmap;
}
