#ifndef INTERPRETER_H
#define INTERPRETER_H
#include "analyzer.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#ifdef SAND_ENABLE_QT_WIDGETS
#include "qt_widget_runtime.h"
#endif

static bool safe_to_size_t(const BigDecimal &num, size_t &out)
{
    if (num.negative)
        return false;
    std::string s = num.to_string();
    try
    {
        unsigned long long v = std::stoull(s);
        out = static_cast<size_t>(v);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

template <typename MapT>
auto map_find_const(MapT &m, const typename MapT::key_type &key)
{
    auto it = m.lower_bound(key);
    if (it != m.end() && !m.key_comp()(key, it->first))
    {
        return it;
    }
    return m.end();
}

static bool worker_cancelled(Scope *scope)
{
    if (!scope)
        return false;
    if (RuntimeVal *taskValue = scope->get("__worker_task"))
    {
        if (auto *task = taskValue->as_task())
            return task->value->cancel_requested.load(std::memory_order_acquire);
    }
    return false;
}

struct EvalFrame
{
    bool has_return = false;
    RuntimeVal ret_val;

    bool break_flag = false;
    bool continue_flag = false;

    EvalFrame make_child() const
    {
        EvalFrame cf{};
        return cf;
    }

    static EvalFrame make_expr_frame()
    {
        EvalFrame ef{};
        return ef;
    }

    void reset_control()
    {
        has_return = false;
        ret_val = RuntimeVal();
        break_flag = false;
        continue_flag = false;
    }
};

struct Interpreter
{
    Scope global;
    Console::Color colorMode;
    std::mutex workerTasksMutex;
    std::vector<std::shared_ptr<WorkerTaskState>> workerTasks;
    std::vector<std::unique_ptr<ASTNode>> retainedAstRoots;
    std::unordered_map<int, std::shared_ptr<std::fstream>> fileStreams;
    int nextFileStreamId = 1;
#ifdef _WIN32
    std::unordered_map<int, HMODULE> pluginLibraries;
#else
    std::unordered_map<int, void *> pluginLibraries;
#endif
    int nextPluginId = 1;
#ifdef SAND_ENABLE_QT_WIDGETS
    QtWidgetRuntime qtWidgetRuntime;
    std::vector<QWidget *> widgetStack;
    std::unordered_map<std::string, QPointer<QWidget>> widgetIdentifiers;
    std::unordered_map<std::string, ASTNode *> widgetClasses;
    QWidget *resolveWidgetIdentifier(const std::string &name) const
    {
        if (name == "self")
            return widgetStack.empty() ? nullptr : widgetStack.back();
        auto widget = widgetIdentifiers.find(name);
        return widget == widgetIdentifiers.end() ? nullptr : widget->second.data();
    }
#endif
    Interpreter(){
        colorMode = Console::Color::Reset;
    }
    ~Interpreter()
    {
        joinWorkers();
    }
    void joinWorkers()
    {
        size_t joined = 0;
        for (;;)
        {
            std::shared_ptr<WorkerTaskState> task;
            {
                std::lock_guard<std::mutex> lock(workerTasksMutex);
                if (joined >= workerTasks.size())
                    break;
                task = workerTasks[joined++];
            }
            std::lock_guard<std::mutex> joinLock(task->join_mutex);
            if (task->thread.joinable())
                task->thread.join();
        }
    }
#ifdef SAND_ENABLE_QT_WIDGETS
    int runWidgets() { return qtWidgetRuntime.runEventLoop(); }
#endif
    FuncT *lookup_instance_method_from(ClassMeta *start_meta, const std::string &name);
    FuncT *lookup_instance_method(ClassMeta *meta, const std::string &name);
    FuncT *lookup_static_method(ClassMeta *meta, const std::string &name);
    std::shared_ptr<ClassMeta> resolve_superclass(const std::string &super_name, Scope *scope, size_t line);
    std::vector<std::string> cmd_args;
    std::vector<std::unique_ptr<ASTNode>> importedModules;
    RuntimeVal *get_lvalue(RuntimeVal &root, ASTNode *idx_node, Scope *scope, EvalFrame &expr_frame, size_t line);

    void import_file(const std::string &path, const std::vector<std::string> &names,
                     bool selective, Scope *target);

    RuntimeVal eval(ASTNode *node, Scope *scope, EvalFrame &frame);
#ifdef SAND_ENABLE_QT_WIDGETS
    std::unique_ptr<ASTNode> cloneWidgetExpr(const ASTNode *node);
    void collectWidgetDependencies(const ASTNode *node, std::vector<std::pair<std::string, std::string>> &dependencies);
#endif
};
#endif