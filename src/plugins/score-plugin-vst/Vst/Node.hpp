#pragma once
#include <Process/Dataflow/TimeSignature.hpp>
#include <Vst/EffectModel.hpp>

#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/editor/scenario/time_signature.hpp>
namespace vst
{

class vst_node_base : public ossia::graph_node
{
public:
  std::shared_ptr<AEffectWrapper> fx{};

protected:
  explicit vst_node_base(std::shared_ptr<AEffectWrapper>&& ptr)
      : fx{std::move(ptr)}
  {
    m_inlets.reserve(10);
    controls.reserve(10);
  }

  struct vst_control
  {
    int idx{};
    float value{};
    ossia::value_port* port{};
  };

  inline void dispatch(
      int32_t opcode,
      int32_t index = 0,
      intptr_t value = 0,
      void* ptr = nullptr,
      float opt = 0.0f)
  {
    fx->dispatch(opcode, index, value, ptr, opt);
  }

public:
  ossia::small_vector<vst_control, 16> controls;

  void setControls()
  {
    for (vst_control& p : controls)
    {
      const auto& vec = p.port->get_data();
      if (vec.empty())
        continue;
      if (auto t = last(vec).template target<float>())
      {
        p.value = ossia::clamp<float>(*t, 0.f, 1.f);
        fx->fx->setParameter(fx->fx, p.idx, p.value);
      }
    }
  }

  auto& prepareInput(int64_t offset, int64_t samples)
  {
    const auto bs = offset + samples;
    auto& ip = m_inlets[0]->template target<ossia::audio_port>()->samples;
    switch (ip.size())
    {
      case 0:
      {
        ip.resize(2);
        ip[0].resize(bs);
        ip[1].resize(bs);
        break;
      }
      case 1:
      {
        ip.resize(2);
        ip[0].resize(bs);
        ip[1].assign(ip[0].begin(), ip[0].end());
        break;
      }
      default:
      {
        for (auto& i : ip)
          i.resize(bs);
        break;
      }
    }

    return ip;
  }

  auto& prepareOutput(int64_t offset, int64_t samples)
  {
    const auto bs = offset + samples;
    auto& op = m_outlets[0]->template target<ossia::audio_port>()->samples;
    op.resize(2);
    for (auto& chan : op)
      chan.resize(bs);
    return op;
  }

  void
  setupTimeInfo(const ossia::token_request& tk, ossia::exec_state_facade st)
  {
    static const constexpr double ppq_reference = 960.;

    auto& time_info = fx->info;
    time_info.samplePos = tk.date.impl;
    time_info.sampleRate = st.sampleRate();
    time_info.nanoSeconds = st.currentDate() - st.startDate();
    time_info.ppqPos = tk.musical_start_position * ppq_reference;
    time_info.tempo = tk.tempo;
    time_info.barStartPos = tk.musical_start_last_bar * ppq_reference;
    time_info.cycleStartPos = 0.;
    time_info.cycleEndPos = 0.;
    time_info.timeSigNumerator = tk.signature.upper;
    time_info.timeSigDenominator = tk.signature.lower;
    time_info.smpteOffset = 0;
    time_info.smpteFrameRate = 0;
    time_info.samplesToNextClock = 0;
    time_info.flags = kVstTransportPlaying | kVstNanosValid | kVstPpqPosValid
                      | kVstTempoValid | kVstBarsValid | kVstTimeSigValid
                      | kVstClockValid;
  }
};

template <bool UseDouble, bool IsSynth>
class vst_node final : public vst_node_base
{
public:
  static constexpr bool synth = IsSynth;
  int m_bs{};
  vst_node(std::shared_ptr<AEffectWrapper> dat, int sampleRate, int bs)
      : vst_node_base{std::move(dat)}
      , m_bs{bs}
  {
    // Midi or audio input
    m_inlets.push_back(new ossia::audio_inlet);
    if constexpr (IsSynth)
      m_inlets.push_back(new ossia::midi_inlet);

    // audio output
    m_outlets.push_back(new ossia::audio_outlet);

    dispatch(effSetSampleRate, 0, sampleRate, nullptr, sampleRate);
    dispatch(effSetBlockSize, 0, bs, nullptr, bs); // Generalize what's in pd
    dispatch(
        effSetProcessPrecision,
        0,
        UseDouble ? kVstProcessPrecision64 : kVstProcessPrecision32);
    dispatch(effMainsChanged, 0, 1);
    dispatch(effStartProcess);

    fx->fx->resvd2 = reinterpret_cast<intptr_t>(this);
  }

