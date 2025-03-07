#pragma once
#include <Audio/AudioInterface.hpp>
#include <Audio/PortAudioInterface.hpp>
#include <Audio/Settings/Model.hpp>

#include <score/widgets/SignalUtils.hpp>

#include <QComboBox>
#include <QFormLayout>

namespace Audio
{
#if __has_include(<pa_win_wasapi.h>)
class WASAPIFactory final
    : public QObject
    , public AudioFactory
{
  SCORE_CONCRETE("afcd9c64-0367-4fa1-b2bb-ee65b1c5e5a7")
public:
  std::vector<PortAudioCard> devices;

  WASAPIFactory() { rescan(); }

  ~WASAPIFactory() override { }
  bool available() const noexcept override { return true; }
  void initialize(
      Audio::Settings::Model& set,
      const score::ApplicationContext& ctx) override
  {
    auto device_in = ossia::find_if(devices, [&](const PortAudioCard& dev) {
      return dev.raw_name == set.getCardIn() && dev.hostapi != paInDevelopment;
    });
    auto device_out = ossia::find_if(devices, [&](const PortAudioCard& dev) {
      return dev.raw_name == set.getCardOut()
             && dev.hostapi != paInDevelopment;
    });

    if (device_in == devices.end() || device_out == devices.end())
    {
      set.setCardIn(devices.back().raw_name);
      set.setCardOut(devices.back().raw_name);
      set.setDefaultIn(devices.back().inputChan);
      set.setDefaultOut(devices.back().outputChan);
      set.setRate(devices.back().rate);

      set.changed();
    }
    else
    {
      if (device_out != devices.end())
      {
        set.setDefaultIn(device_out->inputChan);
        set.setDefaultOut(device_out->outputChan);
        set.setRate(device_out->rate);

        set.changed();
      }
    }
  }

  void rescan()
  {
    devices.clear();
    PortAudioScope portaudio;

    devices.push_back(
        PortAudioCard{{}, {}, QObject::tr("No device"), -1, 0, 0, {}});
    for (int i = 0; i < Pa_GetHostApiCount(); i++)
    {
      auto hostapi = Pa_GetHostApiInfo(i);
      if (hostapi->type == PaHostApiTypeId::paWASAPI)
      {
        for (int card = 0; card < hostapi->deviceCount; card++)
        {
          auto dev_idx = Pa_HostApiDeviceIndexToDeviceIndex(i, card);
          auto dev = Pa_GetDeviceInfo(dev_idx);
          if (dev->maxOutputChannels > 0)
          {
            auto raw_name = QString::fromUtf8(Pa_GetDeviceInfo(dev_idx)->name);
            devices.push_back(PortAudioCard{
                "WASAPI",
                raw_name,
                raw_name,
                dev_idx,
                dev->maxInputChannels,
                dev->maxOutputChannels,
                hostapi->type,
                dev->defaultSampleRate});
          }
        }
      }
    }
  }

  QString prettyName() const override { return QObject::tr("WASAPI"); }
  std::unique_ptr<ossia::audio_engine> make_engine(
      const Audio::Settings::Model& set,
      const score::ApplicationContext& ctx) override
  {
    return std::make_unique<ossia::portaudio_engine>(
        "ossia score",
        set.getCardIn().toStdString(),
        set.getCardOut().toStdString(),
        set.getDefaultIn(),
        set.getDefaultOut(),
        set.getRate(),
        set.getBufferSize(),
        paWASAPI);
  }

  void setCard(QComboBox* combo, QString val)
  {
    auto dev_it = ossia::find_if(
        devices, [&](const PortAudioCard& d) { return d.raw_name == val; });
    if (dev_it != devices.end())
    {
      combo->setCurrentIndex(dev_it->out_index);
    }
  }

  QWidget* make_settings(
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp,
      QWidget* parent) override
  {
    auto w = new QWidget{parent};
    auto lay = new QFormLayout{w};

    auto card_list = new QComboBox{w};

    // Disabled case
    card_list->addItem(devices.front().pretty_name, 0);
    devices.front().out_index = 0;

    // Normal devices
    for (std::size_t i = 1; i < devices.size(); i++)
    {
      auto& card = devices[i];
      card_list->addItem(card.pretty_name, (int)i);
      card.out_index = card_list->count() - 1;
    }

    using Model = Audio::Settings::Model;

    {
      lay->addRow(QObject::tr("Device"), card_list);

      auto update_dev = [=, &m, &m_disp](const PortAudioCard& dev) {
        if (dev.raw_name != m.getCardOut())
        {
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardIn>(
              m, dev.raw_name);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardOut>(
              m, dev.raw_name);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(
              m, dev.inputChan);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(
              m, dev.outputChan);
        }
      };

      QObject::connect(
          card_list,
          SignalUtils::QComboBox_currentIndexChanged_int(),
          &v,
          [=](int i) {
            auto& device = devices[card_list->itemData(i).toInt()];
            update_dev(device);
          });

      if (m.getCardOut().isEmpty())
      {
        if (!devices.empty())
        {
          update_dev(devices.front());
        }
      }
      else
      {
        setCard(card_list, m.getCardOut());
      }
    }

    addBufferSizeWidget(*w, m, v);
    addSampleRateWidget(*w, m, v);

    con(m, &Model::changed, w, [=, &m] {
      setCard(card_list, m.getCardOut());
    });
    return w;
  }
};
#endif
}
