#include "NodeUpdateProxy.hpp"
#include "DeviceDocumentPlugin.hpp"
#include "Plugin/Panel/DeviceExplorerModel.hpp"
#include <DeviceExplorer/Address/AddressSettings.hpp>
#include <boost/range/algorithm/find_if.hpp>

NodeUpdateProxy::NodeUpdateProxy(DeviceDocumentPlugin& root):
    m_devModel{root}
{

}

void NodeUpdateProxy::addDevice(const iscore::DeviceSettings& dev)
{
    iscore::Node node(dev, nullptr);
    auto newNode = m_devModel.createDeviceFromNode(node);

    if(m_deviceExplorer)
    {
        m_deviceExplorer->addDevice(std::move(newNode));
    }
    else
    {
        m_devModel.rootNode().push_back(std::move(newNode));
    }
}

void NodeUpdateProxy::loadDevice(const iscore::Node& node)
{
    m_devModel.createDeviceFromNode(node);

    if(m_deviceExplorer)
    {
        m_deviceExplorer->addDevice(node);
    }
    else
    {
        m_devModel.rootNode().push_back(node);
    }
}

void NodeUpdateProxy::updateDevice(
        const QString &name,
        const iscore::DeviceSettings& dev)
{
    m_devModel.list().device(name).updateSettings(dev);

    if(m_deviceExplorer)
    {
        m_deviceExplorer->updateDevice(name, dev);
    }
    else
    {
        auto it = std::find_if(m_devModel.rootNode().begin(), m_devModel.rootNode().end(),
                               [&] (const iscore::Node& n) { return n.get<iscore::DeviceSettings>().name == name; });

        ISCORE_ASSERT(it != m_devModel.rootNode().end());
        it->set(dev);
    }
}

void NodeUpdateProxy::removeDevice(const iscore::DeviceSettings& dev)
{
    m_devModel.list().removeDevice(dev.name);

    for(auto it = m_devModel.rootNode().begin(); it < m_devModel.rootNode().end(); ++it)
    {
        if(it->is<iscore::DeviceSettings>() && it->get<iscore::DeviceSettings>().name == dev.name)
        {
            if(m_deviceExplorer)
            {
                m_deviceExplorer->removeNode(it);
            }
            else
            {
                m_devModel.rootNode().removeChild(it);
            }
        }
    }
}

void NodeUpdateProxy::addAddress(
        const iscore::NodePath& parentPath,
        const iscore::AddressSettings& settings)
{
    auto parentnode = parentPath.toNode(&m_devModel.rootNode());

    // Add in the device impl
    // Get the device node :
    const auto& dev_node = m_devModel.rootNode().childAt(parentPath.at(0));
    ISCORE_ASSERT(dev_node.is<iscore::DeviceSettings>());

    // Make a full path
    iscore::FullAddressSettings full = iscore::FullAddressSettings::make<iscore::FullAddressSettings::as_parent>(
                                   settings,
                                   iscore::address(*parentnode));

    // Add in the device implementation
    m_devModel
            .list()
            .device(dev_node.get<iscore::DeviceSettings>().name)
            .addAddress(full);

    // Add in the device explorer
    if(m_deviceExplorer)
    {
        m_deviceExplorer->addAddress(
                    parentnode,
                    settings);
    }
    else
    {
        parentnode->emplace_back(settings, parentnode);
    }
}

void NodeUpdateProxy::updateAddress(
        const iscore::NodePath &nodePath,
        const iscore::AddressSettings &settings)
{
    auto node = nodePath.toNode(&m_devModel.rootNode());
    const auto addr = iscore::address(*node);
    // Make a full path
    iscore::FullAddressSettings full = iscore::FullAddressSettings::make<iscore::FullAddressSettings::as_child>(
                                   settings,
                                   addr);

    // Update in the device implementation
    m_devModel
            .list()
            .device(addr.device)
            .updateAddress(full);

    // Update in the device explorer
    if(m_deviceExplorer)
    {
        m_deviceExplorer->updateAddress(
                    node,
                    settings);
    }
    else
    {
        node->set(settings);
    }
}

void NodeUpdateProxy::removeAddress(
        const iscore::NodePath& parentPath,
        const iscore::AddressSettings& settings)
{
    iscore::Node* parentnode = parentPath.toNode(&m_devModel.rootNode());

    auto addr = iscore::address(*parentnode);
    addr.path.append(settings.name);

    // Remove from the device implementation
    const auto& dev_node = m_devModel.rootNode().childAt(parentPath.at(0));
    m_devModel.list().device(
                dev_node.get<iscore::DeviceSettings>().name)
            .removeAddress(addr);

    // Remove from the device explorer
    auto it = std::find_if(
                  parentnode->begin(), parentnode->end(),
                  [&] (const iscore::Node& n) { return n.get<iscore::AddressSettings>().name == settings.name; });
    ISCORE_ASSERT(it != parentnode->end());

    if(m_deviceExplorer)
    {
        m_deviceExplorer->removeNode(it);
    }
    else
    {
        parentnode->removeChild(it);
    }
}

void NodeUpdateProxy::updateLocalValue(
        const iscore::Address& addr,
        const iscore::Value& v)
{
    auto n = iscore::try_getNodeFromAddress(m_devModel.rootNode(), addr);
    if(!n->is<iscore::AddressSettings>())
    {
        qDebug() << "Updating invalid node";
        return;
    }
    if(m_deviceExplorer)
    {
        m_deviceExplorer->updateValue(n, v);
    }
    else
    {
        n->get<iscore::AddressSettings>().value = v;
    }
}

void NodeUpdateProxy::updateRemoteValue(const iscore::Address& addr, const iscore::Value& val)
{
    // TODO add these checks everywhere.
    if(m_devModel.list().hasDevice(addr.device))
    {
        // Update in the device implementation
        m_devModel
                .list()
                .device(addr.device)
                .sendMessage({addr, val});
    }
}
