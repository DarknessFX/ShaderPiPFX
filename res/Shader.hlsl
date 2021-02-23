// === VS+PS HLSL Init ========================================================
cbuffer Global : register(b0)
{
    float4 resolution;              // Width, Height, DPI, AspectRatio
    float time;
    float timeDelta;
    int frame;
    int ticks;
    int4 date;                      // as Year, Month, Day of month, Day since Jan.1st
    int4 clock;                     // as Hour, Minutes, Seconds, isDaylight Savings.
    int4 mouse;                     // z=LeftClick, w=RightClick. 1 Click, 2 DoubleClick
    int random;                     // from CPU rand() * RAND_MAX
    
    int perfHigh;                   // from QueryPerformanceCounter high-resolution 
    int perfLow;                    // time stamp int64 split in 2x int32 High Low.
};

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 texuv0 : TEXCOORD0;
    float2 texuv1 : TEXCOORD1;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 texuv0 : TEXCOORD0;
    float2 texuv1 : TEXCOORD1;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    //float frontface : VFACE;
};

PSInput VSMain(VSInput input)
{
    PSInput result;

    result.position = float4(input.position, 1);
    result.color = input.color;
    result.texuv0 = input.texuv0;
    result.texuv1 = input.texuv1;
    result.normal = input.normal;
    result.tangent = input.tangent;
    result.bitangent = input.bitangent;
    //result.frontface = input.frontface;
    //result.depth = input.depth;
    return result;
}

// Helpers: PI, glsl mod, uvs.
static const float PI = 3.1415926535897932384626433832795028;
float mod(float p) { if (sign(p.x)!=-1. ) { return fmod(p.x,1.); } else { return -fmod(p.x,1.); } }
float2 mod2(float2 p) { return float2(mod(p.x), mod(p.y)); }
float2 uv_glsl(float2 pos) { return float2(pos.x / resolution.x, 1 - ( pos.y / resolution.y )); }
float2 uv_center(float2 pos) { return float2(pos.y / resolution.y - 0.5, (pos.y / resolution.y - 0.5) * resolution.w); }

// === Pixel Shader code goes in PSMain =======================================

float4 PSMain(PSInput input) : SV_TARGET
{
    //UV Types Helper
    float2 uvhlsl = input.position.xy / resolution.xy;      // TopLeft     0,0     BottomRight    1,1
    float2 uvglsl = uv_glsl(input.position.xy);             // BottomLeft  0,0     TopRigth       1,1
    float2 uvcenter = uv_center(input.position.xy);         // TopLeft  -0.5,-0.5  BottomRight  0.5,0.5

    float2 uv = uvcenter;
    float2 pos = input.position.xy;                         // fragCoord
    float4 color;                                           // fragColor

    {
        // Credits ShaderToy.com - New Shader
        // Time varying pixel color
        float3 col = 0.5 + 0.5 * cos(time + uv.xyx + float3(0, 2, 4));
        color = float4(col, 1.0);
    }

    return color;
}