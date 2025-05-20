//
// Created by Michael Desmedt on 13/05/2025.
//
#pragma once

#include "spdlog/spdlog.h"

#include "Core.hpp"

namespace VoidArchitect
{
    class VA_API Logger
    {
    public:
        static void Initialize();

        inline static std::shared_ptr<spdlog::logger>& GetEngineLogger() { return s_EngineLogger; };

        inline static std::shared_ptr<spdlog::logger>& GetApplicationLogger()
        {
            return s_ApplicationLogger;
        };

    private:
        static std::shared_ptr<spdlog::logger> s_EngineLogger;
        static std::shared_ptr<spdlog::logger> s_ApplicationLogger;
    };
} // namespace VoidArchitect

#define VA_ENGINE_CRITICAL(...) ::VoidArchitect::Logger::GetEngineLogger()->critical(__VA_ARGS__)
#define VA_ENGINE_ERROR(...) ::VoidArchitect::Logger::GetEngineLogger()->error(__VA_ARGS__)
#define VA_ENGINE_WARN(...) ::VoidArchitect::Logger::GetEngineLogger()->warn(__VA_ARGS__)
#define VA_ENGINE_INFO(...) ::VoidArchitect::Logger::GetEngineLogger()->info(__VA_ARGS__)
#define VA_ENGINE_DEBUG(...) ::VoidArchitect::Logger::GetEngineLogger()->debug(__VA_ARGS__)
#define VA_ENGINE_TRACE(...) ::VoidArchitect::Logger::GetEngineLogger()->trace(__VA_ARGS__)

#define VA_APP_CRITICAL(...) ::VoidArchitect::Logger::GetApplicationLogger()->critical(__VA_ARGS__)
#define VA_APP_ERROR(...) ::VoidArchitect::Logger::GetApplicationLogger()->error(__VA_ARGS__)
#define VA_APP_WARN(...) ::VoidArchitect::Logger::GetApplicationLogger()->warn(__VA_ARGS__)
#define VA_APP_INFO(...) ::VoidArchitect::Logger::GetApplicationLogger()->info(__VA_ARGS__)
#define VA_APP_DEBUG(...) ::VoidArchitect::Logger::GetApplicationLogger()->debug(__VA_ARGS__)
#define VA_APP_TRACE(...) ::VoidArchitect::Logger::GetApplicationLogger()->trace(__VA_ARGS__)
