//
// Created by Michael Desmedt on 14/05/2025.
//
#pragma once

#include "Core/Core.hpp"
#include "Events/Event.hpp"

namespace VoidArchitect
{
    class VA_API Layer
    {
    public:
        Layer(const std::string& name = "Layer");
        virtual ~Layer();

        virtual void OnAttach() {}

        virtual void OnDetach() {}

        virtual void OnFixedUpdate(const float fixedTimestep) {}

        virtual void OnUpdate(float deltaTime) {}

        virtual void OnEvent(Event& e) {}

        inline const std::string& GetName() const { return m_DebugName; }

    protected:
        std::string m_DebugName;
    };
} // namespace VoidArchitect
