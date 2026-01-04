struct PS_INPUT
{
    float4 Position : SV_POSITION;
};

void main(PS_INPUT input)
{
    // Shadow pass only writes to depth buffer
}
