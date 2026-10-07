// Binding contract v2: five index bits, then filtering metadata.
#define SR_SAMPLER_INDEX_MASK 31u
#define SR_SAMPLER_MANUAL_FILTER 32u
#define SR_SAMPLER_MAG_LINEAR 64u
#define SR_SAMPLER_MIN_LINEAR 128u
#define SR_SAMPLER_MIP_SHIFT 8u

float4 srFilter2DLevel(Texture2D<float4> texture, SamplerState taps,
                      float2 uv, uint level, bool filterLinear) {
    if (!filterLinear) return texture.SampleLevel(taps, uv, float(level));
    uint width, height, levels;
    texture.GetDimensions(level, width, height, levels);
    float2 size = float2(width, height);
    float2 grid = uv * size - 0.5;
    float2 base = floor(grid);
    float2 weight = grid - base;
    float2 p = (base + 0.5) / size;
    float2 step = 1.0 / size;
    float4 a = texture.SampleLevel(taps, p, float(level));
    float4 b = texture.SampleLevel(taps, p + float2(step.x, 0), float(level));
    float4 c = texture.SampleLevel(taps, p + float2(0, step.y), float(level));
    float4 d = texture.SampleLevel(taps, p + step, float(level));
    return lerp(lerp(a, b, weight.x), lerp(c, d, weight.x), weight.y);
}

float4 srFilter2D(Texture2D<float4> texture, SamplerState taps,
                 float2 uv, float lod, uint samplerWord) {
    uint width, height, levels;
    texture.GetDimensions(0, width, height, levels);
    bool filterLinear = (samplerWord & (lod > 0 ? SR_SAMPLER_MIN_LINEAR : SR_SAMPLER_MAG_LINEAR)) != 0;
    uint mip = (samplerWord >> SR_SAMPLER_MIP_SHIFT) & 3u;
    if (mip == 2u) return srFilter2DLevel(texture, taps, uv, 0, filterLinear);
    float selected = clamp(lod, 0.0, float(levels - 1));
    if (mip == 0u)
        return srFilter2DLevel(texture, taps, uv, uint(floor(selected + 0.5)), filterLinear);
    uint lo = uint(floor(selected));
    uint hi = min(lo + 1u, levels - 1u);
    return lerp(srFilter2DLevel(texture, taps, uv, lo, filterLinear),
                srFilter2DLevel(texture, taps, uv, hi, filterLinear), selected - float(lo));
}

float4 srFilter2DAuto(Texture2D<float4> texture, SamplerState taps,
                     float2 uv, uint samplerWord) {
    float lod = texture.CalculateLevelOfDetailUnclamped(taps, uv);
    return srFilter2D(texture, taps, uv, lod, samplerWord);
}
