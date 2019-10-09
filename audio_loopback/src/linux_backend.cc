#include <audio_loopback/loopback_recorder.h>
#include <cmath>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <thread>
#include <unistd.h> // For STDOUT_FILENO
#include <stdexcept>
#include <iostream>
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
    PulseAudioWrapper() {
        pulse_simple_api = pa_simple_new(NULL,               // Use the default server.
                              "Visualizer",           // Our application's name.
                              PA_STREAM_RECORD,
                                         NULL,               // Use the default device. //TODO CHECK
                              "Record",            // Description of our stream.
                              &SAMPLE_SPEC,                // Our sample format.
                              NULL,               // Use default channel map
                              NULL,               // Use default buffering attributes.
                              NULL               // Ignore error code.
        );
    }

    audio::AudioBuffer read_sink()
    {
        audio::AudioBuffer new_data;
        audio::StereoPacket buf[BUFSIZE];
        int32_t error;
        if (pa_simple_read(pulse_simple_api, buf, sizeof(buf), &error) < 0)
            std::cout << "error" << error;
            //throw std::runtime_error("error reading from pulse audio");

        std::copy(std::begin(buf), std::end(buf), std::back_inserter(new_data));
        return new_data;
    }


     ~PulseAudioWrapper(){
        pa_simple_free(pulse_simple_api);
    }
private:
    pa_simple *pulse_simple_api;
};



void record_loop(audio::CaptureCallback callback)
{
    static PulseAudioWrapper pulse;
    bool playing = true;
    while(playing)
    {
        auto result = pulse.read_sink();
        callback(result);
    }
}


namespace audio
{
    AudioSinkInfo get_default_sink()
    {
        AudioSinkInfo apa;
        apa.name = "Apa";
        apa.device_id ="kamu";
        return apa;
    }

    static CaptureCallback the_callback;

    void capture_data(CaptureCallback callback, const AudioSinkInfo &sink)
    {

        auto record_thread = std::thread([=] {record_loop(callback);});
        record_thread.detach();
    }


}