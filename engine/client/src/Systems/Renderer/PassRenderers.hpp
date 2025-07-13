//
// Created by Michael Desmedt on 04/06/2025.
//
#pragma once
#include <any>

#include <VoidArchitect/Engine/RHI/Resources/RendererTypes.hpp>

#include "Systems/RenderPassSystem.hpp"

namespace VoidArchitect::Resources
{
    struct RenderPassSignature;
}

namespace VoidArchitect
{
    namespace Platform
    {
        class IRenderingHardware;
    }

    namespace Resources
    {
        class IRenderPass;
        class IRenderTarget;
        class IRenderState;
        using RenderPassPtr = std::shared_ptr<IRenderPass>;
        using RenderTargetPtr = std::shared_ptr<IRenderTarget>;
        using RenderStatePtr = std::shared_ptr<IRenderState>;
    }

    namespace Renderer
    {
        struct FrameData;
        enum class RenderPassType;
        class RenderGraphBuilder;

        struct RenderContext
        {
            Platform::IRenderingHardware& rhi;
            const FrameData& frameData;
            const Resources::RenderPassHandle currentPassHandle;
            const Resources::RenderPassSignature& currentPassSignature;
        };

        class IPassRenderer
        {
        public:
            virtual ~IPassRenderer() = default;

            virtual void Setup(RenderGraphBuilder& builder) = 0;

            virtual void Execute(const RenderContext& context) = 0;

            virtual Renderer::RenderPassConfig GetRenderPassConfig() const = 0;
            virtual const std::string& GetName() const = 0;
        };

        using PassRendererPtr = std::shared_ptr<IPassRenderer>;

        class ForwardOpaquePassRenderer final : public IPassRenderer
        {
        public:
            ForwardOpaquePassRenderer() = default;

            void Setup(RenderGraphBuilder& builder) override;

            void Execute(const RenderContext& context) override;

            Renderer::RenderPassConfig GetRenderPassConfig() const override;
            const std::string& GetName() const override { return m_Name; }

        private:
            static const std::string m_Name;
        };

        class ForwardTransparentPassRenderer final : public IPassRenderer
        {
        public:
            ForwardTransparentPassRenderer() = default;

            void Execute(const RenderContext& context) override;
            Renderer::RenderPassConfig GetRenderPassConfig() const override;
            const std::string& GetName() const override { return m_Name; }

        private:
            static const std::string m_Name;
        };

        class ShadowPassRenderer final : public IPassRenderer
        {
        public:
            ShadowPassRenderer() = default;

            void Execute(const RenderContext& context) override;
            const std::string& GetName() const override { return m_Name; }
            void Setup(RenderGraphBuilder& builder) override;
            Renderer::RenderPassConfig GetRenderPassConfig() const override;

        private:
            static const std::string m_Name;
        };

        class DepthPrepassPassRenderer final : public IPassRenderer
        {
        public:
            DepthPrepassPassRenderer() = default;

            void Execute(const RenderContext& context) override;
            const std::string& GetName() const override { return m_Name; }
            void Setup(RenderGraphBuilder& builder) override;
            Renderer::RenderPassConfig GetRenderPassConfig() const override;

        private:
            static const std::string m_Name;
        };

        class PostProcessPassRenderer final : public IPassRenderer
        {
        public:
            PostProcessPassRenderer() = default;

            void Execute(const RenderContext& context) override;
            const std::string& GetName() const override { return m_Name; }
            void Setup(RenderGraphBuilder& builder) override;
            Renderer::RenderPassConfig GetRenderPassConfig() const override;

        private:
            static const std::string m_Name;
        };

        class UIPassRenderer final : public IPassRenderer
        {
        public:
            UIPassRenderer() = default;

            void Setup(RenderGraphBuilder& builder) override;

            void Execute(const RenderContext& context) override;
            Renderer::RenderPassConfig GetRenderPassConfig() const override;
            const std::string& GetName() const override { return m_Name; }

        private:
            static const std::string m_Name;
        };
    } // Renderer
} // VoidArchitect
