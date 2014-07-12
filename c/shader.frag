#version 150

precision highp float;

in vec4 out_Color;

out vec4 gl_FragColor;

void main (void) {
    gl_FragColor = out_Color;
}
