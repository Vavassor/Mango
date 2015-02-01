#version 330

layout(std140) uniform ObjectBlock
{
	mat4 model_view_projection;
};

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 textureCoordinate;

out vec2 texCoord;

void main(void)
{
	texCoord = textureCoordinate;
	gl_Position = model_view_projection * vec4(position, 0, 1);
}