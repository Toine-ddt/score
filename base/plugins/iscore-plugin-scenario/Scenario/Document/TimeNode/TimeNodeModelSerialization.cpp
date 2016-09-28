
#include <iscore/tools/std/Optional.hpp>
#include <QDataStream>
#include <QtGlobal>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>

#include <iscore/model/ModelMetadata.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <State/Expression.hpp>
#include "TimeNodeModel.hpp"
#include "Trigger/TriggerModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/TreeNode.hpp>

template <typename T> class Reader;
template <typename T> class Writer;
template <typename model> class IdentifiedObject;

template<>
ISCORE_PLUGIN_SCENARIO_EXPORT void Visitor<Reader<DataStream>>::readFrom(const Scenario::TimeNodeModel& timenode)
{
    readFrom(static_cast<const iscore::Entity<Scenario::TimeNodeModel>&>(timenode));

    m_stream << timenode.m_date
             << timenode.m_events
             << timenode.m_extent;

    m_stream << timenode.trigger()->active()
             << timenode.trigger()->expression();

    insertDelimiter();
}

template<>
ISCORE_PLUGIN_SCENARIO_EXPORT void Visitor<Writer<DataStream>>::writeTo(Scenario::TimeNodeModel& timenode)
{
    bool a;
    State::Trigger t;
    m_stream >> timenode.m_date
             >> timenode.m_events
             >> timenode.m_extent
             >> a
             >> t;

    timenode.m_trigger = new Scenario::TriggerModel{Id<Scenario::TriggerModel>(0), &timenode};
    timenode.trigger()->setExpression(t);
    timenode.trigger()->setActive(a);

    checkDelimiter();
}

template<>
ISCORE_PLUGIN_SCENARIO_EXPORT void Visitor<Reader<JSONObject>>::readFrom(const Scenario::TimeNodeModel& timenode)
{
    readFrom(static_cast<const iscore::Entity<Scenario::TimeNodeModel>&>(timenode));

    m_obj["Date"] = toJsonValue(timenode.date());
    m_obj["Events"] = toJsonArray(timenode.m_events);
    m_obj["Extent"] = toJsonValue(timenode.m_extent);

    QJsonObject trig;
    trig["Active"] = timenode.m_trigger->active();
    trig["Expression"] = toJsonObject(timenode.m_trigger->expression());
    m_obj["Trigger"] = trig;
}

template<>
ISCORE_PLUGIN_SCENARIO_EXPORT void Visitor<Writer<JSONObject>>::writeTo(Scenario::TimeNodeModel& timenode)
{
    if(timenode.metadata().getLabel() == QStringLiteral("TimeNode"))
        timenode.metadata().setLabel("");

    timenode.m_date = fromJsonValue<TimeValue> (m_obj["Date"]);
    timenode.m_extent = fromJsonValue<Scenario::VerticalExtent>(m_obj["Extent"]);

    fromJsonValueArray(m_obj["Events"].toArray(), timenode.m_events);

    timenode.m_trigger =  new Scenario::TriggerModel{Id<Scenario::TriggerModel>(0), &timenode};

    State::Trigger t;
    fromJsonObject(m_obj["Trigger"].toObject()["Expression"], t);
    timenode.m_trigger->setExpression(t);
    timenode.m_trigger->setActive(m_obj["Trigger"].toObject()["Active"].toBool());
}
