#version 150

precision highp float;

in vec4 out_Color;

out vec4 gl_FragColor;

void main (void) {
    if (out_Color.a < 1.0) {
        gl_FragColor = vec4(0.5, 0.5, 0.5, 0.7);
        //gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
    }
    else {
        gl_FragColor = vec4(out_Color.rgb, 1.0);
        //gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
    }
}
