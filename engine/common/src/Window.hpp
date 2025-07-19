//
// Created by Michael Desmedt on 13/05/2025.
//
#pragma once

#include "Core.hpp"

#include <VoidArchitect/Engine/RHI/Interface/ISurfaceFactory.hpp>

namespace VoidArchitect
{
    struct WindowProps
    {
        std::string Title;
        unsigned int Width;
        unsigned int Height;

        explicit WindowProps(
            const std::string& title = "VoidArchitect Engine",
            unsigned int width = 1280,
            unsigned int height = 720)
            : Title(title),
              Width(width),
              Height(height)
        {
        }
    };

    // Interface representing a desktop window
    class VA_API Window
    {
    public:
        virtual ~Window() = default;

        virtual std::unique_ptr<RHI::ISurfaceFactory> CreateSurfaceFactory() = 0;

        virtual void OnUpdate() = 0;

        virtual unsigned int GetWidth() const = 0;
        virtual unsigned int GetHeight() const = 0;

        // Window attributes
        virtual void SetVSync(bool enabled) = 0;
        virtual bool IsVSync() const = 0;

        static Window* Create(const WindowProps& props = WindowProps());
    };
} // namespace VoidArchitect
