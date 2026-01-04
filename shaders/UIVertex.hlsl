struct VS_INPUT
{
    float2 Position : POSITION;
    float4 Color : COLOR;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;

    output.Position = float4(input.Position, 0.0f, 1.0f);
    output.Color = input.Color;

    return output;
}
