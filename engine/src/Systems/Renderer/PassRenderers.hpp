//
// Created by Michael Desmedt on 04/06/2025.
//
#pragma once
#include <any>

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

        struct RenderContext
        {
            Platform::IRenderingHardware& Rhi;
            const FrameData& FrameData;
            const Resources::RenderPassPtr& RenderPass;
            const Resources::RenderTargetPtr& RenderTarget;
            const Resources::RenderStatePtr& RenderState;

            VAHashMap<std::string, std::any> passData;
        };

        class IPassRenderer
        {
        public:
            virtual ~IPassRenderer() = default;

            virtual void Execute(
                const RenderContext& context) = 0;
            virtual std::string GetCompatibleRenderState() const = 0;

            virtual bool IsCompatibleWith(RenderPassType passType) const = 0;

            virtual const std::string& GetName() const = 0;
        };

        using PassRendererPtr = std::shared_ptr<IPassRenderer>;

        class ForwardOpaquePassRenderer final : public IPassRenderer
        {
        public:
            ForwardOpaquePassRenderer() = default;

            void Execute(
                const RenderContext& context) override;
            [[nodiscard]] std::string GetCompatibleRenderState() const override;

            bool IsCompatibleWith(RenderPassType passType) const override;
            const std::string& GetName() const override { return m_Name; }

        private:
            static const std::string m_Name;
        };

        class ForwardTransparentPassRenderer final : public IPassRenderer
        {
        public:
            ForwardTransparentPassRenderer() = default;

            void Execute(
                const RenderContext& context) override;
            std::string GetCompatibleRenderState() const override;
            bool IsCompatibleWith(RenderPassType passType) const override;
            const std::string& GetName() const override { return m_Name; }

        private:
            static const std::string m_Name;
        };

        class ShadowPassRenderer final : public IPassRenderer
        {
        public:
            ShadowPassRenderer() = default;

            void Execute(
                const RenderContext& context) override;
            std::string GetCompatibleRenderState() const override;
            bool IsCompatibleWith(RenderPassType passType) const override;
            const std::string& GetName() const override { return m_Name; }

        private:
            static const std::string m_Name;
        };

        class DepthPrepassPassRenderer final : public IPassRenderer
        {
        public:
            DepthPrepassPassRenderer() = default;

            void Execute(
                const RenderContext& context) override;
            std::string GetCompatibleRenderState() const override;
            bool IsCompatibleWith(RenderPassType passType) const override;
            const std::string& GetName() const override { return m_Name; }

        private:
            static const std::string m_Name;
        };

        class PostProcessPassRenderer final : public IPassRenderer
        {
        public:
            PostProcessPassRenderer() = default;

            void Execute(
                const RenderContext& context) override;
            std::string GetCompatibleRenderState() const override;
            bool IsCompatibleWith(RenderPassType passType) const override;
            const std::string& GetName() const override { return m_Name; }

        private:
            static const std::string m_Name;
        };

        class UIPassRenderer final : public IPassRenderer
        {
        public:
            UIPassRenderer() = default;

            void Execute(
                const RenderContext& context) override;
            std::string GetCompatibleRenderState() const override;
            bool IsCompatibleWith(RenderPassType passType) const override;
            const std::string& GetName() const override { return m_Name; }

        private:
            static const std::string m_Name;
        };
    } // Renderer
} // VoidArchitect
