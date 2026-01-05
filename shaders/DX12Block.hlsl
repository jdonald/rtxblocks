cbuffer RasterCB : register(b0)
{
    float4x4 World;
    float4x4 View;
    float4x4 Projection;
    float4 LightDir;
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
    float3 Normal : NORMAL;
    float4 Color : COLOR;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    float4 worldPos = mul(float4(input.Position, 1.0f), World);
    float4 viewPos = mul(worldPos, View);
    output.Position = mul(viewPos, Projection);
    output.Normal = normalize(mul(input.Normal, (float3x3)World));
    output.Color = input.Color;
    return output;
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    float3 normal = normalize(input.Normal);
    float3 lightDir = normalize(-LightDir.xyz);
    float diff = max(dot(normal, lightDir), 0.0f);
    float3 ambient = input.Color.rgb * 0.3f;
    float3 diffuse = input.Color.rgb * diff;
    return float4(ambient + diffuse, input.Color.a);
}
