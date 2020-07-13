#pragma once

#include <functional>
#include <string>
#include <vector>


namespace audio
{
    struct StereoPacket
    {
        float left;
        float right;

        StereoPacket operator+ (const StereoPacket &first) const
        {
            return StereoPacket{left+first.left, right+first.right};
        }
    };

struct AudioSinkInfo
{
    std::string name;
    std::string device_id;
    bool capture_device;
};

typedef std::vector<StereoPacket> AudioBuffer;
typedef std::function<bool(const AudioBuffer& buffer)> CaptureCallback;

std::vector<AudioSinkInfo> list_sinks();
AudioSinkInfo get_default_sink(bool capture);

// Captures data on the specified audiosink until the capture callback returns false
void capture_data(CaptureCallback callback, const AudioSinkInfo &sink);
}

