#ifndef OTC_HHEA_H_
#define OTC_HHEA_H_

struct OpenTypeHHEA {
  int16_t ascent;
  int16_t descent;
  int16_t linegap;
  uint16_t adv_width_max;
  int16_t min_lsb;
  int16_t min_rsb;
  int16_t x_max_extent;
  int16_t caret_slope_rise;
  int16_t caret_slope_run;
  int16_t caret_offset;
  uint16_t num_hmetrics;
};

#endif  // OTC_HHEA_H_
