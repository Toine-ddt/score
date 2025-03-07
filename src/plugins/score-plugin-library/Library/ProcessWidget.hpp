#pragma once
#include <Library/PresetListView.hpp>
#include <Library/ProcessTreeView.hpp>
#include <Library/LibraryInterface.hpp>

#include <score/tools/std/Optional.hpp>

#include <QSplitter>
#include <QTreeView>

#include <score_plugin_library_export.h>

#include <Process/Preset.hpp>
namespace score
{
struct GUIApplicationContext;
}
namespace Library
{
class ProcessesItemModel;
class PresetItemModel;

class PresetLibraryHandler final
    : public QObject
    , public Library::LibraryInterface
{
  SCORE_CONCRETE("7fc3a366-7792-489f-aca9-79d9f6d4415d")

  QSet<QString> acceptedFiles() const noexcept override;

  void setup(
      Library::ProcessesItemModel& model,
      const score::GUIApplicationContext& ctx) override;

  void addPath(std::string_view path) override;

  const Process::ProcessFactoryList* processes{};
public:
  std::vector<Process::Preset> presets;
};

class SCORE_PLUGIN_LIBRARY_EXPORT ProcessWidget : public QWidget
{
public:
  ProcessWidget(const score::GUIApplicationContext& ctx, QWidget* parent);
  ~ProcessWidget();

  ProcessesItemModel& processModel() const noexcept { return *m_processModel; }
  const ProcessTreeView& processView() const noexcept { return m_tv; }
  ProcessTreeView& processView() noexcept { return m_tv; }

private:
  ProcessesItemModel* m_processModel{};
  PresetItemModel* m_presetModel{};
  ProcessTreeView m_tv;
  PresetListView m_lv;
};
}
