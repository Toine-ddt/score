// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DocumentPlugin.hpp"

#include "BaseScenarioComponent.hpp"

#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/AudioDevice.hpp>
#include <Audio/Settings/Model.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Execution/Settings/ExecutorModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/model/ComponentUtils.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/tools/Bind.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/audio/audio_protocol.hpp>
#include <ossia/dataflow/bench_map.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/flicks.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/network/common/path.hpp>

#include <QCoreApplication>

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/StateExecution.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>
#include <wobjectimpl.h>
W_REGISTER_ARGTYPE(ossia::bench_map)
W_OBJECT_IMPL(Execution::DocumentPlugin)
namespace Execution
{
DocumentPlugin::DocumentPlugin(
    const score::DocumentContext& ctx,
    QObject* parent)
    : score::DocumentPlugin{ctx, "OSSIADocumentPlugin", parent}
    , settings{ctx.app.settings<Execution::Settings::Model>()}
    , m_execQueue(1024)
    , m_editionQueue(1024)
    , m_gcQueue(1024)
    , m_ctx
{
  ctx, m_created, {}, {}, m_execQueue, m_editionQueue, m_gcQueue, m_setup_ctx,
      execGraph, execState
#if (__cplusplus > 201703L) && !defined(_MSC_VER)
      ,
  {
    ossia::disable_init
  }
#endif
}
, m_setup_ctx{m_ctx}, m_base{m_ctx, this}
{
  makeGraph();
  auto& devs = ctx.plugin<Explorer::DeviceDocumentPlugin>();
  local_device = devs.list().localDevice();
  if (auto dev = devs.list().audioDevice())
  {
    audio_device = static_cast<Dataflow::AudioDevice*>(dev);
  }
  else
  {
    audio_device = new Dataflow::AudioDevice(
        {Dataflow::AudioProtocolFactory::static_concreteKey(), "audio", {}});
    ctx.plugin<Explorer::DeviceDocumentPlugin>().list().setAudioDevice(
        audio_device);
  }

  devs.list().apply([this](auto& d) { on_deviceAdded(&d); });
  con(devs.list(),
      &Device::DeviceList::deviceAdded,
      this,
      &DocumentPlugin::on_deviceAdded);
  con(devs.list(), &Device::DeviceList::deviceRemoved, this, [=](auto* dev) {
    if (auto d = dev->getDevice())
      unregisterDevice(d);
  });

  auto& model = ctx.model<Scenario::ScenarioDocumentModel>();
  model.cables.mutable_added.connect<&SetupContext::on_cableCreated>(
      m_setup_ctx);
  model.cables.removing.connect<&SetupContext::on_cableRemoved>(m_setup_ctx);

  con(
      m_base,
      &Execution::BaseScenarioElement::finished,
      this,
      [=] {
        auto& stop_action = context().doc.app.actions.action<Actions::Stop>();
        stop_action.action()->trigger();
      },
      Qt::QueuedConnection);

  connect(
      this,
      &DocumentPlugin::finished,
      this,
      &DocumentPlugin::on_finished,
      Qt::DirectConnection);
  connect(
      this,
      &DocumentPlugin::sig_bench,
      this,
      &DocumentPlugin::slot_bench,
      Qt::QueuedConnection);
}

DocumentPlugin::~DocumentPlugin()
{
  if (m_base.active())
  {
    m_base.baseInterval().stop();
    clear();
  }

  if (auto devs = context().doc.findPlugin<Explorer::DeviceDocumentPlugin>())
  {
    devs->list().setAudioDevice(nullptr);
    devs->updateProxy.removeDevice(audio_device->settings());
  }
  if (audio_device)
    delete audio_device;
}

void DocumentPlugin::on_finished()
{
  if (m_tid != -1)
  {
    killTimer(m_tid);
    m_tid = -1;

    {
      ExecutionCommand cmd;
      while (m_editionQueue.try_dequeue(cmd))
        cmd();
      GCCommand gc;
      while (m_gcQueue.try_dequeue(gc))
        ;
    }
  }

  clear();

  initExecState();

  for (auto& v : m_setup_ctx.runtime_connections)
  {
    v.second.clear();
  }
  m_setup_ctx.runtime_connections.clear();
}

void DocumentPlugin::initExecState()
{
  execState = std::make_shared<ossia::execution_state>();
  auto& devlist
      = score::DocumentPlugin::context().plugin<Explorer::DeviceDocumentPlugin>().list().devices();
  if (audio_device)
    execState->register_device(audio_device->getDevice());
  if (local_device)
    execState->register_device(local_device->getDevice());
  for (auto dev : devlist)
  {
    registerDevice(dev->getDevice());
  }
  execState->apply_device_changes();
}

void DocumentPlugin::timerEvent(QTimerEvent* event)
{
  ExecutionCommand cmd;
  while (m_editionQueue.try_dequeue(cmd))
    cmd();
  GCCommand gc;
  while (m_gcQueue.try_dequeue(gc))
    ;
}

void DocumentPlugin::registerDevice(ossia::net::device_base* d)
{
  if (execState)
    execState->register_device(d);
}

void DocumentPlugin::unregisterDevice(ossia::net::device_base* d)
{
  if (execState)
    execState->unregister_device(d);
}

void DocumentPlugin::makeGraph()
{
  using namespace ossia;
  const score::DocumentContext& ctx = m_ctx.doc;
  auto& audiosettings = ctx.app.settings<Audio::Settings::Model>();

  static const Execution::Settings::SchedulingPolicies sched_t;
  static const Execution::Settings::OrderingPolicies order_t;
  static const Execution::Settings::MergingPolicies merge_t;

  // note: cas qui n'ont pas de sens: dynamic avec les cas ou on append les
  // valeurs. parallel avec dynamic il manque le cas "default score order" il
  // manque le log pour dynamic

  auto sched = settings.getScheduling();

  if (execGraph)
    execGraph->clear();
  execGraph.reset();

  execState.reset();

  initExecState();

  execState->bufferSize = audiosettings.getBufferSize();
  execState->sampleRate = audiosettings.getRate();
  execState->modelToSamplesRatio
      = audiosettings.getRate() / ossia::flicks_per_second<double>;
  execState->samplesToModelRatio
      = ossia::flicks_per_second<double> / audiosettings.getRate();
  execState->samples_since_start = 0;
  execState->start_date = 0; // TODO set it in the first callback
  execState->cur_date = execState->start_date;

  ossia::graph_setup_options opt;
  opt.parallel = settings.getParallel();
  if (settings.getLogging())
    opt.log = ossia::logger_ptr();
  if (settings.getBench())
  {
    bench = std::make_shared<bench_map>();
    opt.bench = bench;
    opt.bench->clear();
  }

  if (sched == sched_t.StaticFixed)
    opt.scheduling = ossia::graph_setup_options::StaticFixed;
  else if (sched == sched_t.StaticBFS)
    opt.scheduling = ossia::graph_setup_options::StaticBFS;
  else if (sched == sched_t.StaticTC)
    opt.scheduling = ossia::graph_setup_options::StaticTC;
  else if (sched == sched_t.Dynamic)
    opt.scheduling = ossia::graph_setup_options::Dynamic;

  execGraph = ossia::make_graph(opt);
}

void DocumentPlugin::reload(Scenario::IntervalModel& cst)
{
  if (m_base.active())
  {
    m_base.baseInterval().stop();
  }
  clear();

  const score::DocumentContext& ctx = m_context;
  auto& settings = ctx.app.settings<Execution::Settings::Model>();

  m_ctx.time = settings.makeTimeFunction(ctx);
  m_ctx.reverseTime = settings.makeReverseTimeFunction(ctx);

  // Notify devices that they have to start running stuff, polling frames, etc.
  auto& devs = m_context.plugin<Explorer::DeviceDocumentPlugin>();
  devs.list().apply([](const Device::DeviceInterface& d) {
    if(auto dev = d.getDevice())
      dev->get_protocol().start_execution();
  });

  makeGraph();

  auto parent = dynamic_cast<Scenario::ScenarioInterface*>(cst.parent());
  SCORE_ASSERT(parent);
  m_base.init(BaseScenarioRefContainer{cst, *parent});
  m_created = true;

  auto& model = context().doc.model<Scenario::ScenarioDocumentModel>();
  for (auto& cable : model.cables)
  {
    m_setup_ctx.connectCable(cable);
  }

  for (auto ctl : model.statesWithControls)
  {
    auto state_comp = score::findComponent<Execution::StateComponentBase>(
        ctl->components());
    if (state_comp)
    {
      state_comp->updateControls();
    }
  }

  m_tid = startTimer(32);
  // runAllCommands();
}

void DocumentPlugin::clear()
{
  m_setup_ctx.inlets.clear();
  m_setup_ctx.outlets.clear();
  m_setup_ctx.m_cables.clear();
  m_setup_ctx.proc_map.clear();

  if (m_base.active())
  {
    runAllCommands();
    runAllCommands();
    m_base.cleanup();
    m_created = false;
    runAllCommands();
    runAllCommands();

    if (execGraph)
      execGraph->clear();
    execGraph.reset();
    execState.reset();
  }

  // Notify devices that they have to stop running stuff, polling frames, etc.
  auto& devs = m_context.plugin<Explorer::DeviceDocumentPlugin>();
  devs.list().apply([](const Device::DeviceInterface& d) {
    if(auto dev = d.getDevice())
      dev->get_protocol().stop_execution();
  });
}

void DocumentPlugin::on_documentClosing()
{
  if (m_base.active())
  {
    m_base.baseInterval().stop();
    m_ctx.context()
        .doc.app.guiApplicationPlugin<Engine::ApplicationPlugin>()
        .execution()
        .request_stop();
    clear();
  }
  execState.reset();

  audioProto().stop();
}

const BaseScenarioElement& DocumentPlugin::baseScenario() const
{
  return m_base;
}

BaseScenarioElement& DocumentPlugin::baseScenario()
{
  return m_base;
}

void DocumentPlugin::playStartState()
{
  auto scenar = score::IDocument::try_get<Scenario::ScenarioDocumentModel>(
      this->m_context.document);
  if (!scenar)
    return;
  auto& sm = scenar->baseScenario().startState();

  // FIXME that does not look thread-safe at all !
  // what if devices are being added/removed in the exec thread !
  if (execState)
  {
    auto state = Engine::score_to_ossia::state(sm, this->context());
    state.launch();
  }
  else
  {
    // Create a temporary execution_state...
    execState = std::make_shared<ossia::execution_state>();

    // Fill its devices
    auto& devs = this->context().doc.plugin<Explorer::DeviceDocumentPlugin>();
    devs.list().apply([this](auto& dev) {
      if (auto d = dev.getDevice())
      {
        execState->register_device(d);
      }
    });

    auto state = Engine::score_to_ossia::state(sm, this->context());
    state.launch();

    execState.reset();
  }
}

bool DocumentPlugin::isPlaying() const
{
  return m_base.active();
}

const ExecutionController& DocumentPlugin::executionController() const noexcept
{
  return this->context()
      .doc.app.guiApplicationPlugin<Engine::ApplicationPlugin>()
      .execution();
}

ossia::audio_protocol& DocumentPlugin::audioProto()
{
  return static_cast<ossia::audio_protocol&>(
      audio_device->getDevice()->get_protocol());
}

void DocumentPlugin::runAllCommands() const
{
  std::atomic_thread_fence(std::memory_order_seq_cst);
  ExecutionCommand com;
  while (m_execQueue.try_dequeue(com))
    com();
}

void DocumentPlugin::registerAction(ExecutionAction& act)
{
  m_actions.push_back(&act);
}

void DocumentPlugin::slot_bench(ossia::bench_map b, int64_t ns)
{
  for (const auto& p : b)
  {
    if (p.second)
    {
      auto proc = m_setup_ctx.proc_map.find(p.first);
      if (proc != m_setup_ctx.proc_map.end())
      {
        if (proc->second)
        {
          const_cast<Process::ProcessModel*>(proc->second)
              ->benchmark(100. * *p.second / (double)ns);
        }
      }
    }
  }
}

void DocumentPlugin::on_deviceAdded(Device::DeviceInterface* dev)
{
  if (auto d = dev->getDevice())
  {
    connect(
        dev,
        &Device::DeviceInterface::deviceChanged,
        this,
        [=](ossia::net::device_base* old_dev,
            ossia::net::device_base* new_dev) {
          if (old_dev)
            unregisterDevice(old_dev);
          if (new_dev)
            registerDevice(new_dev);
        });
    registerDevice(d);
  }
}
}