  ~vst_node()
  {
    fx->fx->resvd2 = 0;
    dispatch(effStopProcess);
    dispatch(effMainsChanged, 0, 0);
  }

  std::string label() const noexcept override { return ""; }

  void all_notes_off() noexcept override
  {
    if constexpr (IsSynth)
    {
      // copy midi data
      // should be 32 but some VSTs read a bit out of bounds apparently ! so we allocate a bit more memory
      constexpr auto sz = sizeof(VstEvents) + sizeof(void*) * 32 * 2;
      VstEvents* events = (VstEvents*)alloca(sz);
      std::memset(events, 0, sz);

      events->numEvents = 32;

      VstMidiEvent ev[32] = {};

      // All notes off
      for (int i = 0; i < 16; i++)
      {
        auto& e = ev[i];
        e.type = kVstMidiType;
        e.flags = kVstMidiEventIsRealtime;
        e.byteSize = sizeof(VstMidiEvent);

        e.midiData[0] = (char)(uint8_t)176;
        e.midiData[1] = (char)(uint8_t)123;
        e.midiData[2] = 0;
        e.midiData[3] = 0;

        events->events[i] = reinterpret_cast<VstEvent*>(&e);
      }

      // All sound off
      for (int i = 0; i < 16; i++)
      {
        auto& e = ev[16 + i];
        e.type = kVstMidiType;
        e.flags = kVstMidiEventIsRealtime;
        e.byteSize = sizeof(VstMidiEvent);

        e.midiData[0] = (char)(uint8_t)176;
        e.midiData[1] = (char)(uint8_t)121;
        e.midiData[2] = 0;
        e.midiData[3] = 0;

        events->events[16 + i] = reinterpret_cast<VstEvent*>(&e);
      }

      dispatch(effProcessEvents, 0, 0, events, 0.f);

      if constexpr (!UseDouble)
      {
        float* dummy = (float*)alloca(sizeof(float) * m_bs);
        std::fill_n(dummy, m_bs, 0.f);

        float** output = (float**)alloca(
            sizeof(float*) * std::max(2, this->fx->fx->numOutputs));
        for (int i = 0; i < this->fx->fx->numOutputs; i++)
          output[i] = dummy;

        fx->fx->processReplacing(fx->fx, output, output, m_bs);
      }
      else
      {
        double* dummy = (double*)alloca(sizeof(double) * m_bs);
        std::fill_n(dummy, m_bs, 0.);

        double** output = (double**)alloca(
            sizeof(double*) * std::max(2, this->fx->fx->numOutputs));
        for (int i = 0; i < this->fx->fx->numOutputs; i++)
          output[i] = dummy;

        fx->fx->processDoubleReplacing(fx->fx, output, output, m_bs);
      }
    }
  }

  // Note: the function that does the actual tick is passed as an argument
  // since some plug-ins only store pointers to VstEvents struct,
  // which would go out of scope if this function was just called like this.
  template <typename Fun>
  void dispatchMidi(int64_t offset, Fun&& f)
  {
    // copy midi data
    auto& ip = static_cast<ossia::midi_inlet*>(m_inlets[1])->data.messages;
    const auto n_mess = ip.size();
    if (n_mess == 0)
    {
      f();
      return;
    }

    // -2 since two are already available ?
    const auto sz = sizeof(VstEvents) + sizeof(void*) * n_mess * 2;
    VstEvents* events = (VstEvents*)alloca(sz);
    std::memset(events, 0, sz);
    events->numEvents = n_mess;

    ossia::small_vector<VstMidiEvent, 16> vec;
    vec.resize(n_mess);
    std::size_t i = 0;
    for (libremidi::message& mess : ip)
    {
      VstMidiEvent& e = vec[i];
      std::memset(&e, 0, sizeof(VstMidiEvent));

      e.type = kVstMidiType;
      e.byteSize = sizeof(VstMidiEvent);
      e.deltaFrames = mess.timestamp - offset;
      e.flags = kVstMidiEventIsRealtime;

      std::memcpy(
          e.midiData,
          mess.bytes.data(),
          std::min(mess.bytes.size(), (std::size_t)4));
      // for (std::size_t k = 0, N = std::min(mess.bytes.size(),
      // (std::size_t)4); k < N; k++)
      //   e.midiData[k] = mess.bytes[k];

      events->events[i] = reinterpret_cast<VstEvent*>(&e);
      i++;
    }
    dispatch(effProcessEvents, 0, 0, events, 0.f);
    f();
  }

