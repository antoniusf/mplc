#version 150

precision highp float;

in vec4 out_Color;

out vec4 gl_FragColor;

void main (void) {
    if (out_Color.a < 0.5) {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 0.3);
        //gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
    }
    else {
        gl_FragColor = vec4(out_Color.rgb, 0.5);
        //gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
    }
}
