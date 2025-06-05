//
// Created by Michael Desmedt on 14/05/2025.
//
#pragma once
#include "Layer.hpp"

namespace VoidArchitect
{
    class LayerStack
    {
    public:
        LayerStack();
        ~LayerStack();

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* layer);
        void PopLayer(Layer* layer);
        void PopOverlay(Layer* layer);

        VAArray<Layer*>::iterator begin() { return m_Layers.begin(); }
        VAArray<Layer*>::iterator end() { return m_Layers.end(); }

    private:
        VAArray<Layer*> m_Layers;
        VAArray<Layer*>::iterator m_LayerInsert;
    };
} // namespace VoidArchitect
