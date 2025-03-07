#pragma once
#include <LocalTree/EventComponent.hpp>
#include <LocalTree/IntervalComponent.hpp>
#include <LocalTree/StateComponent.hpp>
#include <LocalTree/TimeSyncComponent.hpp>

#include <Scenario/Document/Components/ScenarioComponent.hpp>

namespace LocalTree
{
class SCORE_PLUGIN_SCENARIO_EXPORT ScenarioComponentBase
    : public ProcessComponent_T<Scenario::ProcessModel>
{
  COMPONENT_METADATA("57c37324-f5a5-494e-8b45-206750d9fa77")

public:
  ScenarioComponentBase(
      ossia::net::node_base& parent,
      Scenario::ProcessModel& scenario,
      const score::DocumentContext& doc,
      QObject* parent_obj);

  template <typename Component_T, typename Element>
  Component_T* make(Element& elt);

  template <typename... Args>
  bool removing(Args&&...)
  {
    return true;
  }
  template <typename... Args>
  void removed(Args&&...)
  {
  }

private:
  ossia::net::node_base& m_intervalsNode;
  ossia::net::node_base& m_eventsNode;
  ossia::net::node_base& m_timeSyncsNode;
  ossia::net::node_base& m_statesNode;
};

using ScenarioComponent = HierarchicalScenarioComponent<
    ScenarioComponentBase,
    Scenario::ProcessModel,
    Interval,
    Event,
    TimeSync,
    State>;

using ScenarioComponentFactory
    = LocalTree::ProcessComponentFactory_T<ScenarioComponent>;
}
