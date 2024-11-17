#version 330 core

layout(location=0) in vec2 pos;

out vec2 fragCoord;

uniform mat4 mvp;

void main() {
	gl_Position = mvp * vec4(pos.xy, -1.0, 1.0);
	fragCoord = pos;
}
