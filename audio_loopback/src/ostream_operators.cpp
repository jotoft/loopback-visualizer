//
// Created by Johan on 2019-06-18.
//
#include <audio_loopback/ostream_operators.h>

std::ostream &operator<<(std::ostream &os, const audio::AudioSinkInfo &info)
{
  os << "name: " << info.name;
  return os;
}