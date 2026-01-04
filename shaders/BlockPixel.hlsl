cbuffer ConstantBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
    matrix LightViewProj;
    float4 LightDir;
    float4 CameraPos;
};

Texture2D ShadowMap : register(t0);
SamplerState LinearSampler : register(s0);
SamplerComparisonState ShadowSampler : register(s1);

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : WORLDPOS;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 TexCoord : TEXCOORD;
    float4 ShadowPos : SHADOWPOS;
};

float CalcShadowFactor_PCF(float4 shadowPos)
{
    // Perspective divide
    float3 projCoords = shadowPos.xyz / shadowPos.w;

    // Transform to [0,1] range
    projCoords.x = projCoords.x * 0.5f + 0.5f;
    projCoords.y = projCoords.y * -0.5f + 0.5f;

    // Check if outside shadow map
    if (projCoords.x < 0.0f || projCoords.x > 1.0f ||
        projCoords.y < 0.0f || projCoords.y > 1.0f)
        return 1.0f;

    float currentDepth = projCoords.z;
    if (currentDepth > 1.0f)
        return 1.0f;

    // PCF (Percentage Closer Filtering) for soft shadows
    float shadow = 0.0f;
    float2 texelSize = 1.0f / float2(2048.0f, 2048.0f);

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float2 offset = float2(x, y) * texelSize;
            shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler,
                                                    projCoords.xy + offset,
                                                    currentDepth - 0.001f);
        }
    }
    shadow /= 9.0f;

    return shadow;
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float3 normal = normalize(input.Normal);
    float3 lightDir = normalize(-LightDir.xyz);

    // Ambient
    float3 ambient = 0.3f * input.Color.rgb;

    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0f);
    float3 diffuse = diff * input.Color.rgb;

    // Shadow
    float shadowFactor = CalcShadowFactor_PCF(input.ShadowPos);

    // Combine
    float3 finalColor = ambient + shadowFactor * diffuse;

    return float4(finalColor, input.Color.a);
}
