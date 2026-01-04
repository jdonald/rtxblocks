cbuffer ConstantBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
    matrix LightViewProj;
    float4 LightDir;
    float4 CameraPos;
};

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 TexCoord : TEXCOORD;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : WORLDPOS;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 TexCoord : TEXCOORD;
    float4 ShadowPos : SHADOWPOS;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;

    float4 worldPos = mul(float4(input.Position, 1.0f), World);
    output.WorldPos = worldPos.xyz;

    float4 viewPos = mul(worldPos, View);
    output.Position = mul(viewPos, Projection);

    output.Normal = normalize(mul(input.Normal, (float3x3)World));
    output.Color = input.Color;
    output.TexCoord = input.TexCoord;

    output.ShadowPos = mul(worldPos, LightViewProj);

    return output;
}
