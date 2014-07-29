#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
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

void fifo_send (char *string, int len) {
    //int fd = open("/tmp/mplc", O_WRONLY|O_NONBLOCK);
    //write(fd, string, sizeof(string));
    //close(fd);
    FILE *fifo = fopen("/tmp/mplc", "w");
    fwrite(string, len, 1, fifo);
    fclose(fifo);
}

int get_progress (void) {
    fifo_send("get_percent_pos\n", 16);
    char *header = "ANS_PERCENT_POSITION=";
    char *answer = read_file_to_buffer("/tmp/mplo");
    //printf("%s\n", answer);
    //printf("yay\n");
    //FILE *mplo = fopen("/tmp/mplo", "w");
    //fwrite("", 0, 1, mplo);
    //fclose(mplo);
    
    int i=0;
    char pos=0;
    int progress;
    while (answer[i] != 0) {
        if ( answer[i] == header[pos] ) {
            pos += 1;
        }
        else {
            pos = 0;
        }
        if ( pos == 21 ) {
            //printf("%c%c%c%c\n", answer[i-1], answer[i], answer[i+1], answer[i+2]);
            progress = answer[i+1]-48;
            if ( answer[i+2] != 10 ) {
                progress *= 10;
                progress += answer[i+2]-48;
            }
            //printf("%i\n", progress);
        }
        i++;
    }
    return progress;
}

int get_pause (void) {
    fifo_send("get_property pause\n", 19);
    char *header = "ANS_pause=";
    char *answer = read_file_to_buffer("/tmp/mplo");
    
    int i=0;
    char pos=0;
    int pause=0;
    while (answer[i] != 0) {
        if (answer[i] == header[pos]) {
            pos += 1;
        }
        else {
            pos = 0;
        }
        if (pos == 10) {
            if (answer[i+1] == 121) {
                pause = 1;
            }
            else if (answer[i+1] == 110) {
                pause = 0;
            }
        }
        i++;
    }
    //printf("pause: %i\n", pause);
    return pause;
}

