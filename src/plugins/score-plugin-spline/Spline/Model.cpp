// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortSerialization.hpp>

#include <ossia/network/common/destination_qualifiers.hpp>

#include <Spline/Model.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Spline::ProcessModel)
namespace Spline
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::
        ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , outlet{Process::make_value_outlet(Id<Process::Port>(0), this)}
{
  m_spline.points.push_back({0., 0.});

  m_spline.points.push_back({0.4, 0.075});
  m_spline.points.push_back({0.45, 0.24});
  m_spline.points.push_back({0.5, 0.5});

  m_spline.points.push_back({0.55, 0.76});
  m_spline.points.push_back({0.7, 0.9});
  m_spline.points.push_back({1.0, 1.0});

  init();
  metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel() { }

void ProcessModel::init()
{
  outlet->setName("Out");
  m_outlets.push_back(outlet.get());
  connect(
      outlet.get(),
      &Process::Port::addressChanged,
      this,
      [=](const State::AddressAccessor& arg) {
        addressChanged(arg);
        prettyNameChanged();
        unitChanged(arg.qualifiers.get().unit);
      });
}

QString ProcessModel::prettyName() const noexcept
{
  return address().toString();
}

void ProcessModel::setDurationAndScale(const TimeVal& newDuration) noexcept
{
  // We only need to change the duration.
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

TimeVal ProcessModel::contentDuration() const noexcept
{
  return duration();
}

::State::AddressAccessor ProcessModel::address() const
{
  return outlet->address();
}

void ProcessModel::setAddress(const ::State::AddressAccessor& arg)
{
  outlet->setAddress(arg);
}

State::Unit ProcessModel::unit() const
{
  return outlet->address().qualifiers.get().unit;
}

void ProcessModel::setUnit(const State::Unit& u)
{
  if (u != unit())
  {
    auto addr = outlet->address();
    addr.qualifiers.get().unit = u;
    outlet->setAddress(addr);
    prettyNameChanged();
    unitChanged(u);
  }
}

void ProcessModel::loadPreset(const Process::Preset& preset)
{
  m_spline.points <<= JSONWriter{readJson(preset.data)}.obj["Spline"];
  splineChanged();
}

Process::Preset ProcessModel::savePreset() const noexcept
{
  Process::Preset p;
  p.name = this->metadata().getName();
  p.key = {this->concreteKey(), {}};

  JSONReader r;
  r.stream.StartObject();
  r.obj["Spline"] = m_spline.points;
  r.stream.EndObject();

  p.data = r.toByteArray();
  return p;
}

}

/// Point ///
template <>
void DataStreamReader::read(const ossia::spline_point& autom)
{
  m_stream << autom.x << autom.y;
}

template <>
void DataStreamWriter::write(ossia::spline_point& autom)
{
  m_stream >> autom.x >> autom.y;
}

template <>
void JSONReader::read(const ossia::spline_point& autom)
{
  stream.StartArray();
  stream.Double(autom.x);
  stream.Double(autom.y);
  stream.EndArray();
}

template <>
void JSONWriter::write(ossia::spline_point& autom)
{
  const auto& arr = base.GetArray();
  autom.x = arr[0].GetDouble();
  autom.y = arr[1].GetDouble();
}

/// Data ///
template <>
void DataStreamReader::read(const ossia::spline_data& autom)
{
  m_stream << autom.points;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(ossia::spline_data& autom)
{
  m_stream >> autom.points;
  checkDelimiter();
}

template <>
void DataStreamReader::read(const Spline::ProcessModel& autom)
{
  m_stream << *autom.outlet << autom.m_spline << autom.m_tween;

  insertDelimiter();
}
template <>
void DataStreamWriter::write(Spline::ProcessModel& autom)
{
  autom.outlet = Process::load_value_outlet(*this, &autom);
  m_stream >> autom.m_spline >> autom.m_tween;

  checkDelimiter();
}

template <>
void JSONReader::read(const Spline::ProcessModel& autom)
{
  obj["Outlet"] = *autom.outlet;
  obj["Spline"] = autom.m_spline.points;
  obj["Tween"] = autom.tween();
}

template <>
void JSONWriter::write(Spline::ProcessModel& autom)
{
  JSONWriter writer{obj["Outlet"]};
  autom.outlet = Process::load_value_outlet(writer, &autom);

  autom.setTween(obj["Tween"].toBool());
  autom.m_spline.points <<= obj["Spline"];
}
