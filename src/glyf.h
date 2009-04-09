#ifndef OTC_GLYF_H_
#define OTC_GLYF_H_

#include <vector>
#include <utility>

struct OpenTypeGLYF {
  std::vector<std::pair<const uint8_t*, size_t> > iov;
};

#endif  // OTC_GLYF_H_
