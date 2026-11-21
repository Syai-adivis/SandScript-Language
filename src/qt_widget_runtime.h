#ifndef QT_WIDGET_RUNTIME_H
#define QT_WIDGET_RUNTIME_H

#include "RuntimeValue.h"

#include <QApplication>
#include <QAbstractButton>
#include <QAbstractSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDateEdit>
#include <QDynamicPropertyChangeEvent>
#include <QEvent>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QObject>
#include <QProgressBar>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QTextEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QVariant>
#include <QWidget>
#include <QCalendarWidget>
#include <QDial>
#include <QLCDNumber>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSplitter>
#include <QFrame>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QPixmap>
#include <QPainter>
#include <QTimer>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QFile>
#include <QTextStream>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace qt_widget_runtime_detail
{
inline QApplication *ensureApplication()
{
    if (auto *application = qobject_cast<QApplication *>(QCoreApplication::instance()))
        return application;
    if (QCoreApplication::instance())
        throw std::runtime_error("Qt Widgets requires QApplication");

    QCoreApplication::addLibraryPath("C:/Qt/6.12.0/mingw_64/plugins");
    QCoreApplication::addLibraryPath("C:/Qt/6.12.0/plugins");
    QCoreApplication::addLibraryPath(".");

    static int argc = 1;
    static char applicationName[] = "SandScript";
    static char *argv[] = {applicationName, nullptr};
    static std::unique_ptr<QApplication> ownedApplication =
        std::make_unique<QApplication>(argc, argv);
    return ownedApplication.get();
}

inline QVariant toVariant(const RuntimeVal &value)
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

inline int toInt(const RuntimeVal &value, const std::string &name)
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

inline bool toBool(const RuntimeVal &value, const std::string &name)
{
    const auto *number = value.as_num();
    if (!number)
        throw std::runtime_error("widget property '" + name + "' requires a number");
    return number->value != BigDecimal(0);
}
}

class SandCanvasWidget : public QWidget
{
public:
    explicit SandCanvasWidget(QWidget *parent = nullptr) : QWidget(parent) {}

    void setBackground(const RuntimeVal &value)
    {
        if (auto *string = value.as_str())
            background = QColor(QString::fromStdString(string->value));
        update();
    }

    void drawLine(const RuntimeVal &value);
    void drawRect(const RuntimeVal &value);
    void drawText(const RuntimeVal &value);

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), background);
        for (const auto &line : lines)
            painter.drawLine(line);
        for (const auto &rectangle : rectangles)
            painter.drawRect(rectangle);
        for (const auto &text : texts)
            painter.drawText(text.first, text.second);
    }

private:
    QColor background = Qt::white;
    std::vector<QLine> lines;
    std::vector<QRect> rectangles;
    std::vector<std::pair<QPoint, QString>> texts;
};

inline void SandCanvasWidget::drawLine(const RuntimeVal &value)
{
    auto *array = value.as_array();
    if (!array || array->value.size() != 4)
        throw std::runtime_error("drawLine requires [x1,y1,x2,y2]");
    lines.push_back(QLine(
        qt_widget_runtime_detail::toInt(array->value[0], "drawLine"),
        qt_widget_runtime_detail::toInt(array->value[1], "drawLine"),
        qt_widget_runtime_detail::toInt(array->value[2], "drawLine"),
        qt_widget_runtime_detail::toInt(array->value[3], "drawLine")));
    update();
}

inline void SandCanvasWidget::drawRect(const RuntimeVal &value)
{
    auto *array = value.as_array();
    if (!array || array->value.size() != 4)
        throw std::runtime_error("drawRect requires [x,y,width,height]");
    rectangles.push_back(QRect(
        qt_widget_runtime_detail::toInt(array->value[0], "drawRect"),
        qt_widget_runtime_detail::toInt(array->value[1], "drawRect"),
        qt_widget_runtime_detail::toInt(array->value[2], "drawRect"),
        qt_widget_runtime_detail::toInt(array->value[3], "drawRect")));
    update();
}

inline void SandCanvasWidget::drawText(const RuntimeVal &value)
{
    auto *array = value.as_array();
    if (!array || array->value.size() != 3)
        throw std::runtime_error("drawText requires [x,y,text]");
    auto *text = array->value[2].as_str();
    if (!text)
        throw std::runtime_error("drawText text must be a string");
    texts.push_back({
        QPoint(qt_widget_runtime_detail::toInt(array->value[0], "drawText"),
               qt_widget_runtime_detail::toInt(array->value[1], "drawText")),
        QString::fromStdString(text->value)});
    update();
}

class QtWidgetRuntime
{
public:
    QtWidgetRuntime() : eventWatcher(std::make_unique<EventWatcher>()) {}
    ~QtWidgetRuntime()
    {
        for (const auto &widget : rootWidgets)
            delete widget.data();
    }

