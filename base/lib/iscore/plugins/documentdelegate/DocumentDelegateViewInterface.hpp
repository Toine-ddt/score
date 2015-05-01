#pragma once
#include <QObject>
namespace iscore
{
    class DocumentDelegatePresenterInterface;
    class DocumentDelegateViewInterface : public QObject
    {
        public:
            using QObject::QObject;

            virtual QWidget* getWidget() = 0;
    };
}
