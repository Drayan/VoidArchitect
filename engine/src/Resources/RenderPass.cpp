//
// Created by Michael Desmedt on 02/06/2025.
//
#include "RenderPass.hpp"

#include "Core/Utils.hpp"

namespace VoidArchitect::Resources
{
    IRenderPass::IRenderPass(const std::string& name, const RenderPassSignature& signature)
        : m_Name(name), m_Signature(signature)
    {
    }

    bool RenderPassSignature::operator==(const RenderPassSignature& other) const
    {
        return colorAttachmentFormats == other.colorAttachmentFormats &&
            depthAttachmentFormat == other.depthAttachmentFormat;
    }

    size_t RenderPassSignature::GetHash() const
    {
        size_t seed = 0;

        for (auto format : colorAttachmentFormats)
        {
            HashCombine(seed, format);
        }
        if (depthAttachmentFormat.has_value())
            HashCombine(seed, depthAttachmentFormat.value());

        return seed;
    }
} // namespace VoidArchitect::Resources
