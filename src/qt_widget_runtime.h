#ifndef QSand
#define QSand
#include "RuntimeValue.h"

#include <QPointer>
#include <QWidget>

#include <string>
#include <vector>

class QtWidgetRuntime
{
public:
    QtWidgetRuntime() = default;
    ~QtWidgetRuntime();

    QtWidgetRuntime(const QtWidgetRuntime &) = delete;
    QtWidgetRuntime &operator=(const QtWidgetRuntime &) = delete;

    QWidget *createWidget(const std::string &type, QWidget *parent = nullptr);
    void setProperty(QWidget *widget, const std::string &name, const RuntimeVal &value);
    void show(QWidget *widget);
    int runEventLoop();

private:
    std::vector<QPointer<QWidget>> rootWidgets;
};
#endif