struct VertexOut
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

VertexOut VS(uint vertexID : SV_VertexID)
{
    VertexOut vout;
    
    // draw a triangle that covers the entire screen
    const float2 tex = float2(uint2(vertexID, vertexID << 1) & 2);
    vout.Position = float4(lerp(float2(-1, 1), float2(1, -1), tex), 0, 1);
    vout.TexCoord = tex;
    
    return vout;
}
