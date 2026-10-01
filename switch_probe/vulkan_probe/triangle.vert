#version 450
layout(location = 0) out vec3 color;
void main() {
    vec2 positions[3] = vec2[](vec2(0, -.7), vec2(.7, .7), vec2(-.7, .7));
    vec3 colors[3] = vec3[](vec3(1,0,0), vec3(0,1,0), vec3(0,0,1));
    gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
    color = colors[gl_VertexIndex];
}
