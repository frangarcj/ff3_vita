#ifndef __movie_v__
#define __movie_v__

static const char *movie_v = "void main(\n"
                             "    float2 inPos,\n"
                             "    float2 inTex,\n"
                             "    out float4 vPosition : POSITION,\n"
                             "    out float2 vTexcoord : TEXCOORD0)\n"
                             "{\n"
                             "    vPosition = float4(inPos, 0.0f, 1.0f);\n"
                             "    vTexcoord = inTex;\n"
                             "}\n";

#endif
