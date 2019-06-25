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
    Device(IMMDevice *device)
        : m_device(device)
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

      const REFERENCE_TIME ten_ms_in_hns = 20 * (10000);
      // Must be 0 in shared mode
      const REFERENCE_TIME periodicity = 0;
      auto hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                        AUDCLNT_STREAMFLAGS_LOOPBACK,
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
        StereoPacket *audio_capture_buffer;
        uint32_t num_frames_in_buffer;
        DWORD flags;
        uint32_t packet_size;
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        std::vector<StereoPacket> copy;
        do {
          captureClient->GetNextPacketSize(&packet_size);
          //std::cout << copy.size() << std::endl;
          //std::cout << "packet size " << packet_size << std::endl;

          copy.resize(packet_size);

          //std::cout << copy.size() << std::endl;

          captureClient->GetBuffer(reinterpret_cast<uint8_t **>(&audio_capture_buffer),
                                   &num_frames_in_buffer,
                                   &flags,
                                   nullptr,
                                   nullptr);
          for (int i = 0; i < packet_size; i++) {
            copy[i] = audio_capture_buffer[i];
          }
          captureClient->ReleaseBuffer(packet_size);
          if (copy.size() > 0 && packet_size > 0) {
            keep_capturing = callback(copy);
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
                                StringConverter(device_id)};
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
};

class DeviceEnumerator
{
public:

    class DeviceCollection
    {
    public:
        DeviceCollection(IMMDeviceCollection *collection)
            : m_collection(collection)
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
            devices.push_back(std::make_shared<Device>(immDevice));
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
      return Device(device);
    }

    DeviceCollection get_render_collection()
    {
      IMMDeviceCollection *imm_collection;
      auto result = m_enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &imm_collection);
      return DeviceCollection(imm_collection);
    }


private:
    IMMDeviceEnumerator *m_enumerator;

    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
};

AudioSinkInfo get_default_sink()
{
  DeviceEnumerator enumerator;
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
  std::vector<std::shared_ptr<Device>> devices = enumerator.get_render_collection().get_devices();

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
  DeviceEnumerator::DeviceCollection collection(enumerator.get_render_collection());
  auto devices = collection.get_devices();
  for (auto &device : devices) {
    sinks.push_back(*device);
  }
  return sinks;
}
}