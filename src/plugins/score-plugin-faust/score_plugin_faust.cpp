#include "score_plugin_faust.hpp"

#include <Library/LibraryInterface.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <Faust/EffectModel.hpp>
#include <Faust/Library.hpp>
#include <score_plugin_faust_commands_files.hpp>
#include <wobjectimpl.h>

// Undefine macros defined by Qt / Verdigris
#if defined(__arm__)
#undef READ
#undef WRITE
#undef RESET
#undef OPTIONAL

#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#endif
score_plugin_faust::score_plugin_faust()
{
#if defined(__arm__)
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();
#endif
}

score_plugin_faust::~score_plugin_faust() { }

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_plugin_faust::make_commands()
{
  using namespace Faust;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_faust_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_faust::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, Faust::FaustEffectFactory>,
      FW<Process::LayerFactory, Faust::LayerFactory>,
      FW<Library::LibraryInterface, Faust::LibraryHandler>,
      FW<Execution::ProcessComponentFactory,
         Execution::FaustEffectComponentFactory>,
      FW<Process::ProcessDropHandler, Faust::DropHandler>>(ctx, key);
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_faust)
