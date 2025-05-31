#include <audio_loopback/loopback_recorder.h>
#include <cmath>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <thread>
#include <unistd.h> // For STDOUT_FILENO
#include <stdexcept>
#include <iostream>
#include <memory>
namespace {
    //pa_simple* pulse;
    static const pa_sample_spec SAMPLE_SPEC = {
            .format = PA_SAMPLE_FLOAT32,
            .rate = 44100,
            .channels = 2
    };
}

class PulseAudioWrapper
{
public:
    static constexpr uint32_t BUFSIZE = 256;
    
    static core::Result<std::unique_ptr<PulseAudioWrapper>, audio::AudioError> create() {
        // Use monitor device to capture system audio output (loopback)
        const char* monitor_device = "@DEFAULT_MONITOR@";  // PulseAudio will resolve to default sink monitor
        int error = 0;
        
        pa_simple* pulse_simple_api = pa_simple_new(
            NULL,               // Use the default server.
            "Visualizer",       // Our application's name.
            PA_STREAM_RECORD,
            monitor_device,     // Use monitor device for loopback
            "Audio Loopback",   // Description of our stream.
            &SAMPLE_SPEC,       // Our sample format.
            NULL,               // Use default channel map
            NULL,               // Use default buffering attributes.
            &error              // Error code.
        );
        
        if (!pulse_simple_api) {
            return core::Result<std::unique_ptr<PulseAudioWrapper>, audio::AudioError>::Err(
                audio::AudioError::InitializationFailed
            );
        }
        
        return core::Result<std::unique_ptr<PulseAudioWrapper>, audio::AudioError>::Ok(
            std::unique_ptr<PulseAudioWrapper>(new PulseAudioWrapper(pulse_simple_api))
        );
    }

    core::Result<audio::AudioBuffer, audio::AudioError> read_sink()
    {
        audio::AudioBuffer new_data;
        audio::StereoPacket buf[BUFSIZE];
        int32_t error;
        
        if (pa_simple_read(pulse_simple_api, buf, sizeof(buf), &error) < 0) {
            return core::Result<audio::AudioBuffer, audio::AudioError>::Err(
                audio::AudioError::ReadError
            );
        }

        std::copy(std::begin(buf), std::end(buf), std::back_inserter(new_data));
        return core::Result<audio::AudioBuffer, audio::AudioError>::Ok(new_data);
    }

    ~PulseAudioWrapper() {
        if (pulse_simple_api) {
            pa_simple_free(pulse_simple_api);
        }
    }

private:
    pa_simple *pulse_simple_api;
    
    // Private constructor to enforce using create() factory method
    explicit PulseAudioWrapper(pa_simple* api) : pulse_simple_api(api) {}
};



core::Result<core::Unit, audio::AudioError> record_loop(audio::CaptureCallback callback)
{
    auto pulse_result = PulseAudioWrapper::create();
    if (pulse_result.is_err()) {
        return core::Result<core::Unit, audio::AudioError>::Err(pulse_result.error());
    }
    
    auto pulse = std::move(pulse_result).unwrap();
    bool playing = true;
    
    while(playing)
    {
        auto read_result = pulse->read_sink();
        if (read_result.is_err()) {
            return core::Result<core::Unit, audio::AudioError>::Err(read_result.error());
        }
        
        playing = callback(read_result.unwrap());
    }
    
    return core::Result<core::Unit, audio::AudioError>::Ok(core::unit);
}


namespace audio
{
    core::Option<AudioSinkInfo> get_default_sink(bool capture)
    {
        AudioSinkInfo apa;
        apa.name = "Default PulseAudio Sink";
        apa.device_id = "default";
        apa.capture_device = capture;
        return core::Some(apa);
    }

    core::Result<std::vector<AudioSinkInfo>, AudioError> list_sinks()
    {
        // For now, just return the default sink
        std::vector<AudioSinkInfo> sinks;
        auto default_sink = get_default_sink(false);
        if (default_sink.is_some()) {
            sinks.push_back(default_sink.unwrap());
        }
        return core::Result<std::vector<AudioSinkInfo>, AudioError>::Ok(sinks);
    }

    static CaptureCallback the_callback;

    core::Result<core::Unit, AudioError> capture_data(CaptureCallback callback, const AudioSinkInfo &sink)
    {
        try {
            // Note: Since we're detaching the thread, we can't easily propagate errors
            // from within the thread. For now, we'll just ensure thread creation succeeds.
            // A better design might use a shared state or callback for error reporting.
            auto record_thread = std::thread([=] {
                auto result = record_loop(callback);
                if (result.is_err()) {
                    std::cerr << "Audio capture error in background thread" << std::endl;
                }
            });
            record_thread.detach();
            return core::Result<core::Unit, AudioError>::Ok(core::unit);
        } catch (const std::exception& e) {
            return core::Result<core::Unit, AudioError>::Err(AudioError::SystemError);
        }
    }
}