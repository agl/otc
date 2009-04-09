#ifndef OTC_POST_H_
#define OTC_POST_H_

#include <vector>
#include <map>
#include <string>

struct OpenTypePOST {
  uint32_t version;
  uint32_t italic_angle;
  uint16_t underline;
  uint16_t underline_thickness;
  uint32_t is_fixed_pitch;

  std::vector<uint16_t> glyph_name_index;
  std::vector<std::string> names;
};

#endif  // OTC_POST_H_
