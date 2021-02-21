static const float PI = 3.14159265f;

cbuffer Global : register(b0)
{
    float4 resolution;              // Width, Height, DPI, AspectRatio
    float time;
    float timeDelta;
    int frame;
    int ticks;
    int4 date;                      // as Year, Month, Day of month, Day since Jan.1st
    int4 clock;                     // as Hour, Minutes, Seconds, isDaylight Savings.
    int random;                     // from CPU rand() * RAND_MAX

    int perfHigh;                   // from QueryPerformanceCounter high-resolution time stamp
    int perfLow;                    // int64 split in 2x int32 High Low.
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

// ============================================================================

float2 uv_glsl(float2 pos) 
{
  float2 uv;
  uv.x = pos.x / resolution.x;
  uv.y = 1 - ( pos.y / resolution.y );
  return uv;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float2 uv = input.position.xy / resolution.xy;  // UV as 0 to 1
    float2 uvgl = uv_glsl(input.position);          // GLSL UV
    float2 pos = input.position.xy                  // fragCoord
    float4 color;                                   // fragColor
    
    {
        // Credits ShaderToy.com - New Shader
        // Time varying pixel color
        float3 col = 0.5 + 0.5 * cos(time + uv.xyx + float3(0, 2, 4));
        color = float4(col, 1.0);
    }

    return color;
}