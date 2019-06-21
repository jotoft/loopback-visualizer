#ifndef VISUALIZER_OSTREAM_OPERATORS_H
#define VISUALIZER_OSTREAM_OPERATORS_H
#include <ostream>
#include "loopback_recorder.h"

std::ostream &operator<<(std::ostream &os, const audio::AudioSinkInfo &info);

#endif //VISUALIZER_OSTREAM_OPERATORS_H
