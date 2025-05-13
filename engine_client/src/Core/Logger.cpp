//
// Created by Michael Desmedt on 13/05/2025.
//
#include "Logger.hpp"
#include "vapch.hpp"

#include "spdlog/sinks/stdout_color_sinks.h"

namespace VoidArchitect
{
    std::shared_ptr<spdlog::logger> Logger::s_EngineLogger;
    std::shared_ptr<spdlog::logger> Logger::s_ApplicationLogger;

    void Logger::Initialize()
    {
        spdlog::set_pattern("%^[%d-%m-%Y %H:%M:%S --- %=8l --- %n]%$ %v");

        s_EngineLogger = spdlog::stdout_color_mt("ENG");
        s_EngineLogger->set_level(spdlog::level::trace);

        s_ApplicationLogger = spdlog::stdout_color_mt("APP");
        s_ApplicationLogger->set_level(spdlog::level::trace);
    }
} // namespace VoidArchitect
