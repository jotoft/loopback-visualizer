#include <iostream>
#include <audio_loopback/ostream_operators.h>
#include <audio_loopback/loopback_recorder.h>

class Initializer
{
public:
    Initializer()
    {
      std::ios_base::sync_with_stdio(false);
    }
};

void audio_callback(const std::vector<uint16_t>& buffer)
{
  std::cout << "called back" << std::endl;
}

int main()
{
  Initializer _init;
  //for(auto& sink: audio::list_sinks())
  //{
  //  std::cout << sink << std::endl;
  //}

  std::cout << "Using Default Sink" << std::endl;
  std::cout << audio::get_default_sink() << std::endl;
  audio::AudioSinkInfo default_sink = audio::get_default_sink();

  audio::capture_data(&audio_callback, default_sink);

  return 0;
}