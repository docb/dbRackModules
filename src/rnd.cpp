#include "rnd.h"
#include <atomic>
static std::atomic<uint64_t> counter {0};

void RND::reset(unsigned long long rseed) {
  if(rseed==0) {
    state=seed=(unsigned long long)(time(nullptr)+(counter++))*1234567;
  } else {
    state=seed=rseed*1234567;
  }
}