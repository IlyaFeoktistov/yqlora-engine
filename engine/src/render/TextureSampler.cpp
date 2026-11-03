#include "yqlora/render/TextureSampler.hpp"

namespace yqlora {

namespace {

rhi::Wrap wrapOf(TextureWrap wrap)
{
    switch (wrap) {
    case TextureWrap::Repeat: return rhi::Wrap::Repeat;
    case TextureWrap::Clamp: return rhi::Wrap::Clamp;
    case TextureWrap::Mirror: return rhi::Wrap::Mirror;
    }

    return rhi::Wrap::Repeat;
}

rhi::Filter filterOf(TextureFilter filter)
{
    switch (filter) {
    case TextureFilter::Linear: return rhi::Filter::Linear;
    case TextureFilter::Nearest: return rhi::Filter::Nearest;
    }

    return rhi::Filter::Linear;
}

} // namespace

rhi::SamplerDesc samplerDesc(const TextureSampling& sampling)
{
    return {.filter = filterOf(sampling.filter), .wrap = wrapOf(sampling.wrap), .mipmaps = sampling.mipmaps};
}

} // namespace yqlora
