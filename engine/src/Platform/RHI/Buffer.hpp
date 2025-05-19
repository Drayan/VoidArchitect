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

    class VertexBuffer : public IBuffer
    {
    public:
        static std::unique_ptr<VertexBuffer> Create(std::vector<float> data);
    };

    class IndexBuffer : public IBuffer
    {
    public:
        static std::unique_ptr<IndexBuffer> Create(std::vector<uint32_t> data);
    };
} // VoidArchitect
