// Shared source for SDL GPU's SPIR-V, DXIL and MSL artifacts. Keep the
// bindings stable: SDL's push-uniform slot 0 is used by each stage. SDL GPU
// maps vertex uniforms to HLSL space1 and fragment uniforms to space3.

cbuffer VertexUniforms : register(b0, space1) {
    float4x4 transform;
    float4x4 cameraView;
    float4x4 projection;
};

struct VertexInput {
    float4 position : TEXCOORD0;
    float4 normal : TEXCOORD1;
    float2 texcoord : TEXCOORD2;
};

struct VertexOutput {
    float4 position : SV_Position;
};

VertexOutput VSMain(VertexInput input) {
    VertexOutput output;
    output.position = mul(input.position, transform);
    output.position = mul(output.position, cameraView);
    output.position = mul(output.position, projection);
    return output;
}

cbuffer FragmentUniforms : register(b0, space3) {
    float4 baseColor;
};

float4 PSMain(VertexOutput input) : SV_Target0 {
    return baseColor;
}
