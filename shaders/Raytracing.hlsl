struct Vertex {
    float3 position;
    float3 normal;
    float4 color;
    float2 texCoord;
};

RaytracingAccelerationStructure Scene : register(t0);
StructuredBuffer<Vertex> Vertices : register(t1);
ByteAddressBuffer Indices : register(t2);
RWTexture2D<float4> Output : register(u0);

cbuffer CameraCB : register(b0) {
    float3 camOrigin;
    float pad0;
    float3 camForward;
    float pad1;
    float3 camRight;
    float pad2;
    float3 camUp;
    float pad3;
    float tanHalfFov;
    float aspect;
    float2 pad4;
};

struct RayPayload {
    float3 color;
};

[shader("raygeneration")]
void RayGen() {
    uint2 index = DispatchRaysIndex().xy;
    uint2 dims = DispatchRaysDimensions().xy;
    float2 uv = (float2(index) + 0.5f) / float2(dims);
    float2 screen = uv * 2.0f - 1.0f;
    screen.y = -screen.y;

    float2 scaled = float2(screen.x * aspect * tanHalfFov, screen.y * tanHalfFov);
    float3 dir = normalize(camForward + scaled.x * camRight + scaled.y * camUp);

    RayDesc ray;
    ray.Origin = camOrigin;
    ray.Direction = dir;
    ray.TMin = 0.001f;
    ray.TMax = 1000.0f;

    RayPayload payload;
    payload.color = float3(0.5f, 0.7f, 1.0f);

    TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    Output[index] = float4(payload.color, 1.0f);
}

[shader("miss")]
void Miss(inout RayPayload payload) {
    payload.color = float3(0.5f, 0.7f, 1.0f);
}

struct Attributes {
    float2 barycentrics;
};

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in Attributes attr) {
    uint primIndex = PrimitiveIndex();
    uint i0 = Indices.Load(primIndex * 12 + 0);
    uint i1 = Indices.Load(primIndex * 12 + 4);
    uint i2 = Indices.Load(primIndex * 12 + 8);

    Vertex v0 = Vertices[i0];
    Vertex v1 = Vertices[i1];
    Vertex v2 = Vertices[i2];

    float3 bary;
    bary.y = attr.barycentrics.x;
    bary.z = attr.barycentrics.y;
    bary.x = 1.0f - bary.y - bary.z;

    float3 normal = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);
    float3 baseColor = v0.color.rgb * bary.x + v1.color.rgb * bary.y + v2.color.rgb * bary.z;
    float3 lightDir = normalize(float3(0.5f, 1.0f, 0.3f));
    float diff = max(dot(normal, lightDir), 0.0f);
    payload.color = baseColor * (0.25f + 0.75f * diff);
}
