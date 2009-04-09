#ifndef OTC_HMTX_H_
#define OTC_HMTX_H_

#include <vector>
#include <utility>

struct OpenTypeHMTX {
  std::vector<std::pair<uint16_t, int16_t> > metrics;
  std::vector<int16_t> lsbs;
};

#endif  // OTC_HMTX_H_