  void
  run(const ossia::token_request& tk,
      ossia::exec_state_facade st) noexcept override
  {
    if (!muted() && tk.date > tk.prev_date)
    {
      const int64_t offset
          = tk.physical_start(st.modelToSamples());
      const int64_t samples
          = tk.physical_write_duration(st.modelToSamples());
      this->setControls();
      this->setupTimeInfo(tk, st);

      if constexpr (UseDouble)
      {
        if constexpr (IsSynth)
        {
          dispatchMidi(offset, [=] { processDouble(offset, samples); });
        }
        else
        {
          processDouble(offset, samples);
        }
      }
      else
      {
        if constexpr (IsSynth)
        {
          dispatchMidi(offset, [=] { processFloat(offset, samples); });
        }
        else
        {
          processFloat(offset, samples);
        }
      }

      // upmix mono VSTs to stereo
      if (this->fx->fx->numOutputs == 1)
      {
        auto& op = m_outlets[0]->template target<ossia::audio_port>()->samples;
        op[1].assign(op[0].begin(), op[0].end());
      }
    }
  }

  void processFloat(int64_t offset, int64_t samples)
  {
    if constexpr (!UseDouble)
    {
      SCORE_ASSERT(m_bs >= offset + samples);

      const auto max_i = std::max(2, this->fx->fx->numInputs);
      const auto max_o = std::max(2, this->fx->fx->numOutputs);
      const auto max_io = std::max(max_i, max_o);

      // prepare ossia::graph_node buffers
      auto& ip = prepareInput(offset, samples);
      SCORE_ASSERT(ip.size() >= 2);

      auto& op = prepareOutput(offset, samples);
      SCORE_ASSERT(op.size() >= 2);

      // copy io
      float_v[0].resize(samples);
      float_v[1].resize(samples);

      std::copy(ip[0].begin() + offset, ip[0].end(), float_v[0].begin());
      std::copy(ip[1].begin() + offset, ip[1].end(), float_v[1].begin());

      float** io = (float**)alloca(sizeof(float*) * max_io);
      io[0] = float_v[0].data();
      io[1] = float_v[1].data();

      if(max_io > 2)
      {
        // Note that alloca has *function* scope, not block scope so it is
        // freed at the end
        float* dummy = (float*)alloca(sizeof(float) * m_bs);
        std::fill_n(dummy, m_bs, 0.f);

        for (int i = 2; i < max_io; i++)
          io[i] = dummy;
      }

      fx->fx->processReplacing(fx->fx, io, io, samples);

      std::copy(float_v[0].begin(), float_v[0].end(), op[0].begin() + offset);
      std::copy(float_v[1].begin(), float_v[1].end(), op[1].begin() + offset);

      float_v[0].clear();
      float_v[1].clear();
    }
  }

  void processDouble(int64_t offset, int64_t samples)
  {
    if constexpr (UseDouble)
    {
      SCORE_ASSERT(m_bs >= offset + samples);

      const auto max_i = std::max(2, this->fx->fx->numInputs);
      const auto max_o = std::max(2, this->fx->fx->numOutputs);
      const auto max_io = std::max(max_i, max_o);

      // copy audio data
      auto& ip = prepareInput(offset, samples);
      SCORE_ASSERT(ip.size() >= 2);

      auto& op = prepareOutput(offset, samples);
      SCORE_ASSERT(op.size() >= 2);

      double** input = (double**)alloca(sizeof(double*) * max_i);
      input[0] = ip[0].data() + offset;
      input[1] = ip[1].data() + offset;

      double** output = (double**)alloca(sizeof(double*) * max_o);
      output[0] = op[0].data() + offset;
      output[1] = op[1].data() + offset;

      if(max_io > 2)
      {
        // Note that alloca has *function* scope, not block scope so it is
        // freed at the end
        double* dummy = (double*)alloca(sizeof(double) * m_bs);
        std::fill_n(dummy, m_bs, 0.);
        for (int i = 2; i < this->fx->fx->numInputs; i++)
          input[i] = dummy;
        for (int i = 2; i < this->fx->fx->numOutputs; i++)
          output[i] = dummy;
      }

      fx->fx->processDoubleReplacing(fx->fx, input, output, samples);
    }
  }

  struct dummy_t
  {
  };
  std::conditional_t<!UseDouble, std::array<ossia::float_vector, 2>, dummy_t>
      float_v;
};

template <bool b1, bool b2, typename... Args>
auto make_vst_fx(Args&... args)
{
  return std::make_shared<vst_node<b1, b2>>(args...);
}
}
