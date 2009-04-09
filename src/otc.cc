#include <vector>
#include <map>
#include <algorithm>

#include <stdint.h>
#include <sys/types.h>

#include <stdlib.h>

#include "otc.h"

#define F(name, capname) \
  bool otc_##name##_parse(OpenTypeFile *file, const uint8_t *data, size_t length); \
  bool otc_##name##_should_serialise(OpenTypeFile *file); \
  bool otc_##name##_serialise(OTCStream *out, OpenTypeFile *file); \
  void otc_##name##_free(OpenTypeFile *fiel);
FOR_EACH_TABLE_TYPE
#undef F

struct OpenTypeTable {
  uint32_t tag;
  uint32_t chksum;
  uint32_t offset;
  uint32_t length;
};

// Round a value up to the nearest multiple of 4. Note that this can overflow
// and return zero.
template<typename T>
T Round4(T value) {
  return (value + 3) & ~3;
}

static uint32_t
tag(const char *tag_str) {
  uint32_t ret;
  memcpy(&ret, tag_str, 4);
  return ret;
}

struct OutputTable {
  uint32_t tag;
  size_t offset;
  size_t length;

  static bool SortByTag(const OutputTable& a, const OutputTable& b) {
    const uint32_t atag = ntohl(a.tag);
    const uint32_t btag = ntohl(b.tag);
    return atag < btag;
  }
};