    QtWidgetRuntime(const QtWidgetRuntime &) = delete;
    QtWidgetRuntime &operator=(const QtWidgetRuntime &) = delete;

    int createTimer(int intervalMs, bool repeated, std::function<void()> callback)
    {
        qt_widget_runtime_detail::ensureApplication();
        int id = nextTimerId++;
        auto timer = std::make_shared<QTimer>();
        timer->setInterval(std::max(0, intervalMs));
        if (!repeated)
            timer->setSingleShot(true);
        QObject::connect(timer.get(), &QTimer::timeout, timer.get(), [callback]() { callback(); });
        timers[id] = std::move(timer);
        timers[id]->start();
        return id;
    }

    void startTimer(int id)
    {
        auto it = timers.find(id);
        if (it == timers.end())
            throw std::runtime_error("unknown timer id");
        it->second->start();
    }

    void stopTimer(int id)
    {
        auto it = timers.find(id);
        if (it == timers.end())
            throw std::runtime_error("unknown timer id");
        it->second->stop();
    }

    void setTimerInterval(int id, int intervalMs)
    {
        auto it = timers.find(id);
        if (it == timers.end())
            throw std::runtime_error("unknown timer id");
        it->second->setInterval(std::max(0, intervalMs));
    }

    QWidget *createWidget(const std::string &type, QWidget *parent = nullptr)
    {
        qt_widget_runtime_detail::ensureApplication();

        QWidget *widget = nullptr;
        if (type == "Window")
            widget = new QWidget(parent, parent ? Qt::Widget : Qt::Window);
        else if (type == "Widget")
            widget = new QWidget(parent);
        else if (type == "Label")
            widget = new QLabel(parent);
        else if (type == "Button")
            widget = new QPushButton(parent);
        else if (type == "TextBox")
            widget = new QLineEdit(parent);
        else if (type == "TextArea")
            widget = new QTextEdit(parent);
        else if (type == "CheckBox")
            widget = new QCheckBox(parent);
        else if (type == "RadioButton")
            widget = new QRadioButton(parent);
        else if (type == "ComboBox")
            widget = new QComboBox(parent);
        else if (type == "Slider")
            widget = new QSlider(Qt::Horizontal, parent);
        else if (type == "ProgressBar")
            widget = new QProgressBar(parent);
        else if (type == "SpinBox")
            widget = new QSpinBox(parent);
        else if (type == "GroupBox")
            widget = new QGroupBox(parent);
        else if (type == "ListView")
            widget = new QListWidget(parent);
        else if (type == "TableWidget")
            widget = new QTableWidget(parent);
        else if (type == "DateEdit")
            widget = new QDateEdit(parent);
        else if (type == "TabWidget")
            widget = new QTabWidget(parent);
        else if (type == "ScrollArea")
            widget = new QScrollArea(parent);
        else if (type == "Splitter")
            widget = new QSplitter(parent);
        else if (type == "Frame")
            widget = new QFrame(parent);
        else if (type == "Dial")
            widget = new QDial(parent);
        else if (type == "LCDNumber")
            widget = new QLCDNumber(parent);
        else if (type == "Calendar")
            widget = new QCalendarWidget(parent);
        else if (type == "PlainText")
            widget = new QPlainTextEdit(parent);
        else if (type == "TreeView")
            widget = new QTreeWidget(parent);
        else if (type == "StackWidget")
            widget = new QStackedWidget(parent);
        else if (type == "MenuBar")
            widget = new QMenuBar(parent);
        else if (type == "ToolBar")
            widget = new QToolBar(parent);
        else if (type == "StatusBar")
            widget = new QStatusBar(parent);
        else if (type == "Image")
            widget = new QLabel(parent);
        else if (type == "Canvas")
            widget = new SandCanvasWidget(parent);
        else
            throw std::runtime_error("unknown Qt widget type '" + type + "'");

        if (!parent)
        {
            if (type == "Window")
                widget->resize(640, 480);
            rootWidgets.emplace_back(widget);
        }
        return widget;
    }

