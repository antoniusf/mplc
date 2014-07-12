#version 150

in vec2 in_Position;
in vec4 in_Color;

out vec4 out_Color;

void main (void) {
    
    gl_Position = vec4(in_Position, 0.0, 1.0);
    out_Color = in_Color;
}
