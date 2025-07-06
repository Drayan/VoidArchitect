//
// Created by Michael Desmedt on 13/05/2025.
//
#include "Logger.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace VoidArchitect
{
    std::shared_ptr<spdlog::logger> Logger::s_EngineLogger;
    std::shared_ptr<spdlog::logger> Logger::s_ApplicationLogger;

    void Logger::Initialize()
    {
        auto consoleSink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
        consoleSink->set_pattern("%^[%d-%m-%Y %H:%M:%S.%f --- %=8l --- %n]%$ %v");
#ifdef DEBUG
        consoleSink->set_level(spdlog::level::trace);
#else
        consoleSink->set_level(spdlog::level::info);
#endif

        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            "VoidArchitect.log",
            true);
        fileSink->set_pattern("%^[%d-%m-%Y %H:%M:%S.%f --- %=8l --- %n]%$ %v");
        fileSink->set_level(spdlog::level::trace);

        s_EngineLogger = std::make_shared<spdlog::logger>(
            "ENGINE",
            spdlog::sinks_init_list{consoleSink, fileSink});
        s_EngineLogger->set_level(spdlog::level::trace);

        s_ApplicationLogger = std::make_shared<spdlog::logger>(
            "APPLICATION",
            spdlog::sinks_init_list{consoleSink, fileSink});
        s_ApplicationLogger->set_level(spdlog::level::trace);
    }

    void Logger::Shutdown()
    {
        if (s_EngineLogger)
        {
            s_EngineLogger->flush();
            s_EngineLogger.reset();
        }

        if (s_ApplicationLogger)
        {
            s_ApplicationLogger->flush();
            s_ApplicationLogger.reset();
        }
    }
} // namespace VoidArchitect