    RuntimeVal propertyValue(QWidget *widget, const std::string &name) const
    {
        if (!widget)
            throw std::runtime_error("cannot read a property from a null widget");
        if (name == "x")
            return RuntimeVal(BigDecimal(widget->x()));
        if (name == "y")
            return RuntimeVal(BigDecimal(widget->y()));
        if (name == "width" || name == "weith")
            return RuntimeVal(BigDecimal(widget->width()));
        if (name == "height")
            return RuntimeVal(BigDecimal(widget->height()));
        if (name == "isShow" || name == "visible")
            return RuntimeVal(BigDecimal(widget->isVisible() ? 1 : 0));
        if (name == "enabled")
            return RuntimeVal(BigDecimal(widget->isEnabled() ? 1 : 0));
        if (name == "title")
            return RuntimeVal(widget->windowTitle().toStdString());
        if (name == "text")
        {
            if (auto *label = qobject_cast<QLabel *>(widget))
                return RuntimeVal(label->text().toStdString());
            if (auto *button = qobject_cast<QPushButton *>(widget))
                return RuntimeVal(button->text().toStdString());
            if (auto *lineEdit = qobject_cast<QLineEdit *>(widget))
                return RuntimeVal(lineEdit->text().toStdString());
            if (auto *textEdit = qobject_cast<QTextEdit *>(widget))
                return RuntimeVal(textEdit->toPlainText().toStdString());
            if (auto *checkBox = qobject_cast<QCheckBox *>(widget))
                return RuntimeVal(checkBox->text().toStdString());
            if (auto *radioButton = qobject_cast<QRadioButton *>(widget))
                return RuntimeVal(radioButton->text().toStdString());
            if (auto *groupBox = qobject_cast<QGroupBox *>(widget))
                return RuntimeVal(groupBox->title().toStdString());
            return RuntimeVal(widget->windowTitle().toStdString());
        }
        if (name == "value")
        {
            if (auto *slider = qobject_cast<QAbstractSlider *>(widget))
                return RuntimeVal(BigDecimal(slider->value()));
            if (auto *progress = qobject_cast<QProgressBar *>(widget))
                return RuntimeVal(BigDecimal(progress->value()));
            if (auto *spinBox = qobject_cast<QSpinBox *>(widget))
                return RuntimeVal(BigDecimal(spinBox->value()));
        }
        if (name == "minimum" || name == "maximum")
        {
            const bool minimum = name == "minimum";
            if (auto *slider = qobject_cast<QAbstractSlider *>(widget))
                return RuntimeVal(BigDecimal(minimum ? slider->minimum() : slider->maximum()));
            if (auto *progress = qobject_cast<QProgressBar *>(widget))
                return RuntimeVal(BigDecimal(minimum ? progress->minimum() : progress->maximum()));
            if (auto *spinBox = qobject_cast<QSpinBox *>(widget))
                return RuntimeVal(BigDecimal(minimum ? spinBox->minimum() : spinBox->maximum()));
        }
        if (name == "checked")
        {
            if (auto *button = qobject_cast<QAbstractButton *>(widget))
                return RuntimeVal(BigDecimal(button->isChecked() ? 1 : 0));
        }
        if (name == "currentIndex")
        {
            if (auto *comboBox = qobject_cast<QComboBox *>(widget))
                return RuntimeVal(BigDecimal(comboBox->currentIndex()));
            if (auto *tabs = qobject_cast<QTabWidget *>(widget))
                return RuntimeVal(BigDecimal(tabs->currentIndex()));
        }
        if (name == "currentText")
        {
            if (auto *comboBox = qobject_cast<QComboBox *>(widget))
                return RuntimeVal(comboBox->currentText().toStdString());
            if (auto *list = qobject_cast<QListWidget *>(widget))
                return RuntimeVal(list->currentItem() ? list->currentItem()->text().toStdString() : std::string());
        }
        if (name == "currentRow")
        {
            if (auto *list = qobject_cast<QListWidget *>(widget))
                return RuntimeVal(BigDecimal(list->currentRow()));
            if (auto *table = qobject_cast<QTableWidget *>(widget))
                return RuntimeVal(BigDecimal(table->currentRow()));
        }
        if (name == "currentColumn")
        {
            if (auto *table = qobject_cast<QTableWidget *>(widget))
                return RuntimeVal(BigDecimal(table->currentColumn()));
        }
        if (name == "rowCount")
        {
            if (auto *table = qobject_cast<QTableWidget *>(widget))
                return RuntimeVal(BigDecimal(table->rowCount()));
        }
        if (name == "columnCount")
        {
            if (auto *table = qobject_cast<QTableWidget *>(widget))
                return RuntimeVal(BigDecimal(table->columnCount()));
        }
        if (name == "date")
        {
            if (auto *dateEdit = qobject_cast<QDateEdit *>(widget))
                return RuntimeVal(dateEdit->date().toString(Qt::ISODate).toStdString());
        }
        if (name == "items")
        {
            Array items;
            if (auto *comboBox = qobject_cast<QComboBox *>(widget))
            {
                for (int i = 0; i < comboBox->count(); ++i)
                    items.emplace_back(comboBox->itemText(i).toStdString());
                return RuntimeVal(std::move(items));
            }
            if (auto *list = qobject_cast<QListWidget *>(widget))
            {
                for (int i = 0; i < list->count(); ++i)
                    items.emplace_back(list->item(i)->text().toStdString());
                return RuntimeVal(std::move(items));
            }
            if (auto *table = qobject_cast<QTableWidget *>(widget))
            {
                for (int row = 0; row < table->rowCount(); ++row)
                    for (int column = 0; column < table->columnCount(); ++column)
                    {
                        const auto *cell = table->item(row, column);
                        items.emplace_back(cell ? cell->text().toStdString() : std::string());
                    }
                return RuntimeVal(std::move(items));
            }
        }
        if (name == "placeholderText")
        {
            if (auto *lineEdit = qobject_cast<QLineEdit *>(widget))
                return RuntimeVal(lineEdit->placeholderText().toStdString());
            if (auto *textEdit = qobject_cast<QTextEdit *>(widget))
                return RuntimeVal(textEdit->placeholderText().toStdString());
        }
        if (name == "readOnly")
        {
            if (auto *lineEdit = qobject_cast<QLineEdit *>(widget))
                return RuntimeVal(BigDecimal(lineEdit->isReadOnly() ? 1 : 0));
            if (auto *textEdit = qobject_cast<QTextEdit *>(widget))
                return RuntimeVal(BigDecimal(textEdit->isReadOnly() ? 1 : 0));
        }

        const QVariant value = widget->property(name.c_str());
        if (!value.isValid())
            throw std::runtime_error("unknown widget property '" + name + "'");
        if (value.metaType().id() == QMetaType::Bool)
            return RuntimeVal(BigDecimal(value.toBool() ? 1 : 0));
        if (value.canConvert<double>())
            return RuntimeVal(BigDecimal(QString::number(value.toDouble()).toStdString()));
        if (value.canConvert<QString>())
            return RuntimeVal(value.toString().toStdString());
        throw std::runtime_error("widget property '" + name + "' cannot be read as a Sand value");
    }

