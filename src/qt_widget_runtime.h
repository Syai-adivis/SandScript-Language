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
#include <QDateTime>
#include <QDateEdit>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDynamicPropertyChangeEvent>
#include <QEvent>
#include <QFontComboBox>
#include <QGraphicsView>
#include <QGroupBox>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMdiArea>
#include <QObject>
#include <QProgressBar>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QTextEdit>
#include <QTextBrowser>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTimeEdit>
#include <QToolBox>
#include <QToolButton>
#include <QCommandLinkButton>
#include <QColumnView>
#include <QTime>
#include <QTreeView>
#include <QUndoView>
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
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QLayout>
#include <QSpacerItem>
#include <QSizePolicy>

#ifdef SAND_ENABLE_QT_OPENGL_WIDGETS
#include <QOpenGLWidget>
#endif

#ifdef SAND_ENABLE_QT_QUICK_WIDGETS
#include <QQuickWidget>
#endif

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

inline double toDouble(const RuntimeVal &value, const std::string &name)
{
    const auto *number = value.as_num();
    if (!number)
        throw std::runtime_error("widget property '" + name + "' requires a number");
    bool ok = false;
    const double converted = QString::fromStdString(number->value.to_string()).toDouble(&ok);
    if (!ok)
        throw std::runtime_error("widget property '" + name + "' is outside Qt's supported range");
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

        if (type == "VerticalLayout" || type == "HorizontalLayout" ||
            type == "GridLayout" || type == "FormLayout")
        {
            QWidget *layoutHost = parent ? parent : new QWidget();
            if (layoutHost->layout())
                throw std::runtime_error("a widget can have only one layout");
            if (type == "VerticalLayout")
                layoutHost->setLayout(new QVBoxLayout());
            else if (type == "HorizontalLayout")
                layoutHost->setLayout(new QHBoxLayout());
            else if (type == "GridLayout")
                layoutHost->setLayout(new QGridLayout());
            else
                layoutHost->setLayout(new QFormLayout());
            layoutHost->setProperty("sandLayoutContainer", true);
            if (!parent)
            {
                layoutHost->resize(640, 480);
                rootWidgets.emplace_back(layoutHost);
            }
            return layoutHost;
        }

        QWidget *widget = nullptr;
        if (type == "Window")
            widget = new QWidget(parent, parent ? Qt::Widget : Qt::Window);
        else if (type == "Widget")
            widget = new QWidget(parent);
        else if (type == "Label")
            widget = new QLabel(parent);
        else if (type == "Button")
            widget = new QPushButton(parent);
        else if (type == "ToolButton")
            widget = new QToolButton(parent);
        else if (type == "CommandLinkButton")
            widget = new QCommandLinkButton(parent);
        else if (type == "DialogButtonBox")
            widget = new QDialogButtonBox(parent);
        else if (type == "TextBox")
            widget = new QLineEdit(parent);
        else if (type == "TextArea")
            widget = new QTextEdit(parent);
        else if (type == "TextBrowser")
            widget = new QTextBrowser(parent);
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
        else if (type == "DoubleSpinBox")
            widget = new QDoubleSpinBox(parent);
        else if (type == "TimeEdit")
            widget = new QTimeEdit(parent);
        else if (type == "DateTimeEdit")
            widget = new QDateTimeEdit(parent);
        else if (type == "FontComboBox")
            widget = new QFontComboBox(parent);
        else if (type == "KeySequenceEdit")
            widget = new QKeySequenceEdit(parent);
        else if (type == "HorizontalScrollBar")
            widget = new QScrollBar(Qt::Horizontal, parent);
        else if (type == "VerticalScrollBar")
            widget = new QScrollBar(Qt::Vertical, parent);
        else if (type == "HorizontalSlider")
            widget = new QSlider(Qt::Horizontal, parent);
        else if (type == "VerticalSlider")
            widget = new QSlider(Qt::Vertical, parent);
        else if (type == "GroupBox")
            widget = new QGroupBox(parent);
        else if (type == "ListView" || type == "ModelListView")
            widget = new QListView(parent);
        else if (type == "TableWidget")
            widget = new QTableWidget(parent);
        else if (type == "DateEdit")
            widget = new QDateEdit(parent);
        else if (type == "ListWidget")
            widget = new QListWidget(parent);
        else if (type == "TreeView" || type == "ModelTreeView")
            widget = new QTreeView(parent);
        else if (type == "TableView")
            widget = new QTableView(parent);
        else if (type == "ColumnView")
            widget = new QColumnView(parent);
        else if (type == "UndoView")
            widget = new QUndoView(parent);
        else if (type == "TreeWidget")
            widget = new QTreeWidget(parent);
        else if (type == "TabWidget")
            widget = new QTabWidget(parent);
        else if (type == "ToolBox")
            widget = new QToolBox(parent);
        else if (type == "MdiArea")
            widget = new QMdiArea(parent);
        else if (type == "DockWidget")
            widget = new QDockWidget(parent);
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
        else if (type == "StackWidget")
            widget = new QStackedWidget(parent);
        else if (type == "MenuBar")
            widget = new QMenuBar(parent);
        else if (type == "ToolBar")
            widget = new QToolBar(parent);
        else if (type == "StatusBar")
            widget = new QStatusBar(parent);
        else if (type == "GraphicsView")
            widget = new QGraphicsView(parent);
        else if (type == "HorizontalLine" || type == "VerticalLine")
        {
            auto *line = new QFrame(parent);
            line->setFrameShape(type == "HorizontalLine" ? QFrame::HLine : QFrame::VLine);
            line->setFrameShadow(QFrame::Sunken);
            widget = line;
        }
        else if (type == "HorizontalSpacer" || type == "VerticalSpacer")
        {
            widget = new QWidget(parent);
            widget->hide();
            widget->setProperty("sandSpacer", type == "HorizontalSpacer" ? "horizontal" : "vertical");
            widget->setSizePolicy(type == "HorizontalSpacer" ? QSizePolicy::Expanding : QSizePolicy::Minimum,
                                  type == "VerticalSpacer" ? QSizePolicy::Expanding : QSizePolicy::Minimum);
        }
#ifdef SAND_ENABLE_QT_OPENGL_WIDGETS
        else if (type == "OpenGLWidget")
            widget = new QOpenGLWidget(parent);
#else
        else if (type == "OpenGLWidget")
            throw std::runtime_error("OpenGLWidget is unavailable; rebuild with Qt6 OpenGLWidgets");
#endif
#ifdef SAND_ENABLE_QT_QUICK_WIDGETS
        else if (type == "QuickWidget" || type == "QmlWidget")
            widget = new QQuickWidget(parent);
#else
        else if (type == "QuickWidget" || type == "QmlWidget")
            throw std::runtime_error("QuickWidget is unavailable; rebuild with Qt6 QuickWidgets");
#endif
        else if (type == "Image")
            widget = new QLabel(parent);
        else if (type == "Canvas")
            widget = new SandCanvasWidget(parent);
        else
            throw std::runtime_error("unknown Qt widget type '" + type + "'");

        if (type == "ListView" || type == "ModelListView" || type == "TreeView" ||
            type == "ModelTreeView" || type == "TableView" || type == "ColumnView")
        {
            auto *model = new QStandardItemModel(widget);
            model->setColumnCount(1);
            qobject_cast<QAbstractItemView *>(widget)->setModel(model);
        }

        if (!parent)
        {
            if (type == "Window" || widget->property("sandLayoutContainer").toBool())
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
            if (auto *plainText = qobject_cast<QPlainTextEdit *>(widget))
                return RuntimeVal(plainText->toPlainText().toStdString());
            if (auto *checkBox = qobject_cast<QCheckBox *>(widget))
                return RuntimeVal(checkBox->text().toStdString());
            if (auto *radioButton = qobject_cast<QRadioButton *>(widget))
                return RuntimeVal(radioButton->text().toStdString());
            if (auto *button = qobject_cast<QAbstractButton *>(widget))
                return RuntimeVal(button->text().toStdString());
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
            if (auto *spinBox = qobject_cast<QDoubleSpinBox *>(widget))
                return RuntimeVal(BigDecimal(QString::number(spinBox->value(), 'g', 15).toStdString()));
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
            if (auto *spinBox = qobject_cast<QDoubleSpinBox *>(widget))
                return RuntimeVal(BigDecimal(QString::number(minimum ? spinBox->minimum() : spinBox->maximum(), 'g', 15).toStdString()));
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
            if (auto *view = qobject_cast<QAbstractItemView *>(widget))
                return RuntimeVal(BigDecimal(view->currentIndex().row()));
        }
        if (name == "currentText")
        {
            if (auto *comboBox = qobject_cast<QComboBox *>(widget))
                return RuntimeVal(comboBox->currentText().toStdString());
            if (auto *list = qobject_cast<QListWidget *>(widget))
                return RuntimeVal(list->currentItem() ? list->currentItem()->text().toStdString() : std::string());
            if (auto *view = qobject_cast<QAbstractItemView *>(widget))
                return RuntimeVal(view->currentIndex().data().toString().toStdString());
            if (auto *fontCombo = qobject_cast<QFontComboBox *>(widget))
                return RuntimeVal(fontCombo->currentFont().family().toStdString());
        }
        if (name == "currentRow")
        {
            if (auto *list = qobject_cast<QListWidget *>(widget))
                return RuntimeVal(BigDecimal(list->currentRow()));
            if (auto *table = qobject_cast<QTableWidget *>(widget))
                return RuntimeVal(BigDecimal(table->currentRow()));
            if (auto *view = qobject_cast<QAbstractItemView *>(widget))
                return RuntimeVal(BigDecimal(view->currentIndex().row()));
        }
        if (name == "currentColumn")
        {
            if (auto *table = qobject_cast<QTableWidget *>(widget))
                return RuntimeVal(BigDecimal(table->currentColumn()));
            if (auto *view = qobject_cast<QAbstractItemView *>(widget))
                return RuntimeVal(BigDecimal(view->currentIndex().column()));
        }
        if (name == "rowCount")
        {
            if (auto *table = qobject_cast<QTableWidget *>(widget))
                return RuntimeVal(BigDecimal(table->rowCount()));
            if (auto *view = qobject_cast<QAbstractItemView *>(widget))
                return RuntimeVal(BigDecimal(view->model() ? view->model()->rowCount() : 0));
        }
        if (name == "columnCount")
        {
            if (auto *table = qobject_cast<QTableWidget *>(widget))
                return RuntimeVal(BigDecimal(table->columnCount()));
            if (auto *view = qobject_cast<QAbstractItemView *>(widget))
                return RuntimeVal(BigDecimal(view->model() ? view->model()->columnCount() : 0));
        }
        if (name == "date")
        {
            if (auto *dateEdit = qobject_cast<QDateEdit *>(widget))
                return RuntimeVal(dateEdit->date().toString(Qt::ISODate).toStdString());
        }
        if (name == "time")
        {
            if (auto *dateTimeEdit = qobject_cast<QDateTimeEdit *>(widget))
                return RuntimeVal(dateTimeEdit->time().toString(Qt::ISODateWithMs).toStdString());
        }
        if (name == "dateTime")
        {
            if (auto *dateTimeEdit = qobject_cast<QDateTimeEdit *>(widget))
                return RuntimeVal(dateTimeEdit->dateTime().toString(Qt::ISODateWithMs).toStdString());
        }
        if (name == "currentFont")
        {
            if (auto *fontCombo = qobject_cast<QFontComboBox *>(widget))
                return RuntimeVal(fontCombo->currentFont().family().toStdString());
        }
        if (name == "keySequence")
        {
            if (auto *keySequence = qobject_cast<QKeySequenceEdit *>(widget))
                return RuntimeVal(keySequence->keySequence().toString(QKeySequence::NativeText).toStdString());
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
            if (auto *view = qobject_cast<QAbstractItemView *>(widget))
            {
                if (!view->model())
                    return RuntimeVal(std::move(items));
                for (int row = 0; row < view->model()->rowCount(); ++row)
                    for (int column = 0; column < view->model()->columnCount(); ++column)
                        items.emplace_back(view->model()->data(view->model()->index(row, column)).toString().toStdString());
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

        if (auto *spinBox = qobject_cast<QDoubleSpinBox *>(widget))
        {
            if (signal != "valueChanged")
                throw std::runtime_error("unsupported double spin box signal '" + signal + "'");
            QObject::connect(spinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), eventWatcher.get(), [callback](double) { callback(); });
            return;
        }

        if (auto *lineEdit = qobject_cast<QLineEdit *>(widget))
        {
            if (signal != "textChanged")
                throw std::runtime_error("unsupported text box signal '" + signal + "'");
            QObject::connect(lineEdit, &QLineEdit::textChanged, eventWatcher.get(), [callback](const QString &) { callback(); });
            return;
        }

        if (auto *plainText = qobject_cast<QPlainTextEdit *>(widget))
        {
            if (signal != "textChanged")
                throw std::runtime_error("unsupported plain text signal '" + signal + "'");
            QObject::connect(plainText, &QPlainTextEdit::textChanged, eventWatcher.get(), callback);
            return;
        }

        if (auto *keySequenceEdit = qobject_cast<QKeySequenceEdit *>(widget))
        {
            if (signal != "keySequenceChanged")
                throw std::runtime_error("unsupported key sequence signal '" + signal + "'");
            QObject::connect(keySequenceEdit, &QKeySequenceEdit::keySequenceChanged, eventWatcher.get(),
                             [callback](const QKeySequence &) { callback(); });
            return;
        }

        if (auto *fontCombo = qobject_cast<QFontComboBox *>(widget))
        {
            if (signal != "currentFontChanged")
                throw std::runtime_error("unsupported font combo box signal '" + signal + "'");
            QObject::connect(fontCombo, &QFontComboBox::currentFontChanged, eventWatcher.get(),
                             [callback](const QFont &) { callback(); });
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

        if (auto *view = qobject_cast<QAbstractItemView *>(widget))
        {
            if (signal != "currentChanged" && signal != "currentIndexChanged" &&
                signal != "currentRowChanged" && signal != "currentCellChanged")
                throw std::runtime_error("unsupported item view signal '" + signal + "'");
            if (!view->selectionModel())
                throw std::runtime_error("item view signal requires a model");
            QObject::connect(view->selectionModel(), &QItemSelectionModel::currentChanged, eventWatcher.get(),
                             [callback](const QModelIndex &, const QModelIndex &) { callback(); });
            return;
        }

        if (auto *dateTimeEdit = qobject_cast<QDateTimeEdit *>(widget))
        {
            if (signal == "dateChanged")
                QObject::connect(dateTimeEdit, &QDateTimeEdit::dateChanged, eventWatcher.get(), [callback](const QDate &) { callback(); });
            else if (signal == "timeChanged")
                QObject::connect(dateTimeEdit, &QDateTimeEdit::timeChanged, eventWatcher.get(), [callback](const QTime &) { callback(); });
            else if (signal == "dateTimeChanged")
                QObject::connect(dateTimeEdit, &QDateTimeEdit::dateTimeChanged, eventWatcher.get(), [callback](const QDateTime &) { callback(); });
            else
                throw std::runtime_error("unsupported date/time edit signal '" + signal + "'");
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
        if (!parent || !page || parent == page)
            return;
        if (parent->property("sandLayoutContainer").toBool())
        {
            QLayout *layout = parent->layout();
            const QString spacer = page->property("sandSpacer").toString();
            if (spacer == "horizontal" || spacer == "vertical")
            {
                layout->addItem(new QSpacerItem(
                    0, 0,
                    spacer == "horizontal" ? QSizePolicy::Expanding : QSizePolicy::Minimum,
                    spacer == "vertical" ? QSizePolicy::Expanding : QSizePolicy::Minimum));
            }
            else if (auto *form = qobject_cast<QFormLayout *>(layout))
                form->addRow(page);
            else if (auto *grid = qobject_cast<QGridLayout *>(layout))
            {
                const int row = grid->property("sandNextRow").toInt();
                grid->addWidget(page, row, 0);
                grid->setProperty("sandNextRow", row + 1);
            }
            else
                layout->addWidget(page);
        }
        else if (auto *tabs = qobject_cast<QTabWidget *>(parent))
            tabs->addTab(page, page->windowTitle());
        else if (auto *toolBox = qobject_cast<QToolBox *>(parent))
            toolBox->addItem(page, page->windowTitle());
        else if (auto *stack = qobject_cast<QStackedWidget *>(parent))
            stack->addWidget(page);
        else if (auto *splitter = qobject_cast<QSplitter *>(parent))
            splitter->addWidget(page);
        else if (auto *mdiArea = qobject_cast<QMdiArea *>(parent))
            mdiArea->addSubWindow(page);
        else if (auto *dock = qobject_cast<QDockWidget *>(parent))
        {
            if (dock->widget())
                throw std::runtime_error("DockWidget can contain only one child widget");
            dock->setWidget(page);
        }
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
            else if (auto *plainText = qobject_cast<QPlainTextEdit *>(widget))
                plainText->setPlainText(text);
            else if (auto *checkBox = qobject_cast<QCheckBox *>(widget))
                checkBox->setText(text);
            else if (auto *radioButton = qobject_cast<QRadioButton *>(widget))
                radioButton->setText(text);
            else if (auto *button = qobject_cast<QAbstractButton *>(widget))
                button->setText(text);
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
            if (auto *spinBox = qobject_cast<QDoubleSpinBox *>(widget))
                spinBox->setValue(qt_widget_runtime_detail::toDouble(value, name));
            else
            {
                const int number = qt_widget_runtime_detail::toInt(value, name);
                if (auto *slider = qobject_cast<QAbstractSlider *>(widget))
                    slider->setValue(number);
                else if (auto *progress = qobject_cast<QProgressBar *>(widget))
                    progress->setValue(number);
                else if (auto *spinBox = qobject_cast<QSpinBox *>(widget))
                    spinBox->setValue(number);
                else
                    throw std::runtime_error("widget property 'value' requires Slider, ProgressBar, or a spin box");
            }
        }
        else if (name == "minimum" || name == "maximum")
        {
            const bool minimum = name == "minimum";
            if (auto *spinBox = qobject_cast<QDoubleSpinBox *>(widget))
            {
                const double converted = qt_widget_runtime_detail::toDouble(value, name);
                if (minimum)
                    spinBox->setMinimum(converted);
                else
                    spinBox->setMaximum(converted);
            }
            else
            {
                const int number = qt_widget_runtime_detail::toInt(value, name);
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
                    throw std::runtime_error("widget property '" + name + "' requires Slider, ProgressBar, or a spin box");
            }
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
            else if (auto *view = qobject_cast<QAbstractItemView *>(widget))
            {
                if (!view->model())
                    throw std::runtime_error("widget property 'currentIndex' requires an item view model");
                view->setCurrentIndex(view->model()->index(index, 0));
            }
            else
                throw std::runtime_error("widget property 'currentIndex' requires a combo box, tab, or item view");
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
            auto *slider = qobject_cast<QAbstractSlider *>(widget);
            if (!slider || !string)
                throw std::runtime_error("widget property 'orientation' requires a slider or scroll bar and a string");
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
            else if (auto *view = qobject_cast<QAbstractItemView *>(widget);
                     view && !qobject_cast<QTableWidget *>(widget))
            {
                auto *model = qobject_cast<QStandardItemModel *>(view->model());
                if (!model)
                    throw std::runtime_error("widget property 'items' requires a standard item model");
                if (model->columnCount() == 0)
                    model->setColumnCount(1);
                model->removeRows(0, model->rowCount());
                const int columns = model->columnCount();
                model->setRowCount(static_cast<int>((items.size() + columns - 1) / columns));
                for (qsizetype i = 0; i < items.size(); ++i)
                    model->setItem(static_cast<int>(i / columns), static_cast<int>(i % columns),
                                   new QStandardItem(items[i]));
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
                throw std::runtime_error("widget property 'items' requires a list or table view");
        }
        else if (name == "currentRow")
        {
            const int row = qt_widget_runtime_detail::toInt(value, name);
            if (auto *list = qobject_cast<QListWidget *>(widget))
                list->setCurrentRow(row);
            else if (auto *table = qobject_cast<QTableWidget *>(widget))
                table->setCurrentCell(row, std::max(0, table->currentColumn()));
            else if (auto *view = qobject_cast<QAbstractItemView *>(widget))
            {
                if (!view->model())
                    throw std::runtime_error("widget property 'currentRow' requires an item view model");
                view->setCurrentIndex(view->model()->index(row, std::max(0, view->currentIndex().column())));
            }
            else
                throw std::runtime_error("widget property 'currentRow' requires an item view");
        }
        else if (name == "currentColumn")
        {
            auto *table = qobject_cast<QTableWidget *>(widget);
            const int column = qt_widget_runtime_detail::toInt(value, name);
            if (table)
                table->setCurrentCell(std::max(0, table->currentRow()), column);
            else if (auto *view = qobject_cast<QAbstractItemView *>(widget))
            {
                if (!view->model())
                    throw std::runtime_error("widget property 'currentColumn' requires an item view model");
                view->setCurrentIndex(view->model()->index(std::max(0, view->currentIndex().row()), column));
            }
            else
                throw std::runtime_error("widget property 'currentColumn' requires a table view");
        }
        else if (name == "rowCount" || name == "columnCount")
        {
            auto *table = qobject_cast<QTableWidget *>(widget);
            const int count = qt_widget_runtime_detail::toInt(value, name);
            if (count < 0)
                throw std::runtime_error("widget property '" + name + "' cannot be negative");
            if (table)
            {
                if (name == "rowCount")
                    table->setRowCount(count);
                else
                    table->setColumnCount(count);
            }
            else if (auto *view = qobject_cast<QAbstractItemView *>(widget))
            {
                QStandardItemModel *model = qobject_cast<QStandardItemModel *>(view->model());
                if (!model)
                    throw std::runtime_error("widget property '" + name + "' requires an item view model");
                if (name == "rowCount")
                    model->setRowCount(count);
                else
                    model->setColumnCount(count);
            }
            else
                throw std::runtime_error("widget property '" + name + "' requires a table or item view");
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
        else if (name == "time")
        {
            auto *dateTimeEdit = qobject_cast<QDateTimeEdit *>(widget);
            const auto *string = value.as_str();
            if (!dateTimeEdit || !string)
                throw std::runtime_error("widget property 'time' requires TimeEdit or DateTimeEdit and a string");
            const QTime time = QTime::fromString(QString::fromStdString(string->value), Qt::ISODateWithMs);
            if (!time.isValid())
                throw std::runtime_error("widget property 'time' must use HH:mm:ss[.zzz] format");
            dateTimeEdit->setTime(time);
        }
        else if (name == "dateTime")
        {
            auto *dateTimeEdit = qobject_cast<QDateTimeEdit *>(widget);
            const auto *string = value.as_str();
            if (!dateTimeEdit || !string)
                throw std::runtime_error("widget property 'dateTime' requires DateTimeEdit and a string");
            const QDateTime dateTime = QDateTime::fromString(QString::fromStdString(string->value), Qt::ISODateWithMs);
            if (!dateTime.isValid())
                throw std::runtime_error("widget property 'dateTime' must use ISO 8601 format");
            dateTimeEdit->setDateTime(dateTime);
        }
        else if (name == "currentFont" || name == "currentText")
        {
            auto *fontCombo = qobject_cast<QFontComboBox *>(widget);
            const auto *string = value.as_str();
            if (!fontCombo || !string)
                throw std::runtime_error("widget property '" + name + "' requires FontComboBox and a string");
            QFont font = fontCombo->currentFont();
            font.setFamily(QString::fromStdString(string->value));
            fontCombo->setCurrentFont(font);
        }
        else if (name == "keySequence")
        {
            auto *keySequenceEdit = qobject_cast<QKeySequenceEdit *>(widget);
            const auto *string = value.as_str();
            if (!keySequenceEdit || !string)
                throw std::runtime_error("widget property 'keySequence' requires KeySequenceEdit and a string");
            keySequenceEdit->setKeySequence(QKeySequence::fromString(QString::fromStdString(string->value),
                                                                      QKeySequence::PortableText));
        }
        else if (name == "buttons")
        {
            auto *buttonBox = qobject_cast<QDialogButtonBox *>(widget);
            if (!buttonBox)
                throw std::runtime_error("widget property 'buttons' requires DialogButtonBox");

            QStringList names;
            if (const auto *array = value.as_array())
            {
                for (const auto &item : array->value)
                {
                    const auto *text = item.as_str();
                    if (!text)
                        throw std::runtime_error("widget property 'buttons' requires strings");
                    names.push_back(QString::fromStdString(text->value));
                }
            }
            else if (const auto *string = value.as_str())
                names = QString::fromStdString(string->value).split(',');
            else
                throw std::runtime_error("widget property 'buttons' requires a string or array of strings");

            QDialogButtonBox::StandardButtons buttons;
            for (const QString &name : names)
            {
                const QString normalized = name.trimmed().toLower();
                if (normalized == "ok")
                    buttons |= QDialogButtonBox::Ok;
                else if (normalized == "cancel")
                    buttons |= QDialogButtonBox::Cancel;
                else if (normalized == "yes")
                    buttons |= QDialogButtonBox::Yes;
                else if (normalized == "no")
                    buttons |= QDialogButtonBox::No;
                else if (normalized == "save")
                    buttons |= QDialogButtonBox::Save;
                else if (normalized == "open")
                    buttons |= QDialogButtonBox::Open;
                else if (normalized == "apply")
                    buttons |= QDialogButtonBox::Apply;
                else if (normalized == "close")
                    buttons |= QDialogButtonBox::Close;
                else if (normalized == "reset")
                    buttons |= QDialogButtonBox::Reset;
                else if (normalized == "help")
                    buttons |= QDialogButtonBox::Help;
                else
                    throw std::runtime_error("unsupported DialogButtonBox button '" + name.toStdString() + "'");
            }
            buttonBox->setStandardButtons(buttons);
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
            else if (auto *spinBox = qobject_cast<QDoubleSpinBox *>(widget))
                QObject::connect(spinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, widget](double) { notify(widget, {"value"}); });
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
            else if (auto *view = qobject_cast<QAbstractItemView *>(widget))
            {
                if (view->selectionModel())
                    QObject::connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this,
                                     [this, widget](const QModelIndex &, const QModelIndex &) {
                                         notify(widget, {"currentRow", "currentColumn", "currentText"});
                                     });
            }
            else if (auto *dateTimeEdit = qobject_cast<QDateTimeEdit *>(widget))
            {
                QObject::connect(dateTimeEdit, &QDateTimeEdit::dateChanged, this,
                                 [this, widget](const QDate &) { notify(widget, {"date"}); });
                QObject::connect(dateTimeEdit, &QDateTimeEdit::timeChanged, this,
                                 [this, widget](const QTime &) { notify(widget, {"time"}); });
                QObject::connect(dateTimeEdit, &QDateTimeEdit::dateTimeChanged, this,
                                 [this, widget](const QDateTime &) { notify(widget, {"dateTime"}); });
            }
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