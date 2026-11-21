#pragma once

#ifdef _WIN32
#define SAND_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define SAND_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

typedef const char *(*SandPluginCall)(const char *name, const char *arguments);

SAND_PLUGIN_EXPORT const char *sand_plugin_call(const char *name, const char *arguments);