    void trackDependency(QWidget *source, const std::string &property, std::function<void()> update)
    {
        if (!source)
            throw std::runtime_error("cannot track a property on a null widget");
        eventWatcher->watch(source, property, std::move(update));
    }

    void connectSignal(QWidget *widget, const std::string &signal, std::function<void()> callback)
    {
        if (!widget)
            throw std::runtime_error("cannot connect a signal on a null widget");

        if (auto *button = qobject_cast<QAbstractButton *>(widget))
        {
            if (signal == "clicked")
                QObject::connect(button, &QAbstractButton::clicked, eventWatcher.get(), [callback](bool) { callback(); });
            else if (signal == "pressed")
                QObject::connect(button, &QAbstractButton::pressed, eventWatcher.get(), callback);
            else if (signal == "released")
                QObject::connect(button, &QAbstractButton::released, eventWatcher.get(), callback);
            else if (signal == "toggled")
                QObject::connect(button, &QAbstractButton::toggled, eventWatcher.get(), [callback](bool) { callback(); });
            else
                throw std::runtime_error("unsupported button signal '" + signal + "'");
            return;
        }

        if (auto *slider = qobject_cast<QAbstractSlider *>(widget))
        {
            if (signal != "valueChanged")
                throw std::runtime_error("unsupported slider signal '" + signal + "'");
            QObject::connect(slider, &QAbstractSlider::valueChanged, eventWatcher.get(), [callback](int) { callback(); });
            return;
        }

        if (auto *spinBox = qobject_cast<QSpinBox *>(widget))
        {
            if (signal != "valueChanged")
                throw std::runtime_error("unsupported spin box signal '" + signal + "'");
            QObject::connect(spinBox, qOverload<int>(&QSpinBox::valueChanged), eventWatcher.get(), [callback](int) { callback(); });
            return;
        }

        if (auto *lineEdit = qobject_cast<QLineEdit *>(widget))
        {
            if (signal != "textChanged")
                throw std::runtime_error("unsupported text box signal '" + signal + "'");
            QObject::connect(lineEdit, &QLineEdit::textChanged, eventWatcher.get(), [callback](const QString &) { callback(); });
            return;
        }

        if (auto *comboBox = qobject_cast<QComboBox *>(widget))
        {
            if (signal != "currentIndexChanged" && signal != "activated")
                throw std::runtime_error("unsupported combo box signal '" + signal + "'");
            if (signal == "activated")
                QObject::connect(comboBox, qOverload<int>(&QComboBox::activated), eventWatcher.get(), [callback](int) { callback(); });
            else
                QObject::connect(comboBox, qOverload<int>(&QComboBox::currentIndexChanged), eventWatcher.get(), [callback](int) { callback(); });
            return;
        }

        if (auto *list = qobject_cast<QListWidget *>(widget))
        {
            if (signal != "currentRowChanged" && signal != "currentItemChanged")
                throw std::runtime_error("unsupported list view signal '" + signal + "'");
            QObject::connect(list, &QListWidget::currentRowChanged, eventWatcher.get(), [callback](int) { callback(); });
            return;
        }

        if (auto *table = qobject_cast<QTableWidget *>(widget))
        {
            if (signal != "currentCellChanged")
                throw std::runtime_error("unsupported table widget signal '" + signal + "'");
            QObject::connect(table, &QTableWidget::currentCellChanged, eventWatcher.get(),
                             [callback](int, int, int, int) { callback(); });
            return;
        }

        if (auto *dateEdit = qobject_cast<QDateEdit *>(widget))
        {
            if (signal != "dateChanged")
                throw std::runtime_error("unsupported date edit signal '" + signal + "'");
            QObject::connect(dateEdit, &QDateEdit::dateChanged, eventWatcher.get(), [callback](const QDate &) { callback(); });
            return;
        }

        if (auto *tabs = qobject_cast<QTabWidget *>(widget))
        {
            if (signal != "currentChanged")
                throw std::runtime_error("unsupported tab widget signal '" + signal + "'");
            QObject::connect(tabs, &QTabWidget::currentChanged, eventWatcher.get(), [callback](int) { callback(); });
            return;
        }

        throw std::runtime_error("signal '" + signal + "' is not supported by this widget");
    }

