#pragma once

#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <core/result.h>
#include <core/option.h>
#include <core/unit.h>

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

// Error types for audio operations
enum class AudioError {
    DeviceNotFound,
    InitializationFailed,
    ReadError,
    UnsupportedFormat,
    SystemError
};

// Returns list of available sinks, or error if enumeration fails
core::Result<std::vector<AudioSinkInfo>, AudioError> list_sinks();

// Returns default sink if available
core::Option<AudioSinkInfo> get_default_sink(bool capture);

// Captures data on the specified audiosink until the capture callback returns false
// Returns error if capture initialization fails
core::Result<core::Unit, AudioError> capture_data(CaptureCallback callback, const AudioSinkInfo &sink);

// Forward declaration
class AudioCapture;

// Modern capture API with built-in lock-free buffering
std::unique_ptr<AudioCapture> create_audio_capture(const AudioSinkInfo& sink);
}

