#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_shape.h>
#include <GL/gl.h>
#include <math.h>
#include "glext.h"

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 640
#define WINDOW_WIDTH_2 320
#define WINDOW_HEIGHT_2 320

float sq (float a) {
    return pow(a, 2);
}

void get_circle (GLfloat *points, double centerx, double centery, double radius, int nr_of_segs, double offset) {

    double step = 2*M_PI/nr_of_segs;

    int i;
    for (i=0; i<nr_of_segs; i++) {
        double angle = step*i+offset;
        double a = sin(angle)*radius;
        double b = cos(angle)*radius;
        points[i*2] = centerx+a;
        points[i*2+1] = centery+b;
        //printf("%f\n", points[i*2]);
    }
}

char *read_file_to_buffer (char *file) {
    
    FILE *fileptr = fopen(file, "rb");
    if (fileptr == NULL) {
        return NULL;
    }
    fseek(fileptr, 0, SEEK_END);
    long length = ftell(fileptr);
    
    char *buffer = (char *) malloc(length+1);
    fseek(fileptr, 0, SEEK_SET);
    fread(buffer, length, 1, fileptr);
    fclose(fileptr);
    buffer[length] = 0;
    return buffer;
}

int main (void) {

    int i;
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL initialization failed: %s", SDL_GetError());
        return 1;
    }

    //OpenGL stuff
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    //Setup
    SDL_Window *window = SDL_CreateWindow("mplc", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL|SDL_WINDOW_BORDERLESS);
    if (window == NULL) {
        SDL_Quit();
        printf("Window creation failed: %s", SDL_GetError());
        return 1;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (context == 0) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        printf("OpenGL context creation failed: %s", SDL_GetError());
        return 1;
    }

    SDL_GL_SetSwapInterval(1);

    //OpenGL setup
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    //vbo data
    GLuint outer_circle[2];
    glGenBuffers(1, outer_circle);
    glBindBuffer(GL_ARRAY_BUFFER, outer_circle[0]);
    GLfloat points[200];
    //points[200] = 0.0;
    //points[201] = 0.0;
    get_circle(points, 0, 0, 0.5, 100, 0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, outer_circle[1]);
    GLfloat colors[400];
    for (i=0; i<400; i+=4) {
        colors[i] = 0.7;
        colors[i+1] = 0.7;
        colors[i+2] = 0.7;
        colors[i+3] = 1.0;
    }
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
    GLubyte outer_circle_indices[300]; //GLubyte should be fine here, because indices are lower than 255
    for (i=0; i<=100; i++) {
        outer_circle_indices[i*3] = 0;
        outer_circle_indices[i*3+1] = i+1;
        outer_circle_indices[i*3+2] = i+2;
    }
    outer_circle_indices[299] = 0; //wrap around to the beginning

    //shaders
    GLchar *vertexsource = read_file_to_buffer("shader.vert");
    GLchar *fragmentsource = read_file_to_buffer("shader.frag");
    if ((vertexsource == NULL) || (fragmentsource == NULL)) {
        printf("Could not find shaders.\n");
        return 0;
    }

    GLuint vertexshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexshader, 1, (const GLchar **) &vertexsource, 0);
    glCompileShader(vertexshader);
    int isCompiled_VS;
    glGetShaderiv(vertexshader, GL_COMPILE_STATUS, &isCompiled_VS);
    if (isCompiled_VS == 0) {
        printf("OpenGL vertex shader compilation failed. Source:\n\n%s\n", vertexsource);
        return 0;
    }

    GLuint fragmentshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentshader, 1, (const GLchar **) &fragmentsource, 0);
    glCompileShader(fragmentshader);
    int isCompiled_FS;
    glGetShaderiv(fragmentshader, GL_COMPILE_STATUS, &isCompiled_FS);
    if (isCompiled_FS == 0) {
        printf("OpenGL fragment shader compilation failed.\n");
        return 0;
    }

    GLuint shaderprogram = glCreateProgram();
    glAttachShader(shaderprogram, vertexshader);
    glAttachShader(shaderprogram, fragmentshader);
    glBindAttribLocation(shaderprogram, 0, "in_Position");
    glBindAttribLocation(shaderprogram, 1, "in_Color");
    glLinkProgram(shaderprogram);
    int IsLinked;
    glGetProgramiv(shaderprogram, GL_LINK_STATUS, &IsLinked);
    if (IsLinked == 0) {
        printf("OpenGL shader linking failed.\n");
        return 0;
    }

    //Main Loop
    int quit = 0;
    SDL_Event e;
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    quit = 1;
                    break;
                case SDL_KEYDOWN:
                    if (e.key.keysym.sym == SDLK_ESCAPE) {
                        quit = 1;
                    }
                    break;
                case SDL_WINDOWEVENT:
                    if (e.window.event == SDL_WINDOWEVENT_LEAVE) {
                        quit = 1;
                    }
                    break;
                default:
                    break;
            }
        }
        glClearColor(0.0, 0.0, 0.0, 0.5);
        glClear(GL_COLOR_BUFFER_BIT);
        //glDrawElements(GL_TRIANGLES, 1, GL_UNSIGNED_BYTE, outer_circle_indices);
        glDrawArrays(GL_LINE_LOOP, 0, 100);
        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
