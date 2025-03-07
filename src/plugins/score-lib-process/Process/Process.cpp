// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Process.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SetIcons.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QObject>

#include <wobjectimpl.h>

#include <stdexcept>
W_OBJECT_IMPL(Process::ProcessModel)

#if !defined(SCORE_ALL_UNITY)
template class IdentifiedObject<Process::ProcessModel>;
template class score::SerializableInterface<Process::ProcessModelFactory>;
#endif
namespace Process
{

const QIcon& getCategoryIcon(const QString& category) noexcept
{
  static const std::map<QString, QIcon> categoryIcon{
      {"Audio", makeIcon(QStringLiteral(":/icons/audio.png"))},
      {"Mappings", makeIcon(QStringLiteral(":/icons/filter.png"))},
      {"Midi", makeIcon(QStringLiteral(":/icons/midi.png"))},
      {"Control", makeIcon(QStringLiteral(":/icons/controls.png"))},
      {"GFX", makeIcon(QStringLiteral(":/icons/gfx.png"))},
      {"Automations", makeIcon(QStringLiteral(":/icons/automation.png"))},
      {"Impro", makeIcon(QStringLiteral(":/icons/controls.png"))},
      {"Script", makeIcon(QStringLiteral(":/icons/script.png"))},
      {"Structure", makeIcon(QStringLiteral(":/icons/structure.png"))},
      {"Monitoring", makeIcon(QStringLiteral(":/icons/ui.png"))}};
  static const QIcon invalid;
  if (auto it = categoryIcon.find(category); it != categoryIcon.end())
  {
    return it->second;
  }
  return invalid;
}
ProcessModel::ProcessModel(
    TimeVal duration,
    const Id<ProcessModel>& id,
    const QString& name,
    QObject* parent)
    : Entity{id, name, parent}
    , m_duration{std::move(duration)}
    , m_slotHeight{300}
    , m_loopDuration{m_duration}
    , m_size{200, 100}
    , m_loops{false}
{
  con(metadata(), &score::ModelMetadata::NameChanged, this, [=] {
    prettyNameChanged();
  });
  connect(this, &Process::ProcessModel::resetExecution, this, [this] {
        for(auto& p : this->m_inlets)
          p->executionReset();
        for(auto& p : this->m_outlets)
          p->executionReset();
      });
  // metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel()
{
  identified_object_destroying(this);
}

void ProcessModel::setDurationAndScale(const TimeVal& newDuration) noexcept
{
  setDuration(newDuration);
}

void ProcessModel::setDurationAndGrow(const TimeVal& newDuration) noexcept
{
  setDuration(newDuration);
}

void ProcessModel::setDurationAndShrink(const TimeVal& newDuration) noexcept
{
  setDuration(newDuration);
}

ProcessModel::ProcessModel(DataStream::Deserializer& vis, QObject* parent)
    : Entity(vis, parent)
{
  vis.writeTo(*this);
  con(metadata(), &score::ModelMetadata::NameChanged, this, [=] {
    prettyNameChanged();
  });
}

ProcessModel::ProcessModel(JSONObject::Deserializer& vis, QObject* parent)
    : Entity(vis, parent)
{
  vis.writeTo(*this);
  con(metadata(), &score::ModelMetadata::NameChanged, this, [=] {
    prettyNameChanged();
  });
}

QString ProcessModel::prettyName() const noexcept
{
  return metadata().getName();
}

void ProcessModel::setParentDuration(
    ExpandMode mode,
    const TimeVal& t) noexcept
{
  switch (mode)
  {
    case ExpandMode::Scale:
      setDurationAndScale(t);
      break;
    case ExpandMode::GrowShrink:
    {
      if (duration() < t)
        setDurationAndGrow(t);
      else
        setDurationAndShrink(t);
      break;
    }
    case ExpandMode::ForceGrow:
    {
      if (duration() < t)
        setDurationAndGrow(t);
      break;
    }
    case ExpandMode::CannotExpand:
    default:
      break;
  }
}

TimeVal ProcessModel::contentDuration() const noexcept
{
  return TimeVal::zero();
}

void ProcessModel::setDuration(const TimeVal& other) noexcept
{
  m_duration = other;
  durationChanged(m_duration);
}

const TimeVal& ProcessModel::duration() const noexcept
{
  return m_duration;
}

ProcessStateDataInterface* ProcessModel::startStateData() const noexcept
{
  return nullptr;
}

ProcessStateDataInterface* ProcessModel::endStateData() const noexcept
{
  return nullptr;
}

Selection ProcessModel::selectableChildren() const noexcept
{
  return {};
}

Selection ProcessModel::selectedChildren() const noexcept
{
  return {};
}

void ProcessModel::setSelection(const Selection& s) const noexcept { }

Process::Inlet* ProcessModel::inlet(const Id<Process::Port>& p) const noexcept
{
  for (auto e : m_inlets)
    if (e->id() == p)
      return e;
  return nullptr;
}

Process::Outlet*
ProcessModel::outlet(const Id<Process::Port>& p) const noexcept
{
  for (auto e : m_outlets)
    if (e->id() == p)
      return e;
  return nullptr;
}

void ProcessModel::loadPreset(const Preset& preset)
{
  const rapidjson::Document doc = readJson(preset.data);
  const auto& ctrls = doc.GetArray();

  for (const auto& arr : ctrls)
  {
    const auto& id = arr[0].GetInt();
    ossia::value val = JsonValue{arr[1]}.to<ossia::value>();

    auto it = ossia::find_if(
        m_inlets, [&](const auto& inl) { return inl->id().val() == id; });
    if (it != m_inlets.end())
    {
      Process::Inlet& inlet = **it;
      if (auto ctrl = qobject_cast<Process::ControlInlet*>(&inlet))
      {
        ctrl->setValue(val);
      }
    }
  }
}

Preset ProcessModel::savePreset() const noexcept
{
  Preset p;
  p.name = this->metadata().getName();
  p.key.key = this->concreteKey();

  JSONReader r;
  r.stream.StartArray();
  for (const auto& inlet : m_inlets)
  {
    if (auto ctrl = qobject_cast<Process::ControlInlet*>(inlet))
    {
      r.stream.StartArray();
      r.stream.Int(ctrl->id().val());
      r.readFrom(ctrl->value());
      r.stream.EndArray();
    }
  }
  r.stream.EndArray();
  p.data = r.toByteArray();
  return p;
}

void ProcessModel::ancestorStartDateChanged() { }

void ProcessModel::ancestorTempoChanged() { }

void ProcessModel::forEachControl(
    smallfun::function<void(ControlInlet&, const ossia::value&)> f) const
{
  for (const auto& inlet : m_inlets)
  {
    if (auto ctrl = qobject_cast<Process::ControlInlet*>(inlet))
    {
      f(*ctrl, ctrl->value());
    }
  }
}

void ProcessModel::setLoops(bool b)
{
  if (b != m_loops)
  {
    m_loops = b;
    loopsChanged(b);
  }
}

void ProcessModel::setStartOffset(TimeVal b)
{
  if (b != m_startOffset)
  {
    m_startOffset = b;
    startOffsetChanged(b);
  }
}

void ProcessModel::setLoopDuration(TimeVal b)
{
  if (b.msec() < 0.1)
    b = TimeVal::fromMsecs(0.1);

  if (b != m_loopDuration)
  {
    m_loopDuration = b;
    loopDurationChanged(b);
  }
}

QPointF ProcessModel::position() const noexcept
{
  return m_position;
}

QSizeF ProcessModel::size() const noexcept
{
  return m_size;
}

void ProcessModel::setPosition(const QPointF& v)
{
  if (v != m_position)
  {
    m_position = v;
    positionChanged(v);
  }
}

void ProcessModel::setSize(const QSizeF& v)
{
  if (v != m_size)
  {
    m_size = v;
    sizeChanged(v);
  }
}

double ProcessModel::getSlotHeight() const noexcept
{
  return m_slotHeight;
}

void ProcessModel::setSlotHeight(double v) noexcept
{
  m_slotHeight = v;
  slotHeightChanged(v);
}

ProcessModel* parentProcess(QObject* obj) noexcept
{
  if (obj)
    obj = obj->parent();

  while (obj && !qobject_cast<ProcessModel*>(obj))
  {
    obj = obj->parent();
  }

  if (obj)
    return static_cast<ProcessModel*>(obj);
  return nullptr;
}

const ProcessModel* parentProcess(const QObject* obj) noexcept
{
  if (obj)
    obj = obj->parent();

  while (obj && !qobject_cast<const ProcessModel*>(obj))
  {
    obj = obj->parent();
  }

  if (obj)
    return static_cast<const ProcessModel*>(obj);
  return nullptr;
}
}
