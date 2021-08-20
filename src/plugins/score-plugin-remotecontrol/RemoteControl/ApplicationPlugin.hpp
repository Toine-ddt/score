#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <RemoteControl/HttpServer.hpp>
namespace RemoteControl
{
class ApplicationPlugin final : public score::GUIApplicationPlugin
{
public:
  ApplicationPlugin(const score::GUIApplicationContext& app);

protected:
  void on_createdDocument(score::Document& doc) override;
  HttpServer m_server;
};
}
