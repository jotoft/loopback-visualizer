#include <audio_loopback/loopback_recorder.h>

#include <mmdeviceapi.h>
#include <assert.h>
#include <locale>
#include <codecvt>
#include <iostream>
#include <functiondiscoverykeys_devpkey.h>
#include <algorithm>
#include <Audioclient.h>
#include <chrono>
#include <thread>
#include <numeric>


namespace
{
const IID MY_IID_IAudioClient = __uuidof(IAudioClient);
}

namespace audio
{

class ComInitializer
{
public:
    ComInitializer()
    {
      auto result = CoInitializeEx(NULL, COINIT_MULTITHREADED);
      assert(SUCCEEDED(result));
    }
    ~ComInitializer()
    {
      CoUninitialize();
    }
};

static ComInitializer _windows_com_initializer;

class StringConverter
{
public:
    StringConverter(LPWSTR windows_string)
        : m_windows_string(windows_string)
    {

    }
    operator std::string()
    {
      using convert_type = std::codecvt_utf8<wchar_t>;
      std::wstring_convert<convert_type, wchar_t> converter;

      std::string result = converter.to_bytes(m_windows_string);
      return result;
    }
private:
    LPWSTR m_windows_string;
};

std::ostream &operator<<(std::ostream &os, const tWAVEFORMATEX &waveformatex)
{
  os << "wFormatTag: " << std::hex << waveformatex.wFormatTag << std::dec << " nChannels: " << waveformatex.nChannels
     << " nSamplesPerSec: "
     << waveformatex.nSamplesPerSec << " nAvgBytesPerSec: " << waveformatex.nAvgBytesPerSec << " nBlockAlign: "
     << waveformatex.nBlockAlign << " wBitsPerSample: " << waveformatex.wBitsPerSample << " cbSize: "
     << waveformatex.cbSize;
  return os;
}

class Device
{
public:
    Device(IMMDevice *device, bool is_capture)
        : m_device(device), m_capture(is_capture)
    {
    }

    void start_capture(CaptureCallback callback, AudioBuffer &buffer)
    {
      IAudioClient *audioClient;
      m_device->Activate(MY_IID_IAudioClient, CLSCTX_INPROC_SERVER, NULL, reinterpret_cast<void **>(&audioClient));

      WAVEFORMATEX *format;
      audioClient->GetMixFormat(&format);
      std::cout << *format;


      WAVEFORMATEXTENSIBLE *format_ex = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(format);
      std::cout << std::hex << format_ex->SubFormat.Data1 << std::dec << std::endl;

      const REFERENCE_TIME ten_ms_in_hns = 10 * (10000);
      // Must be 0 in shared mode
      const REFERENCE_TIME periodicity = 0;
      // Set the loopback flag if we are not using a capture device
      const DWORD stream_flags = m_capture ? 0 : AUDCLNT_STREAMFLAGS_LOOPBACK;
      auto hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                        stream_flags,
                                        ten_ms_in_hns,
                                        periodicity,
                                        format,
                                        nullptr);
      uint32_t frames;
      hr = audioClient->GetBufferSize(&frames);
      std::cout << frames << std::endl;

      assert(SUCCEEDED(hr));


      IAudioCaptureClient *captureClient;
      audioClient->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void **>(&captureClient));
      audioClient->Start();
      bool keep_capturing = true;
      while (keep_capturing) {
        // Really short sleep to reduce CPU load of this thread,
        // might be able to increase it but want to keep latency minimal too.
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        std::vector<StereoPacket> audio_copy;
        uint32_t packet_size;
        do {
          DWORD flags;
          uint32_t num_frames_in_buffer;

          captureClient->GetNextPacketSize(&packet_size);
          audio_copy.resize(packet_size);

          StereoPacket *audio_capture_buffer;
          captureClient->GetBuffer(reinterpret_cast<uint8_t **>(&audio_capture_buffer),
                                   &num_frames_in_buffer,
                                   &flags,
                                   nullptr,
                                   nullptr);
          for (int i = 0; i < packet_size; i++) {
            audio_copy[i] = audio_capture_buffer[i];
          }
          captureClient->ReleaseBuffer(packet_size);
          if (audio_copy.size() > 0 && packet_size > 0) {
            keep_capturing = callback(audio_copy);
          }
        }
        while (packet_size != 0);
      }
    }

    operator AudioSinkInfo()
    {
      LPWSTR device_id;
      assert(m_device != nullptr);
      m_device->GetId(&device_id);

      IPropertyStore *store;
      m_device->OpenPropertyStore(STGM_READ, &store);

      PROPVARIANT friendly_name;
      PropVariantInit(&friendly_name);
      store->GetValue(PKEY_Device_FriendlyName, &friendly_name);
      store->Release();
      audio::AudioSinkInfo info{StringConverter(friendly_name.pwszVal),
                                StringConverter(device_id),
                                m_capture};
      return info;
    }
    ~Device()
    {
      assert(m_device != nullptr);
      m_device->Release();
      m_device = nullptr;
    };
