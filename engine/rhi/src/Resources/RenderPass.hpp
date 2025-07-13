//
// Created by Michael Desmedt on 02/06/2025.
//
#pragma once

namespace VoidArchitect
{
    namespace Renderer
    {
        enum class TextureFormat;
    }

    namespace Resources
    {
        struct RenderPassSignature
        {
            VAArray<Renderer::TextureFormat> colorAttachmentFormats;
            std::optional<Renderer::TextureFormat> depthAttachmentFormat;

            bool operator==(const RenderPassSignature& rhs) const;
            size_t GetHash() const;
        };

        using RenderPassHandle = uint32_t;
        static constexpr RenderPassHandle InvalidRenderPassHandle = std::numeric_limits<
            uint32_t>::max();

        class IRenderPass
        {
        public:
            virtual ~IRenderPass() = default;

            [[nodiscard]] const std::string& GetName() const { return m_Name; }
            [[nodiscard]] const RenderPassSignature& GetSignature() const { return m_Signature; }

        protected:
            IRenderPass(const std::string& name, const RenderPassSignature& signature);

            std::string m_Name;
            RenderPassSignature m_Signature;
        };
    } // namespace Resources
} // namespace VoidArchitect

namespace std
{
    template <>
    struct hash<VoidArchitect::Resources::RenderPassSignature>
    {
        size_t operator()(
            const VoidArchitect::Resources::RenderPassSignature& signature) const noexcept
        {
            return signature.GetHash();
        }
    };
}
