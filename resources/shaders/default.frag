#version 330

uniform sampler2D texture;

in vec2 texCoord;

layout(location = 0) out vec4 outputColor;

void main()
{
    outputColor = texture2D(texture, texCoord);
}