private:
    IMMDevice *m_device;
    IAudioClient *m_client;
    bool m_capture;
};

class DeviceEnumerator
{
public:

    class DeviceCollection
    {
    public:
        DeviceCollection(IMMDeviceCollection *collection, bool capture)
            : m_collection(collection), m_capture(capture)
        {}

        std::vector<std::shared_ptr<Device>> get_devices()
        {
          UINT device_count;
          auto result = m_collection->GetCount(&device_count);
          assert(SUCCEEDED(result));
          std::vector<std::shared_ptr<Device>> devices;

          for (UINT i = 0; i < device_count; i++) {
            IMMDevice *immDevice;
            auto res = m_collection->Item(i, &immDevice);
            assert(SUCCEEDED(res));
            devices.push_back(std::make_shared<Device>(immDevice, m_capture));
          }
          return devices;
        }

        ~DeviceCollection()
        {
          assert(m_collection != nullptr);
          m_collection->Release();
          m_collection = nullptr;
        }

    private:
        IMMDeviceCollection *m_collection;
        bool m_capture;
    };
public:
    DeviceEnumerator()
    {

      auto result =
          CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_enumerator));
      assert(SUCCEEDED(result));
    }
    ~DeviceEnumerator()
    {
      m_enumerator->Release();
    }

    Device get_default()
    {
      IMMDevice *device;
      m_enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
      bool capture = false;
      return Device(device, capture);
    }

    Device get_default_capture()
    {
        IMMDevice *device;
        m_enumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia, &device);
        bool capture = true;
        return Device(device, capture);
    }

    DeviceCollection get_render_collection(bool capture)
    {
      IMMDeviceCollection *imm_collection;
      auto type = capture ? eCapture : eRender;
      auto result = m_enumerator->EnumAudioEndpoints(type, DEVICE_STATE_ACTIVE, &imm_collection);
      return DeviceCollection(imm_collection, capture);
    }


private:
    IMMDeviceEnumerator *m_enumerator;

    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
};

AudioSinkInfo get_default_sink(bool capture)
{
  DeviceEnumerator enumerator;

  if (capture)
    return enumerator.get_default_capture();
  else
    return enumerator.get_default();
};

class DataCapture
{
public:
    DataCapture(std::shared_ptr<Device> dev, CaptureCallback callback)
        :
        m_capturedevice(dev),
        m_callback(callback)
    {
    };
    void start_capture()
    {
      m_capturethread = std::thread([this]
                                    { m_capturedevice->start_capture(m_callback, m_captureBuffer); });
      m_capturethread.detach();
    }

private:
    std::shared_ptr<Device> m_capturedevice;
    std::thread m_capturethread;
    AudioBuffer m_captureBuffer;
    CaptureCallback m_callback;
};

std::vector<std::shared_ptr<DataCapture>> active_devices;

void capture_data(CaptureCallback callback, const AudioSinkInfo &sink)
{
  DeviceEnumerator enumerator;
    std::cout << sink.name << " searching" << std::endl;
  std::vector<std::shared_ptr<Device>> devices = enumerator.get_render_collection(sink.capture_device).get_devices();

  auto device = std::find_if(devices.begin(), devices.end(), [&sink](std::shared_ptr<Device> dev) -> bool
  {
      AudioSinkInfo devInfo = *dev;
      return devInfo.device_id == sink.device_id;
  });

  if (device != devices.end()) {
    std::cout << sink.name << " found" << std::endl;
    std::shared_ptr<Device> device_ptr = *device;
    auto capture = std::make_shared<DataCapture>(device_ptr, callback);
    active_devices.push_back(capture);
    capture->start_capture();
  }
}

std::vector<AudioSinkInfo> list_sinks()
{
  DeviceEnumerator enumerator;
  std::vector<AudioSinkInfo> sinks;
  DeviceEnumerator::DeviceCollection collection(enumerator.get_render_collection(false));
  auto devices = collection.get_devices();
  for (auto &device : devices) {
    sinks.push_back(*device);
  }
  return sinks;
}
}