bool
otc_process(OTCStream *output, const uint8_t *data, size_t length) {
  Buffer file(data, length);

  // we disallow all files > 1GB in size for sanity.
  if (length > 1024 * 1024 * 1024)
    return failure();

  OpenTypeFile header;
  if (!file.ReadU32(&header.version))
    return failure();
  if (header.version >> 16 != 1)
    return failure();

  if (!file.ReadU16(&header.num_tables) ||
      !file.ReadU16(&header.search_range) ||
      !file.ReadU16(&header.entry_selector) ||
      !file.ReadU16(&header.range_shift))
    return failure();

  // search_range is (Maximum power of 2 <= numTables) x 16. Thus, to avoid
  // overflow num_tables is, at most, 2^16 / 16 = 2^12
  if (header.num_tables >= 4096 || header.num_tables < 1)
    return failure();

  unsigned max_pow2 = 0;
  while (1u << (max_pow2 + 1) < header.num_tables)
    max_pow2++;
  const uint16_t expected_search_range = (1u << max_pow2) << 4;
  if (header.search_range != expected_search_range)
    return failure();

  // entry_selector is Log2(maximum power of 2 <= numTables)
  if (header.entry_selector != max_pow2)
    return failure();

  // range_shift is NumTables x 16-searchRange. We know that 16*num_tables
  // doesn't over flow because we range checked it above. Also, we know that
  // it's > header.search_range by construction of search_range.
  const uint32_t expected_range_shift = 16 * header.num_tables - header.search_range;
  if (header.range_shift != expected_range_shift)
    return failure();

  // Next up is the list of tables.
  std::vector<OpenTypeTable> tables;

  for (unsigned i = 0; i < header.num_tables; ++i) {
    OpenTypeTable table;
    if (!file.ReadTag(&table.tag) ||
        !file.ReadU32(&table.chksum) ||
        !file.ReadU32(&table.offset) ||
        !file.ReadU32(&table.length)) {
      return failure();
    }

    tables.push_back(table);
  }

  const size_t data_offset = file.offset();

  for (unsigned i = 0; i < header.num_tables; ++i) {
    // the tables must be sorted by tag (when taken as big-endian numbers).
    // This also remove the possibility of duplicate tables.
    if (i) {
      const uint32_t this_tag = ntohl(tables[i].tag);
      const uint32_t prev_tag = ntohl(tables[i - 1].tag);
      if (this_tag <= prev_tag)
        return failure();
    }

    // tables must be 4-byte aligned
    if (tables[i].offset & 3)
      return failure();

    // and must be within the file
    if (tables[i].offset < data_offset || tables[i].offset >= length)
      return failure();
    // disallow all tables with a length > 1GB
    if (tables[i].length > 1024 * 1024 * 1024)
      return failure();
    // since we required that the file be < 1GB in length, and that the table
    // length is < 1GB, the following addtion doesn't overflow
    const uint32_t end_byte = Round4(tables[i].offset + tables[i].length);
    if (!end_byte || end_byte > length)
      return failure();
  }

  // we could check that the tables are disjoint, but it's now technically
  // invalid for them to overlap according to the spec.

  std::map<uint32_t, OpenTypeTable> table_map;
  for (unsigned i = 0; i < header.num_tables; ++i)
    table_map[tables[i].tag] = tables[i];

  const struct {
    uint32_t tag;
    bool (*parse) (OpenTypeFile *otf, const uint8_t *data, size_t length);
    bool (*serialise) (OTCStream *out, OpenTypeFile *file);
    bool (*should_serialise) (OpenTypeFile *file);
    void (*free) (OpenTypeFile *file);
    bool required;
  } table_parsers[] = {
    { tag("maxp"), otc_maxp_parse, otc_maxp_serialise, otc_maxp_should_serialise, otc_maxp_free, 1 },
    { tag("cmap"), otc_cmap_parse, otc_cmap_serialise, otc_cmap_should_serialise, otc_cmap_free, 1 },
    { tag("head"), otc_head_parse, otc_head_serialise, otc_head_should_serialise, otc_head_free, 1 },
    { tag("hhea"), otc_hhea_parse, otc_hhea_serialise, otc_hhea_should_serialise, otc_hhea_free, 1 },
    { tag("hmtx"), otc_hmtx_parse, otc_hmtx_serialise, otc_hmtx_should_serialise, otc_hmtx_free, 1 },
    { tag("name"), otc_name_parse, otc_name_serialise, otc_name_should_serialise, otc_name_free, 1 },
    { tag("OS/2"), otc_os2_parse, otc_os2_serialise, otc_os2_should_serialise, otc_os2_free, 1 },
    { tag("post"), otc_post_parse, otc_post_serialise, otc_post_should_serialise, otc_post_free, 1 },
    { tag("loca"), otc_loca_parse, otc_loca_serialise, otc_loca_should_serialise, otc_loca_free, 1 },
    { tag("glyf"), otc_glyf_parse, otc_glyf_serialise, otc_glyf_should_serialise, otc_glyf_free, 1 },
    { 0, NULL, NULL, NULL, 0 },
  };

  for (unsigned i = 0; ; ++i) {
    if (table_parsers[i].parse == NULL)
      break;
    const std::map<uint32_t, OpenTypeTable>::const_iterator
      it = table_map.find(table_parsers[i].tag);

    if (it == table_map.end()) {
      if (table_parsers[i].required)
        return failure();
      continue;
    }

    if (!table_parsers[i].parse(&header, data + it->second.offset, it->second.length))
      return failure();
  }

  unsigned num_output_tables = 0;
  for (unsigned i = 0; ; ++i) {
    if (table_parsers[i].parse == NULL)
      break;

    if (table_parsers[i].should_serialise(&header))
      num_output_tables++;
  }

  max_pow2 = 0;
  while (1u << (max_pow2 + 1) < num_output_tables)
    max_pow2++;
  const uint16_t output_search_range = (1 << max_pow2) << 4;

  if (!output->WriteU32(0x00010000) ||
      !output->WriteU16(num_output_tables) ||
      !output->WriteU16(output_search_range) ||
      !output->WriteU16(max_pow2) ||
      !output->WriteU16((num_output_tables << 4) - output_search_range)) {
    return failure();
  }

  const size_t table_record_offset = output->Tell();
  output->Pad(16 * num_output_tables);

  std::vector<OutputTable> out_tables;

  for (unsigned i = 0; ; ++i) {
    if (table_parsers[i].parse == NULL)
      break;

    if (!table_parsers[i].should_serialise(&header))
      continue;

    OutputTable out;
    out.tag = table_parsers[i].tag;
    out.offset = output->Tell();

    if (!table_parsers[i].serialise(output, &header))
      return failure();

    const size_t end_offset = output->Tell();
    out.length = end_offset - out.offset;
    out_tables.push_back(out);

    // align tables to four bytes
    output->Pad((4 - (end_offset & 3)) % 4);
  }

  const size_t end_of_file = output->Tell();

  // Need to sort the output tables for inclusion in the file
  std::sort(out_tables.begin(), out_tables.end(), OutputTable::SortByTag);
  output->Seek(table_record_offset);

  for (unsigned i = 0; i < out_tables.size(); ++i) {
    if (!output->WriteTag(out_tables[i].tag) ||
        !output->WriteU32(0) ||
        !output->WriteU32(out_tables[i].offset) ||
        !output->WriteU32(out_tables[i].length)) {
      return failure();
    }
  }

  output->Seek(end_of_file);

  for (unsigned i = 0; ; ++i) {
    if (table_parsers[i].parse == NULL)
      break;

    table_parsers[i].free(&header);
  }

  return true;
}
