#ifndef OTC_MAXP_H_
#define OTC_MAXP_H_

struct OpenTypeMAXP {
  uint16_t num_glyphs;
  bool version_1;

  uint16_t max_points;
  uint16_t max_contours;
  uint16_t max_c_points;
  uint16_t max_c_contours;
  uint16_t max_c_components;
  uint16_t max_c_recursion;
};

#endif  // OTC_MAXP_H_
