//
// Created by Michael Desmedt on 14/05/2025.
//
#include "LayerStack.hpp"

namespace VoidArchitect
{

    LayerStack::LayerStack() { m_LayerInsert = m_Layers.begin(); }

    LayerStack::~LayerStack()
    {
        for (Layer* layer : m_Layers)
            delete layer;
    }

    void LayerStack::PushLayer(Layer* layer)
    {
        m_LayerInsert = m_Layers.emplace(m_LayerInsert, layer);
    }

    void LayerStack::PushOverlay(Layer* layer) { m_Layers.emplace_back(layer); }

    void LayerStack::PopLayer(Layer* layer)
    {
        auto it = std::ranges::find(m_Layers, layer);
        if (it != m_Layers.end())
        {
            m_Layers.erase(it);
            --m_LayerInsert;
        }
    }

    void LayerStack::PopOverlay(Layer* layer)
    {
        auto it = std::ranges::find(m_Layers, layer);
        if (it != m_Layers.end())
            m_Layers.erase(it);
    }

} // namespace VoidArchitect
