// Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef UTILS_LOGGER_LOG_H_
#define UTILS_LOGGER_LOG_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#define ENTER() LOGI("\n");

#define ANSI_HIGH_LIGHT
#ifdef ANSI_HIGH_LIGHT
#define ANSI_COLOR_BLACK         "\033[30m"
#define ANSI_COLOR_RED           "\033[31m"
#define ANSI_COLOR_GREEN         "\033[32m"
#define ANSI_COLOR_YELLOW        "\033[33m"
#define ANSI_COLOR_BLUE          "\033[34m"
#define ANSI_COLOR_MAGENTA       "\033[35m"
#define ANSI_COLOR_CYAN          "\033[36m"
#define ANSI_COLOR_WHITE         "\033[37m"
#define ANSI_COLOR_LIGHTBLACK    "\033[90m"
#define ANSI_COLOR_LIGHTRED      "\033[91m"
#define ANSI_COLOR_LIGHTGREEN    "\033[92m"
#define ANSI_COLOR_LIGHTYELLOW   "\033[93m"
#define ANSI_COLOR_LIGHTBLUE     "\033[94m"
#define ANSI_COLOR_LIGHTMAGENTA  "\033[95m"
#define ANSI_COLOR_LIGHTCYAN     "\033[96m"
#define ANSI_COLOR_LIGHTWHITE    "\033[97m"
#define ANSI_COLOR_RESET         "\x1b[0m"
#else
#define ANSI_COLOR_BLACK         ""
#define ANSI_COLOR_RED           ""
#define ANSI_COLOR_GREEN         ""
#define ANSI_COLOR_YELLOW        ""
#define ANSI_COLOR_BLUE          ""
#define ANSI_COLOR_MAGENTA       ""
#define ANSI_COLOR_CYAN          ""
#define ANSI_COLOR_WHITE         ""
#define ANSI_COLOR_LIGHTBLACK    ""
#define ANSI_COLOR_LIGHTRED      ""
#define ANSI_COLOR_LIGHTGREEN    ""
#define ANSI_COLOR_LIGHTYELLOW   ""
#define ANSI_COLOR_LIGHTBLUE     ""
#define ANSI_COLOR_LIGHTMAGENTA  ""
#define ANSI_COLOR_LIGHTCYAN     ""
#define ANSI_COLOR_LIGHTWHITE    ""
#define ANSI_COLOR_RESET         ""
#endif

#define LOG_WARN_PREFIX "WARN: "
#define LOG_ERROR_PREFIX "ERROR: "
#define LOG_FATAL_PREFIX "FATAL: "

#define LOGD(FORMAT, ...) \
do { \
    if (!Logger::GetLevel()) \
        printf(ANSI_COLOR_GREEN FORMAT ANSI_COLOR_RESET, ##__VA_ARGS__); \
} while(0)

#define LOGI(...) \
do { \
    if (!Logger::GetLevel() || Logger::GetLevel() > Logger::LEVEL_DEBUG) \
        printf(__VA_ARGS__); \
} while(0)

#define LOGW(FORMAT, ...) \
do { \
    if (!Logger::GetLevel() || Logger::GetLevel() > Logger::LEVEL_INFO) \
        printf(ANSI_COLOR_LIGHTYELLOW LOG_WARN_PREFIX FORMAT ANSI_COLOR_RESET, ##__VA_ARGS__); \
} while(0)

#define LOGE(FORMAT, ...) \
do { \
    if (!Logger::GetLevel() || Logger::GetLevel() > Logger::LEVEL_WARN) \
        printf(ANSI_COLOR_LIGHTRED LOG_ERROR_PREFIX FORMAT ANSI_COLOR_RESET, ##__VA_ARGS__); \
} while(0)

#define LOGF(FORMAT, ...) \
do { \
    if (!Logger::GetLevel() || Logger::GetLevel() > Logger::LEVEL_ERROR) \
        printf(ANSI_COLOR_RED LOG_FATAL_PREFIX FORMAT ANSI_COLOR_RESET, ##__VA_ARGS__); \
} while(0)

class Logger {
public:
    constexpr static uint32_t LEVEL_DEBUG =   0;
    constexpr static uint32_t LEVEL_INFO =    1;
    constexpr static uint32_t LEVEL_WARN =    2;
    constexpr static uint32_t LEVEL_ERROR =   3;
    constexpr static uint32_t LEVEL_FATAL =   4;
    static bool IsDebug() { return !Logger::GetLevel(); }
    static uint32_t GetLevel() { return INSTANCE->getLevel(); }
    static void SetLevel(int lv) { INSTANCE->setLevel(lv); }
    Logger(int lv) { mLevel = lv; }
private:
    inline uint32_t getLevel() { return mLevel; }
    inline void setLevel(int lv) { mLevel = lv; }
    static Logger* INSTANCE;
    uint32_t mLevel;
};

#endif // UTILS_LOGGER_LOG_H_
