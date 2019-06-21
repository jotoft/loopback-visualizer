#pragma once

#include <functional>
#include <string>
#include <vector>

typedef std::vector<uint16_t> AudioBuffer;
typedef std::function<void(const AudioBuffer& buffer)> CaptureCallback;

namespace audio
{


struct AudioSinkInfo
{
    std::string name;
    std::string device_id;
};

std::vector<AudioSinkInfo> list_sinks();
AudioSinkInfo get_default_sink();
void capture_data(CaptureCallback callback, const AudioSinkInfo &sink);
}

