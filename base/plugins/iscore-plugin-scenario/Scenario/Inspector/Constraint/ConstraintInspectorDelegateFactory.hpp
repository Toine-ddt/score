#pragma once
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>
#include <memory>
#include <vector>

#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/tools/std/Algorithms.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
class ConstraintModel;

class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintInspectorDelegateFactory : public iscore::FactoryInterfaceBase
{
         ISCORE_ABSTRACT_FACTORY_DECL(
                 ConstraintInspectorDelegate,
                 "e9ae0303-b616-4953-b148-88d2dda5ac45")
    public:
        virtual ~ConstraintInspectorDelegateFactory();
        virtual std::unique_ptr<ConstraintInspectorDelegate> make(const ConstraintModel& constraint) = 0;
        virtual bool matches(const ConstraintModel& constraint) const = 0;
};

class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintInspectorDelegateFactoryList final : public iscore::FactoryListInterface
{
    public:
      static const iscore::AbstractFactoryKey& static_abstractFactoryKey() {
          return ConstraintInspectorDelegateFactory::static_abstractFactoryKey();
      }

      iscore::AbstractFactoryKey name() const final override {
          return ConstraintInspectorDelegateFactory::static_abstractFactoryKey();
      }

      void insert(std::unique_ptr<iscore::FactoryInterfaceBase> e) final override
      {
          if(auto pf = dynamic_unique_ptr_cast<ConstraintInspectorDelegateFactory>(std::move(e)))
              m_list.emplace_back(std::move(pf));
      }

      const auto& list() const
      {
          return m_list;
      }

      auto make(const ConstraintModel& constraint) const
      {
          auto it = find_if(m_list, [&] (const auto& elt) { return elt->matches(constraint); });
          return (it != m_list.end())
                  ? (*it)->make(constraint)
                  : nullptr;
      }

    private:
      std::vector<std::unique_ptr<ConstraintInspectorDelegateFactory>> m_list;
};
}
