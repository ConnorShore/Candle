// Basic texture shader //

#type vertex
#version 330 core

layout(location=0) in vec3 vertexPos;
layout(location=1) in vec2 vertexUV;

uniform mat4 transform;
uniform mat4 viewProjectionMatrix;

out vec2 fragmentUV;

void main()
{
	gl_Position = viewProjectionMatrix * transform * vec4(vertexPos, 1.0);

	fragmentUV = vertexUV;
}


#type fragment
#version 330 core

layout(location = 0) out vec4 color;

in vec2 fragmentUV;

uniform sampler2D texture2D;

void main()
{
	color = texture(texture2D, fragmentUV);
}