    void addTabPage(QWidget *parent, QWidget *page)
    {
        if (auto *tabs = qobject_cast<QTabWidget *>(parent))
            tabs->addTab(page, page->windowTitle());
    }

    static void applyAnchors(QWidget *widget)
    {
        if (!widget)
            return;
        QWidget *parent = widget->parentWidget();
        if (!parent)
            return;
        const int margin = widget->property("anchorMargins").isValid()
                               ? widget->property("anchorMargins").toInt()
                               : 0;
        const bool left = widget->property("anchorLeft").toBool();
        const bool right = widget->property("anchorRight").toBool();
        const bool top = widget->property("anchorTop").toBool();
        const bool bottom = widget->property("anchorBottom").toBool();
        QRect geometry = widget->geometry();
        if (left)
            geometry.setLeft(margin);
        if (right)
            geometry.setRight(parent->rect().width() - margin);
        if (top)
            geometry.setTop(margin);
        if (bottom)
            geometry.setBottom(parent->rect().height() - margin);
        widget->setGeometry(geometry);
    }

    void setProperty(QWidget *widget, const std::string &name, const RuntimeVal &value)
    {
        if (!widget)
            throw std::runtime_error("cannot set a property on a null widget");

        if (name == "anchorLeft" || name == "anchorRight" || name == "anchorTop" || name == "anchorBottom")
        {
            widget->setProperty(name.c_str(), qt_widget_runtime_detail::toBool(value, name));
            applyAnchors(widget);
        }
        else if (name == "anchorMargins")
        {
            widget->setProperty(name.c_str(), qt_widget_runtime_detail::toInt(value, name));
            applyAnchors(widget);
        }
        else if (name == "pixmap")
        {
            auto *label = qobject_cast<QLabel *>(widget);
            auto *string = value.as_str();
            if (!label || !string)
                throw std::runtime_error("widget property 'pixmap' requires Image and a file path");
            QPixmap pixmap(QString::fromStdString(string->value));
            if (pixmap.isNull())
                throw std::runtime_error("cannot load pixmap '" + string->value + "'");
            label->setPixmap(pixmap);
        }
        else if (name == "background")
        {
            if (auto *canvas = dynamic_cast<SandCanvasWidget *>(widget))
                canvas->setBackground(value);
            else
                throw std::runtime_error("widget property 'background' requires Canvas");
        }
        else if (name == "drawLine")
        {
            auto *canvas = dynamic_cast<SandCanvasWidget *>(widget);
            if (!canvas)
                throw std::runtime_error("widget property 'drawLine' requires Canvas");
            canvas->drawLine(value);
        }
        else if (name == "drawRect")
        {
            auto *canvas = dynamic_cast<SandCanvasWidget *>(widget);
            if (!canvas)
                throw std::runtime_error("widget property 'drawRect' requires Canvas");
            canvas->drawRect(value);
        }
        else if (name == "drawText")
        {
            auto *canvas = dynamic_cast<SandCanvasWidget *>(widget);
            if (!canvas)
                throw std::runtime_error("widget property 'drawText' requires Canvas");
            canvas->drawText(value);
        }
        else if (name == "x")
            widget->move(qt_widget_runtime_detail::toInt(value, name), widget->y());
        else if (name == "y")
            widget->move(widget->x(), qt_widget_runtime_detail::toInt(value, name));
        else if (name == "width")
            widget->resize(qt_widget_runtime_detail::toInt(value, name), widget->height());
        else if (name == "height")
            widget->resize(widget->width(), qt_widget_runtime_detail::toInt(value, name));
        else if (name == "isShow" || name == "visible")
            widget->setVisible(qt_widget_runtime_detail::toBool(value, name));
        else if (name == "enabled")
            widget->setEnabled(qt_widget_runtime_detail::toBool(value, name));
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
            else if (auto *textEdit = qobject_cast<QTextEdit *>(widget))
                textEdit->setPlainText(text);
            else if (auto *checkBox = qobject_cast<QCheckBox *>(widget))
                checkBox->setText(text);
            else if (auto *radioButton = qobject_cast<QRadioButton *>(widget))
                radioButton->setText(text);
            else if (auto *groupBox = qobject_cast<QGroupBox *>(widget))
                groupBox->setTitle(text);
            else
                widget->setWindowTitle(text);
        }
        else if (name == "title")
        {
            const auto *string = value.as_str();
            if (!string)
                throw std::runtime_error("widget property 'title' requires a string");
            const QString title = QString::fromStdString(string->value);
            if (auto *groupBox = qobject_cast<QGroupBox *>(widget))
                groupBox->setTitle(title);
            else
                widget->setWindowTitle(title);
        }
        else if (name == "value")
        {
            const int number = qt_widget_runtime_detail::toInt(value, name);
            if (auto *slider = qobject_cast<QAbstractSlider *>(widget))
                slider->setValue(number);
            else if (auto *progress = qobject_cast<QProgressBar *>(widget))
                progress->setValue(number);
            else if (auto *spinBox = qobject_cast<QSpinBox *>(widget))
                spinBox->setValue(number);
            else
                throw std::runtime_error("widget property 'value' requires Slider, ProgressBar, or SpinBox");
        }
        else if (name == "minimum" || name == "maximum")
        {
            const int number = qt_widget_runtime_detail::toInt(value, name);
            const bool minimum = name == "minimum";
            if (auto *slider = qobject_cast<QAbstractSlider *>(widget))
            {
                if (minimum)
                    slider->setMinimum(number);
                else
                    slider->setMaximum(number);
            }
            else if (auto *progress = qobject_cast<QProgressBar *>(widget))
            {
                if (minimum)
                    progress->setMinimum(number);
                else
                    progress->setMaximum(number);
            }
            else if (auto *spinBox = qobject_cast<QSpinBox *>(widget))
            {
                if (minimum)
                    spinBox->setMinimum(number);
                else
                    spinBox->setMaximum(number);
            }
            else
                throw std::runtime_error("widget property '" + name + "' requires Slider, ProgressBar, or SpinBox");
        }
        else if (name == "checked")
        {
            auto *button = qobject_cast<QAbstractButton *>(widget);
            if (!button)
                throw std::runtime_error("widget property 'checked' requires Button, CheckBox, or RadioButton");
            if (qobject_cast<QPushButton *>(button))
                button->setCheckable(true);
            button->setChecked(qt_widget_runtime_detail::toBool(value, name));
        }
        else if (name == "currentIndex")
        {
            auto *comboBox = qobject_cast<QComboBox *>(widget);
            const int index = qt_widget_runtime_detail::toInt(value, name);
            if (comboBox)
                comboBox->setCurrentIndex(index);
            else if (auto *tabs = qobject_cast<QTabWidget *>(widget))
                tabs->setCurrentIndex(index);
            else
                throw std::runtime_error("widget property 'currentIndex' requires ComboBox or TabWidget");
        }
        else if (name == "placeholderText")
        {
            const auto *string = value.as_str();
            if (!string)
                throw std::runtime_error("widget property 'placeholderText' requires a string");
            const QString text = QString::fromStdString(string->value);
            if (auto *lineEdit = qobject_cast<QLineEdit *>(widget))
                lineEdit->setPlaceholderText(text);
            else if (auto *textEdit = qobject_cast<QTextEdit *>(widget))
                textEdit->setPlaceholderText(text);
            else
                throw std::runtime_error("widget property 'placeholderText' requires TextBox or TextArea");
        }
        else if (name == "readOnly")
        {
            const bool readOnly = qt_widget_runtime_detail::toBool(value, name);
            if (auto *lineEdit = qobject_cast<QLineEdit *>(widget))
                lineEdit->setReadOnly(readOnly);
            else if (auto *textEdit = qobject_cast<QTextEdit *>(widget))
                textEdit->setReadOnly(readOnly);
            else
                throw std::runtime_error("widget property 'readOnly' requires TextBox or TextArea");
        }
        else if (name == "orientation")
        {
            const auto *string = value.as_str();
            auto *slider = qobject_cast<QSlider *>(widget);
            if (!slider || !string)
                throw std::runtime_error("widget property 'orientation' requires Slider and a string");
            if (string->value == "horizontal")
                slider->setOrientation(Qt::Horizontal);
            else if (string->value == "vertical")
                slider->setOrientation(Qt::Vertical);
            else
                throw std::runtime_error("widget orientation must be 'horizontal' or 'vertical'");
        }
        else if (name == "items")
        {
            QStringList items;
            if (const auto *array = value.as_array())
            {
                for (const auto &item : array->value)
                {
                    const auto *text = item.as_str();
                    if (!text)
                        throw std::runtime_error("widget property 'items' requires strings");
                    items.push_back(QString::fromStdString(text->value));
                }
            }
            else if (const auto *string = value.as_str())
            {
                std::istringstream input(string->value);
                std::string item;
                while (std::getline(input, item, ','))
                    items.push_back(QString::fromStdString(item));
            }
            else
                throw std::runtime_error("widget property 'items' requires a string or array of strings");

            if (auto *comboBox = qobject_cast<QComboBox *>(widget))
            {
                comboBox->clear();
                comboBox->addItems(items);
            }
            else if (auto *list = qobject_cast<QListWidget *>(widget))
            {
                list->clear();
                list->addItems(items);
            }
            else if (auto *table = qobject_cast<QTableWidget *>(widget))
            {
                if (table->columnCount() == 0)
                    table->setColumnCount(1);
                table->clearContents();
                table->setRowCount(static_cast<int>((items.size() + table->columnCount() - 1) / table->columnCount()));
                for (qsizetype i = 0; i < items.size(); ++i)
                    table->setItem(static_cast<int>(i / table->columnCount()),
                                   static_cast<int>(i % table->columnCount()),
                                   new QTableWidgetItem(items[i]));
            }
            else
                throw std::runtime_error("widget property 'items' requires ListView or TableWidget");
        }
        else if (name == "currentRow")
        {
            const int row = qt_widget_runtime_detail::toInt(value, name);
            if (auto *list = qobject_cast<QListWidget *>(widget))
                list->setCurrentRow(row);
            else if (auto *table = qobject_cast<QTableWidget *>(widget))
                table->setCurrentCell(row, std::max(0, table->currentColumn()));
            else
                throw std::runtime_error("widget property 'currentRow' requires ListView or TableWidget");
        }
        else if (name == "currentColumn")
        {
            auto *table = qobject_cast<QTableWidget *>(widget);
            if (!table)
                throw std::runtime_error("widget property 'currentColumn' requires TableWidget");
            table->setCurrentCell(std::max(0, table->currentRow()), qt_widget_runtime_detail::toInt(value, name));
        }
        else if (name == "rowCount" || name == "columnCount")
        {
            auto *table = qobject_cast<QTableWidget *>(widget);
            if (!table)
                throw std::runtime_error("widget property '" + name + "' requires TableWidget");
            const int count = qt_widget_runtime_detail::toInt(value, name);
            if (count < 0)
                throw std::runtime_error("widget property '" + name + "' cannot be negative");
            if (name == "rowCount")
                table->setRowCount(count);
            else
                table->setColumnCount(count);
        }
        else if (name == "date")
        {
            auto *dateEdit = qobject_cast<QDateEdit *>(widget);
            const auto *string = value.as_str();
            if (!dateEdit || !string)
                throw std::runtime_error("widget property 'date' requires DateEdit and a string");
            const QDate date = QDate::fromString(QString::fromStdString(string->value), Qt::ISODate);
            if (!date.isValid())
                throw std::runtime_error("widget property 'date' must use YYYY-MM-DD format");
            dateEdit->setDate(date);
        }
        else
            widget->setProperty(name.c_str(), qt_widget_runtime_detail::toVariant(value));
    }

    void show(QWidget *widget)
    {
        if (!widget)
            throw std::runtime_error("cannot show a null widget");
        widget->show();
    }

    RuntimeVal messageBox(const std::string &title, const std::string &text)
    {
        qt_widget_runtime_detail::ensureApplication();
        QMessageBox::information(nullptr, QString::fromStdString(title), QString::fromStdString(text));
        return RuntimeVal();
    }

    bool confirmDialog(const std::string &title, const std::string &text)
    {
        qt_widget_runtime_detail::ensureApplication();
        return QMessageBox::question(nullptr, QString::fromStdString(title), QString::fromStdString(text),
                                     QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
    }

    std::string openFileDialog(const std::string &title)
    {
        qt_widget_runtime_detail::ensureApplication();
        return QFileDialog::getOpenFileName(nullptr, QString::fromStdString(title)).toStdString();
    }

    std::string saveFileDialog(const std::string &title)
    {
        qt_widget_runtime_detail::ensureApplication();
        return QFileDialog::getSaveFileName(nullptr, QString::fromStdString(title)).toStdString();
    }

    std::string inputDialog(const std::string &title, const std::string &prompt)
    {
        qt_widget_runtime_detail::ensureApplication();
        bool ok = false;
        QString value = QInputDialog::getText(nullptr, QString::fromStdString(title),
                                              QString::fromStdString(prompt), QLineEdit::Normal, QString(), &ok);
        return ok ? value.toStdString() : std::string();
    }

    int runEventLoop()
    {
        auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
        if (!application)
            return 0;

        for (const auto &widget : rootWidgets)
        {
            if (widget && widget->isVisible())
                return application->exec();
        }
        if (!timers.empty())
        {
            const auto started = std::chrono::steady_clock::now();
            while (std::chrono::steady_clock::now() - started < std::chrono::seconds(30))
            {
                application->processEvents(QEventLoop::AllEvents, 10);
                bool active = false;
                for (const auto &timer : timers)
                    active = active || timer.second->isActive();
                if (!active)
                    break;
            }
        }
        return 0;
    }

private:
    class EventWatcher : public QObject
    {
    public:
        void watch(QWidget *widget, const std::string &property, std::function<void()> update)
        {
            widget->installEventFilter(this);
            callbacks[widget][property].push_back(std::move(update));
            if (!connectedWidgets.insert(widget).second)
                return;
            if (auto *slider = qobject_cast<QAbstractSlider *>(widget))
                QObject::connect(slider, &QAbstractSlider::valueChanged, this, [this, widget](int) { notify(widget, {"value"}); });
            else if (auto *progress = qobject_cast<QProgressBar *>(widget))
                QObject::connect(progress, &QProgressBar::valueChanged, this, [this, widget](int) { notify(widget, {"value"}); });
            else if (auto *spinBox = qobject_cast<QSpinBox *>(widget))
                QObject::connect(spinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this, widget](int) { notify(widget, {"value"}); });
            else if (auto *comboBox = qobject_cast<QComboBox *>(widget))
                QObject::connect(comboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, widget](int) { notify(widget, {"currentIndex", "currentText"}); });
            else if (auto *button = qobject_cast<QAbstractButton *>(widget))
                QObject::connect(button, &QAbstractButton::toggled, this, [this, widget](bool) { notify(widget, {"checked"}); });
            else if (auto *lineEdit = qobject_cast<QLineEdit *>(widget))
                QObject::connect(lineEdit, &QLineEdit::textChanged, this, [this, widget](const QString &) { notify(widget, {"text"}); });
            else if (auto *textEdit = qobject_cast<QTextEdit *>(widget))
                QObject::connect(textEdit, &QTextEdit::textChanged, this, [this, widget]() { notify(widget, {"text"}); });
            else if (auto *list = qobject_cast<QListWidget *>(widget))
                QObject::connect(list, &QListWidget::currentRowChanged, this, [this, widget](int) { notify(widget, {"currentRow", "currentText"}); });
            else if (auto *table = qobject_cast<QTableWidget *>(widget))
                QObject::connect(table, &QTableWidget::currentCellChanged, this, [this, widget](int, int, int, int) { notify(widget, {"currentRow", "currentColumn"}); });
            else if (auto *dateEdit = qobject_cast<QDateEdit *>(widget))
                QObject::connect(dateEdit, &QDateEdit::dateChanged, this, [this, widget](const QDate &) { notify(widget, {"date"}); });
            else if (auto *tabs = qobject_cast<QTabWidget *>(widget))
                QObject::connect(tabs, &QTabWidget::currentChanged, this, [this, widget](int) { notify(widget, {"currentIndex"}); });
        }

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override
        {
            auto *widget = qobject_cast<QWidget *>(watched);
            if (!widget)
                return QObject::eventFilter(watched, event);
            if (dispatchingWidgets.find(widget) != dispatchingWidgets.end())
                return QObject::eventFilter(watched, event);

            std::vector<std::string> changedProperties;
            switch (event->type())
            {
            case QEvent::Resize:
                changedProperties = {"width", "weith", "height"};
                break;
            case QEvent::Move:
                changedProperties = {"x", "y"};
                break;
            case QEvent::Show:
            case QEvent::Hide:
                changedProperties = {"isShow", "visible"};
                break;
            case QEvent::DynamicPropertyChange:
                changedProperties.push_back(
                    static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName().toStdString());
                break;
            default:
                break;
            }

            notify(widget, changedProperties);
            return QObject::eventFilter(watched, event);
        }

    private:
        void notify(QWidget *widget, const std::vector<std::string> &changedProperties)
        {
            auto widgetIt = callbacks.find(widget);
            if (widgetIt == callbacks.end() || dispatchingWidgets.find(widget) != dispatchingWidgets.end())
                return;
            {
                std::vector<std::function<void()>> updates;
                for (const auto &property : changedProperties)
                {
                    auto propertyIt = widgetIt->second.find(property);
                    if (propertyIt != widgetIt->second.end())
                        updates.insert(updates.end(), propertyIt->second.begin(), propertyIt->second.end());
                }
                dispatchingWidgets.insert(widget);
                for (const auto &update : updates)
                {
                    try
                    {
                        update();
                    }
                    catch (const std::exception &error)
                    {
                        
                    }
                }
                dispatchingWidgets.erase(widget);
            }
        }

        std::unordered_map<QWidget *, std::unordered_map<std::string, std::vector<std::function<void()>>>> callbacks;
        std::unordered_set<QWidget *> dispatchingWidgets;
        std::unordered_set<QWidget *> connectedWidgets;
    };

    std::vector<QPointer<QWidget>> rootWidgets;
    std::unique_ptr<EventWatcher> eventWatcher;
    std::unordered_map<int, std::shared_ptr<QTimer>> timers;
    int nextTimerId = 1;
};
#endif