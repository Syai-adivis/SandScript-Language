CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2

# 头文件列表，用于依赖检测
HEADERS := \
	RuntimeValue.h \
	analyzer.h \
	includes.h \
	Interpreter.h \
	prepross.h \
	SetColor.h

# 公共实现源文件
COMMON_SRCS := prepross.cpp

# 可执行目标
TARGETS := Sandi SandShell

.PHONY: all clean

all: $(TARGETS)

# Sandi 可执行文件
Sandi: Sandi.cpp $(COMMON_SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) Sandi.cpp $(COMMON_SRCS) -o $@

# SandShell 可执行文件
SandShell: SandShell.cpp $(COMMON_SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) SandShell.cpp $(COMMON_SRCS) -o $@

clean:
# Windows MinGW环境
ifeq ($(OS),Windows_NT)
	-del /f /q Sandi.exe SandShell.exe
else
# Linux/macOS
	rm -f Sandi SandShell
endif