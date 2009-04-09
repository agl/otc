#ifndef OTC_CMAP_H_
#define OTC_CMAP_H_

struct OpenTypeCMAPSubtableRange {
  uint32_t start_range;
  uint32_t end_range;
  uint32_t start_glyph_id;
};

struct OpenTypeCMAP {
  OpenTypeCMAP()
      : subtable_314_data(NULL),
        subtable_314_length(0) {
  }

  const uint8_t *subtable_314_data;
  size_t subtable_314_length;
  std::vector<OpenTypeCMAPSubtableRange> subtable_31012;
  std::vector<OpenTypeCMAPSubtableRange> subtable_31013;
};

#endif
