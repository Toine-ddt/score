#pragma once
#include <QString>
#include <map>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>

#include <iscore_lib_base_export.h>

namespace iscore
{
class Menu;
struct Menus
{
        static StringKey<Menu> File() { return StringKey<Menu>{"File"}; }
        static StringKey<Menu> Export() { return StringKey<Menu>{"Export"}; }
        static StringKey<Menu> Edit() { return StringKey<Menu>{"Edit"}; }
        static StringKey<Menu> Object() { return StringKey<Menu>{"Object"}; }
        static StringKey<Menu> Play() { return StringKey<Menu>{"Play"}; }
        static StringKey<Menu> Tool() { return StringKey<Menu>{"Tool"}; }
        static StringKey<Menu> View() { return StringKey<Menu>{"View"}; }
        static StringKey<Menu> Windows() { return StringKey<Menu>{"Windows"}; }
        static StringKey<Menu> Settings() { return StringKey<Menu>{"Settings"}; }
        static StringKey<Menu> About() { return StringKey<Menu>{"About"}; }
};

enum class ToplevelMenuElement : int // ISCORE_LIB_BASE_EXPORT
{
    FileMenu,
    EditMenu,
    ObjectMenu,
    PlayMenu,
    ToolMenu,
    ViewMenu,
    SettingsMenu,
    AboutMenu
};

// this is used to create sub context menu
// and to enable actions in ObjectMenu
enum class ContextMenu : int
{
    Object,
    Constraint,
    Process,
    Slot,
    Rack,
    Event,
    State
};
/**
     * @brief The MenuInterface class
     *
     * It is a way to allow plug-ins to put their options in a sensible place.
     * For instance, add an "Export" option after the standard "Save as...".
     *
     * The strings are not directly available to the plug-ins because they have to be translated.
     */
class ISCORE_LIB_BASE_EXPORT MenuInterface
{
    public:
        template<typename MenuType>
        static std::map<MenuType, QString> map();

        template<typename MenuType>
        static QString name(MenuType elt);

    private:
        static const std::map<ToplevelMenuElement, QString> m_map;
        static const std::map<ContextMenu, QString> m_contextMap;
};
}
