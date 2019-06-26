#ifndef VISUALIZER_FILTERS_H
#define VISUALIZER_FILTERS_H
#include <vector>

namespace audio
{
namespace filters
{
std::vector<float> lowpass(const std::vector<float> to_filter);
}
}


#endif //VISUALIZER_FILTERS_H
