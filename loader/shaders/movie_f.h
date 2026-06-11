#ifndef __movie_f__
#define __movie_f__

static const char *movie_f = "float4 main(\n"
                             "    float2 vTexcoord : TEXCOORD0,\n"
                             "    uniform sampler2D tex : TEXUNIT0) : COLOR\n"
                             "{\n"
                             "    return tex2D(tex, vTexcoord);\n"
                             "}\n";

#endif
