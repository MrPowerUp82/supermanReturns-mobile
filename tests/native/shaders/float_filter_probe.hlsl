#include "float_filter.hlsl"
[[vk::binding(0,0)]] Texture2D<float4> InputTexture;
[[vk::binding(1,0)]] SamplerState PointSampler;
struct Query { float2 uv; float lod; uint samplerWord; };
[[vk::binding(2,0)]] StructuredBuffer<Query> Queries;
[[vk::binding(3,0)]] RWStructuredBuffer<float4> Results;
[numthreads(1,1,1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    Query q = Queries[id.x];
    Results[id.x] = srFilter2D(InputTexture, PointSampler, q.uv, q.lod, q.samplerWord);
}
struct Params {float scale; uint samplerWord; float2 pad;};
[[vk::push_constant]] ConstantBuffer<Params> Parameters;
float4 VSMain(uint id : SV_VertexID) : SV_Position {
    return float4(id == 1 ? 3 : -1, id == 2 ? 3 : -1, 0, 1);
}
float4 PSMain(float4 position : SV_Position) : SV_Target {
    if (Parameters.pad.x != 0)
        return InputTexture.CalculateLevelOfDetailUnclamped(PointSampler, position.xy / 8.0 * Parameters.scale).xxxx;
    return srFilter2DAuto(InputTexture, PointSampler,
                          position.xy / 8.0 * Parameters.scale, Parameters.samplerWord);
}