int main (void) {

    int i;
    int j;
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

    //Outer (big) circle
    GLuint outer_circle_array;
    glGenVertexArrays(1, &outer_circle_array);
    glBindVertexArray(outer_circle_array);
    GLuint outer_circle[2];
    glGenBuffers(2, outer_circle);
    glBindBuffer(GL_ARRAY_BUFFER, outer_circle[0]);
    GLfloat points[202];
    points[0] = 0.0;
    points[1] = 0.0;
    get_circle(&points[2], 0, 0, 0.625, 100, 0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, outer_circle[1]);
    GLfloat colors[404];
    for (i=0; i<404; i+=4) {
        colors[i] = 0.7;
        colors[i+1] = 0.7;
        colors[i+2] = 0.7;
        colors[i+3] = 0.5;
    }
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
    GLuint outer_circle_indices[300];
    for (i=0; i<100; i++) {
        outer_circle_indices[i*3] = 0;
        outer_circle_indices[i*3+1] = i+1;
        outer_circle_indices[i*3+2] = i+2;
    }
    outer_circle_indices[299] = 1; //wrap around to the beginning
    GLuint ibo;
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(outer_circle_indices), outer_circle_indices, GL_STATIC_DRAW);

    //Ring
    GLuint ring_array;
    glGenVertexArrays(1, &ring_array);
    glBindVertexArray(ring_array);
    GLuint ring[2];
    glGenBuffers(2, ring);
    glBindBuffer(GL_ARRAY_BUFFER, ring[0]);
    GLfloat ring_points[400];
    for (i=0; i<200; i++) {
        ring_points[i] = points[i+2];
    }
    get_circle(&ring_points[200], 0, 0, 0.53125, 100, 0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ring_points), ring_points, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, ring[1]);
    int progress = 1;
    GLfloat ring_colors[800];
    for (i=0; i<200; i++) {
        j = i*4;
        if (i%100 < progress) {
            ring_colors[j] = 1.0;
            ring_colors[j+1] = 1.0;
            ring_colors[j+2] = 1.0;
            ring_colors[j+3] = 0.5;
        }
        else {
            ring_colors[j] = 0.0;
            ring_colors[j+1] = 0.0;
            ring_colors[j+2] = 0.0;
            ring_colors[j+3] = 0.3;
        }
    }
    glBufferData(GL_ARRAY_BUFFER, sizeof(ring_colors), ring_colors, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
    GLuint ring_indices[600];
    for (i=0; i<100; i++) {
        j = i*6;
        ring_indices[j] = i;
        ring_indices[j+1] = (i+1)%100;
        ring_indices[j+2] = i+100;
        ring_indices[j+3] = (i+1)%100+100;
        ring_indices[j+4] = i+100;
        ring_indices[j+5] = (i+1)%100;
    }
    GLuint ring_ibo;
    glGenBuffers(1, &ring_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ring_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(ring_indices), ring_indices, GL_STATIC_DRAW);

    //Play triangle
    //New approach: just put all GLuints in one array. Layout: [VAO, Position VBO, Color VBO, Index BO]
    //Actually, I don't need the IBO this time. But I want to try that out.
    GLuint play[4];
    glGenVertexArrays(1, &play[0]);
    glBindVertexArray(play[0]);
    glGenBuffers(3, &play[1]);
    glBindBuffer(GL_ARRAY_BUFFER, play[1]);
    GLfloat play_points[6];
    get_circle(play_points, 0, 0, 0.375, 3, M_PI_2);
    glBufferData(GL_ARRAY_BUFFER, sizeof(play_points), play_points, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, play[2]);
    GLfloat play_colors[16];
    for (i=0; i<16; i++) {
        play_colors[i] = 1.0;
    }
    glBufferData(GL_ARRAY_BUFFER, sizeof(play_colors), play_colors, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, play[3]);
    GLuint play_indices[3] = { 0, 1, 2 };
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(play_indices), play_indices, GL_STATIC_DRAW);

    //Pause bars
    GLuint pause[4];
    glGenVertexArrays(1, &pause[0]);
    glBindVertexArray(pause[0]);
    glGenBuffers(3, &pause[1]);
    glBindBuffer(GL_ARRAY_BUFFER, pause[1]);
    GLfloat left, bottom, top, right, middle;
    left = -0.375/sqrt(2);
    bottom = left;
    right = -left;
    top = right;
    middle = 0.25/sqrt(2);
    GLfloat pause_points[16] = {left, bottom, left+middle, bottom, left+middle, top, left, top, right-middle, bottom, right, bottom, right, top, right-middle, top};
    glBufferData(GL_ARRAY_BUFFER, sizeof(pause_points), pause_points, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, pause[2]);
    GLfloat pause_colors[32];
    for (i=0; i<32; i++) {
        pause_colors[i] = 1.0;
    }
    glBufferData(GL_ARRAY_BUFFER, sizeof(pause_colors), pause_colors, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pause[3]);
    GLuint pause_indices[12] = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(pause_indices), pause_indices, GL_STATIC_DRAW);

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
    Uint32 next_update = 0;
    int pause_state;
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
                case SDL_MOUSEBUTTONDOWN:
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        printf("pause\n");
                        fifo_send("pause\n", 6);
                    }
                    break;
                case SDL_WINDOWEVENT:
                    if (e.window.event == SDL_WINDOWEVENT_LEAVE || e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                        quit = 1;
                    }
                    break;
                default:
                    break;
            }
        }

        if ( next_update < SDL_GetTicks() ) {
            next_update = SDL_GetTicks()+100;
            progress = get_progress();
            for (i=0; i<200; i++) {
                j = i*4;
                if (i%100 < progress) {
                    ring_colors[j] = 1.0;
                    ring_colors[j+1] = 1.0;
                    ring_colors[j+2] = 1.0;
                    ring_colors[j+3] = 0.5;
                }
                else {
                    ring_colors[j] = 0.0;
                    ring_colors[j+1] = 0.0;
                    ring_colors[j+2] = 0.0;
                    ring_colors[j+3] = 0.3;
                }
            }
            glBindBuffer(GL_ARRAY_BUFFER, ring[1]);
            glBufferData(GL_ARRAY_BUFFER, sizeof(ring_colors), ring_colors, GL_STATIC_DRAW);

            pause_state = get_pause();
        }
        glUseProgram(shaderprogram);
        glClearColor(0.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindVertexArray(outer_circle_array);
        glDrawElements(GL_TRIANGLES, 300, GL_UNSIGNED_INT, NULL);
        glBindVertexArray(ring_array);
        glDrawElements(GL_TRIANGLES, 600, GL_UNSIGNED_INT, NULL);
        if (pause_state == 0) {
            glBindVertexArray(play[0]);
            glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, NULL);
        }
        else {
            glBindVertexArray(pause[0]);
            glDrawElements(GL_TRIANGLES, 16, GL_UNSIGNED_INT, NULL);
        }
        //glDrawArrays(GL_TRIANGLE_FAN, 0, 101);
        SDL_GL_SwapWindow(window);
    }

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDeleteBuffers(2, outer_circle);
    glDeleteVertexArrays(1, &outer_circle_array);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
