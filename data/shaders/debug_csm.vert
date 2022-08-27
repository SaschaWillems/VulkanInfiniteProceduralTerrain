#version 450

layout(push_constant) uniform PushConsts {
	uint cascadeIndex;
} pushConsts;

layout (location = 0) out vec2 outUV;
layout (location = 1) out uint outCascadeIndex;

void main() 
{
    float x = float(((uint(gl_VertexIndex) + 2u) / 3u)%2u); 
    float y = float(((uint(gl_VertexIndex) + 1u) / 3u)%2u); 
    outUV = vec2(x, y);
    gl_Position = vec4(-1.0f + x*2.0f, -1.0f+y*2.0f, 0.0f, 1.0f);
	gl_Position.xy *= 0.25f;
	gl_Position.x += 0.75f;
	gl_Position.y += 0.25f;
	outCascadeIndex = pushConsts.cascadeIndex;
}