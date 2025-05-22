//
// Created by Michael Desmedt on 19/05/2025.
//
#pragma once

namespace VoidArchitect
{
    class IBuffer
    {
    public:
        virtual ~IBuffer() = default;

        virtual void Bind() = 0;
        virtual void Unbind() = 0;
    };
} // VoidArchitect
