#include "qt_widget_runtime.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVariant>

#include <memory>
#include <stdexcept>

namespace
{
QApplication *ensureApplication()
{
    if (auto *application = qobject_cast<QApplication *>(QCoreApplication::instance()))
        return application;
    if (QCoreApplication::instance())
        throw std::runtime_error("Qt Widgets requires QApplication");

    static int argc = 1;
    static char applicationName[] = "SandScript";
    static char *argv[] = {applicationName, nullptr};
    static std::unique_ptr<QApplication> ownedApplication =
        std::make_unique<QApplication>(argc, argv);
    return ownedApplication.get();
}

QVariant toVariant(const RuntimeVal &value)
{
    if (const auto *number = value.as_num())
    {
        bool ok = false;
        const double converted = QString::fromStdString(number->value.to_string()).toDouble(&ok);
        if (!ok)
            throw std::runtime_error("widget property number is outside Qt's supported range");
        return converted;
    }
    if (const auto *string = value.as_str())
        return QString::fromStdString(string->value);
    throw std::runtime_error("widget properties require a number or string value");
}

int toInt(const RuntimeVal &value, const std::string &name)
{
    const auto *number = value.as_num();
    if (!number)
        throw std::runtime_error("widget property '" + name + "' requires a number");

    bool ok = false;
    const int converted = QString::fromStdString(number->value.to_string()).toInt(&ok);
    if (!ok)
        throw std::runtime_error("widget property '" + name + "' requires an integer");
    return converted;
}

bool toBool(const RuntimeVal &value, const std::string &name)
{
    const auto *number = value.as_num();
    if (!number)
        throw std::runtime_error("widget property '" + name + "' requires a number");
    return number->value != BigDecimal(0);
}
}

QtWidgetRuntime::~QtWidgetRuntime()
{
    for (const auto &widget : rootWidgets)
        delete widget.data();
}

QWidget *QtWidgetRuntime::createWidget(const std::string &type, QWidget *parent)
{
    ensureApplication();

    QWidget *widget = nullptr;
    if (type == "Window")
    {
        widget = new QWidget(parent, parent ? Qt::Widget : Qt::Window);
    }
    else if (type == "Widget")
    {
        widget = new QWidget(parent);
    }
    else if (type == "Label")
    {
        widget = new QLabel(parent);
    }
    else if (type == "Button")
    {
        widget = new QPushButton(parent);
    }
    else if (type == "TextBox")
    {
        widget = new QLineEdit(parent);
    }
    else if (type == "CheckBox")
    {
        widget = new QCheckBox(parent);
    }
    else
    {
        throw std::runtime_error("unknown Qt widget type '" + type + "'");
    }

    if (!parent)
        rootWidgets.emplace_back(widget);
    return widget;
}

void QtWidgetRuntime::setProperty(QWidget *widget, const std::string &name, const RuntimeVal &value)
{
    if (!widget)
        throw std::runtime_error("cannot set a property on a null widget");

    if (name == "x")
    {
        widget->move(toInt(value, name), widget->y());
    }
    else if (name == "y")
    {
        widget->move(widget->x(), toInt(value, name));
    }
    else if (name == "width")
    {
        widget->resize(toInt(value, name), widget->height());
    }
    else if (name == "height")
    {
        widget->resize(widget->width(), toInt(value, name));
    }
    else if (name == "isShow" || name == "visible")
    {
        widget->setVisible(toBool(value, name));
    }
    else if (name == "enabled")
    {
        widget->setEnabled(toBool(value, name));
    }
    else if (name == "text")
    {
        const auto *string = value.as_str();
        if (!string)
            throw std::runtime_error("widget property 'text' requires a string");
        const QString text = QString::fromStdString(string->value);
        if (auto *label = qobject_cast<QLabel *>(widget))
            label->setText(text);
        else if (auto *button = qobject_cast<QPushButton *>(widget))
            button->setText(text);
        else if (auto *lineEdit = qobject_cast<QLineEdit *>(widget))
            lineEdit->setText(text);
        else if (auto *checkBox = qobject_cast<QCheckBox *>(widget))
            checkBox->setText(text);
        else
            widget->setWindowTitle(text);
    }
    else if (name == "title")
    {
        const auto *string = value.as_str();
        if (!string)
            throw std::runtime_error("widget property 'title' requires a string");
        widget->setWindowTitle(QString::fromStdString(string->value));
    }
    else
    {
        widget->setProperty(name.c_str(), toVariant(value));
    }
}

void QtWidgetRuntime::show(QWidget *widget)
{
    if (!widget)
        throw std::runtime_error("cannot show a null widget");
    widget->show();
}

int QtWidgetRuntime::runEventLoop()
{
    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!application)
        return 0;

    for (const auto &widget : rootWidgets)
    {
        if (widget && widget->isVisible())
            return application->exec();
    }
    return 0;
}
