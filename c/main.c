#include <stdio.h>
#include <stdlib.h>
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
#define SHOW_VOLUME_DELAY_MS 500

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
    int fd = open("/tmp/mplc", O_WRONLY|O_NONBLOCK);
    write(fd, string, len);
    close(fd);
    //FILE *fifo = fopen("/tmp/mplc", "w");
    //fwrite(string, len, 1, fifo);
    //fclose(fifo);
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

int get_volume (void) {
    fifo_send("get_property volume\n", 20);
    char *header = "ANS_volume=";
    char *answer = read_file_to_buffer("/tmp/mplo");
    
    int i=0;
    char pos=0;
    int volume;
    while (answer[i] != 0) {
        if (answer[i] == header[pos]) {
            pos += 1;
        }
        else {
            pos = 0;
        }
        if (pos ==  11) {
            volume = answer[i+1]-48;//ASCII, you know?
            if (answer[i+2] != 46) { //we don't need decimals, 46 is for .
                volume *= 10;
                volume += answer[i+2]-48;
                if (answer[i+3] != 46) {//In case its 100
                    volume *= 10;
                }
            }
        }
        i++;
    }
    //printf("volume: %i\n", volume);
    return volume;
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

//vertex_list layout: { VAO, position VBO, color VBO, index buffer, number of vertices }
GLuint *create_vertex_list (GLuint vertex_count) {
    GLuint *vertex_list = malloc(5*sizeof(*vertex_list));
    glGenVertexArrays(1, &vertex_list[0]);
    glBindVertexArray(vertex_list[0]);
    glGenBuffers(3, &vertex_list[1]);
    vertex_list[4] = vertex_count;
    return vertex_list;
}

void load_vertex_positions (GLuint *vertex_list, size_t list_size, GLfloat *list) {
    glBindBuffer(GL_ARRAY_BUFFER, vertex_list[1]);
    glBufferData(GL_ARRAY_BUFFER, list_size, list, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
}

void load_vertex_colors (GLuint *vertex_list, size_t list_size, GLfloat *list) {
    glBindBuffer(GL_ARRAY_BUFFER, vertex_list[2]);
    glBufferData(GL_ARRAY_BUFFER, list_size, list, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
}

void load_vertex_indices (GLuint *vertex_list, size_t list_size, GLuint *list) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertex_list[3]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, list_size, list, GL_STATIC_DRAW);
}

void draw_vertex_list (GLuint *vertex_list) {
    glBindVertexArray(vertex_list[0]);
    glDrawElements(GL_TRIANGLES, vertex_list[4], GL_UNSIGNED_INT, NULL);
}

void delete_vertex_list (GLuint *vertex_list) {
    glDeleteBuffers(3, &vertex_list[1]);
    glDeleteVertexArrays(1, &vertex_list[0]);
    free(vertex_list);
}

GLuint load_shaders (char *vertex_path, char *fragment_path) {

    GLchar *vertexsource = read_file_to_buffer(vertex_path);
    GLchar *fragmentsource = read_file_to_buffer(fragment_path);
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
        GLchar *info_log = malloc(1000);
        glGetShaderInfoLog(fragmentshader, 1000, NULL, info_log);
        printf("OpenGL fragment shader compilation failed for file \"%s\". Log:\n%s\n", fragment_path, info_log);
        free(info_log);
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
    return shaderprogram;
}

void update_skip_arrows (int skip_value, GLuint lskip[4], GLuint rskip[4]) {

    GLfloat lskip_left = -0.375*1.3;
    GLfloat lskip_right = -0.0625*1.3;
    GLfloat lskip_middle = -0.21785*1.3;
    GLfloat lskip_bottom = -0.15625*1.3;
    GLfloat lskip_top = -lskip_bottom;

    GLfloat rskip_left = -lskip_right;
    GLfloat rskip_right = -lskip_left;
    GLfloat rskip_middle = -lskip_middle;
    GLfloat rskip_bottom = lskip_bottom;
    GLfloat rskip_top = lskip_top;

    GLfloat first_arrow_factor = 0.0;
    GLfloat second_arrow_factor = 0.0;
    if ((skip_value >= -50) && (skip_value < 0)) {
        first_arrow_factor = 0.15625*skip_value/50*1.3;
    }
    if (skip_value < -50) {
        second_arrow_factor = 0.15625*(skip_value+50)/50*1.3;
        first_arrow_factor = -0.15625*1.3;
    }

    GLfloat third_arrow_factor = 0.0;
    GLfloat fourth_arrow_factor = 0.0;
    if ((skip_value <= 50) && (skip_value > 0)) {
        third_arrow_factor = 0.15625*skip_value/50*1.3;
    }
    if (skip_value > 50) {
        fourth_arrow_factor = 0.15625*(skip_value-50)/50*1.3;
        third_arrow_factor = 0.15625*1.3;
    }

   GLfloat lskip_points[] = {lskip_right, lskip_top, lskip_right+first_arrow_factor, lskip_top+first_arrow_factor, lskip_middle, 0.0, lskip_right+first_arrow_factor, lskip_bottom-first_arrow_factor, lskip_right, lskip_bottom, lskip_middle, lskip_top, lskip_middle+second_arrow_factor, lskip_top+second_arrow_factor, lskip_left, 0.0, lskip_middle+second_arrow_factor, lskip_bottom-second_arrow_factor, lskip_middle, lskip_bottom};
    glBindBuffer(GL_ARRAY_BUFFER, lskip[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lskip_points), lskip_points, GL_STATIC_DRAW);

   GLfloat rskip_points[] = {rskip_left, rskip_top, rskip_left+third_arrow_factor, rskip_top-third_arrow_factor, rskip_middle, 0.0, rskip_left+third_arrow_factor, rskip_bottom+third_arrow_factor, rskip_left, rskip_bottom, rskip_middle, rskip_top, rskip_middle+fourth_arrow_factor, rskip_top-fourth_arrow_factor, rskip_right, 0.0, rskip_middle+fourth_arrow_factor, rskip_bottom+fourth_arrow_factor, rskip_middle, rskip_bottom};
    glBindBuffer(GL_ARRAY_BUFFER, rskip[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rskip_points), rskip_points, GL_STATIC_DRAW);
}

void cartesic_to_polar ( Sint32 x, Sint32 y, double result[2] ) {

    double x2 = (double) x-WINDOW_WIDTH_2;
    double y2 = (double) WINDOW_HEIGHT_2-y; //negated this, because SDLs y unit vector points down, not up

    double length = sqrt(pow(x2, 2)+pow(y2, 2));
    double angle;
    if (x2 == 0) {
        if (y2 >= 0) {
            angle = M_PI_2;
        }
        else  {
            angle = M_PI_2*3;
        }
    }

    else if (x2 > 0) {
        angle = atan(y2/x2);
        if (angle < 0) { //Sometimes, you get the correct angle, but it is not given between 0 and 2*pi, instead it is below zero. Adding 2*pi doesn't change the angle, but brings it into the right interval.
            angle = angle + M_PI*2;
        }
    }
    else if (x2 < 0) {
        angle = atan(y2/x2) + M_PI; //For x<0, arctan is always off by one period of tan, which is pi. This is due to the way arctan is defined.
    }

    result[0] = length;
    result[1] = angle;
}

int main (void) {

    int i;
    int j;

    fifo_send("get_property volume\n", 20); //XXX mplayer wants some time to answer; remove when /tmp/mplo answer communication channel is fixed
    //system("scrot -z /tmp/scr.png");
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
    GLuint *outer_circle = create_vertex_list(300);

    GLfloat points[202];
    points[0] = 0.0;
    points[1] = 0.0;
    get_circle(&points[2], 0, 0, 0.625, 100, 0);
    load_vertex_positions(outer_circle, sizeof(points), points);

    GLfloat colors[404];
    for (i=0; i<404; i+=4) {
        colors[i] = 0.7;
        colors[i+1] = 0.7;
        colors[i+2] = 0.7;
        colors[i+3] = 0.5;
    }
    load_vertex_colors(outer_circle, sizeof(colors), colors);

    GLuint outer_circle_indices[300];
    for (i=0; i<100; i++) {
        outer_circle_indices[i*3] = 0;
        outer_circle_indices[i*3+1] = i+1;
        outer_circle_indices[i*3+2] = i+2;
    }
    outer_circle_indices[299] = 1; //wrap around to the beginning
    load_vertex_indices(outer_circle, sizeof(outer_circle_indices), outer_circle_indices);

    //Ring
    GLuint *ring = create_vertex_list(600);

    GLfloat ring_points[400];
    for (i=0; i<200; i++) {
        ring_points[i] = points[i+2];
    }
    get_circle(&ring_points[200], 0, 0, 0.53125, 100, 0);
    load_vertex_positions(ring, sizeof(ring_points), ring_points);

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
    load_vertex_colors(ring, sizeof(ring_colors), ring_colors);

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
    load_vertex_indices(ring, sizeof(ring_indices), ring_indices);

    //Play triangle
    //New approach: just put all GLuints in one array. Layout: [VAO, Position VBO, Color VBO, Index BO]
    //Actually, I don't need the IBO this time. But I want to try that out.
    GLuint *play = create_vertex_list(3);

    GLfloat play_points[6];
    get_circle(play_points, 0, 0, 0.375, 3, M_PI_2);
    load_vertex_positions(play, sizeof(play_points), play_points);

    GLfloat play_colors[16];
    for (i=0; i<16; i++) {
        play_colors[i] = 1.0;
    }
    load_vertex_colors(play, sizeof(play_colors), play_colors);

    GLuint play_indices[3] = { 0, 1, 2 };
    load_vertex_indices(play, sizeof(play_indices), play_indices);

    //Pause bars
    GLuint *pause = create_vertex_list(16);

    GLfloat left, bottom, top, right, middle;
    left = -0.375/sqrt(2);
    bottom = left;
    right = -left;
    top = right;
    middle = 0.25/sqrt(2);
    GLfloat pause_points[16] = {left, bottom, left+middle, bottom, left+middle, top, left, top, right-middle, bottom, right, bottom, right, top, right-middle, top};
    load_vertex_positions(pause, sizeof(pause_points), pause_points);

    GLfloat pause_colors[32];
    for (i=0; i<32; i++) {
        pause_colors[i] = 1.0;
    }
    load_vertex_colors(pause, sizeof(pause_colors), pause_colors);

    GLuint pause_indices[12] = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
    load_vertex_indices(pause, sizeof(pause_indices), pause_indices);

    int skip_value = 0;
    //left skip arrow
    GLuint *lskip = create_vertex_list(18);

    GLfloat lskip_points[20] = { 0.0 };
    load_vertex_positions(lskip, sizeof(lskip_points), lskip_points);
    
    glBindBuffer(GL_ARRAY_BUFFER, lskip[2]);
    GLfloat lskip_colors[40];
    for (i=0; i<40; i+=4) {
        if ((i/4 == 2) || (i/4 == 7)) {
            lskip_colors[i] = 0.0;
            lskip_colors[i+1] = 0.0;
            lskip_colors[i+2] = 0.0;
            lskip_colors[i+3] = 0.5;
        }
        else {
            lskip_colors[i] = 1.0;
            lskip_colors[i+1] = 1.0;
            lskip_colors[i+2] = 1.0;
            lskip_colors[i+3] = 1.0;
        }
    }
    load_vertex_colors(lskip, sizeof(lskip_colors), lskip_colors);

    GLuint lskip_indices[] = {0, 1, 4, 1, 3, 4, 1, 2, 3, 5, 6, 9, 6, 8, 9, 6, 7, 8};
    load_vertex_indices(lskip, sizeof(lskip_indices), lskip_indices);

    //right skip arrow
    GLuint *rskip = create_vertex_list(18);

    GLfloat rskip_points[20] = { 0.0 };
    load_vertex_positions(rskip, sizeof(rskip_points), rskip_points);

    GLfloat rskip_colors[40];
    for (i=0; i<40; i+=4) {
        if ((i/4 == 2) || (i/4 == 7)) {
            rskip_colors[i] = 0.0;
            rskip_colors[i+1] = 0.0;
            rskip_colors[i+2] = 0.0;
            rskip_colors[i+3] = 0.5;
        }
        else {
            rskip_colors[i] = 1.0;
            rskip_colors[i+1] = 1.0;
            rskip_colors[i+2] = 1.0;
            rskip_colors[i+3] = 1.0;
        }
    }
    load_vertex_colors(rskip, sizeof(rskip_colors), rskip_colors);

    GLuint rskip_indices[] = {0, 1, 4, 1, 3, 4, 1, 2, 3, 5, 6, 9, 6, 8, 9, 6, 7, 8};
    load_vertex_indices(rskip, sizeof(rskip_indices), rskip_indices);

    //volume bars
    GLuint *bars = create_vertex_list(60);

    GLfloat bars_points[80] = { 0.0 };
    left = -0.07;
    right = 0.07;
    bottom = -0.36;

    for (i=0; i<80; i+=8) {
        bars_points[i] = left;
        bars_points[i+1] = bottom;
        bars_points[i+2] = right;
        bars_points[i+3] = bottom;
        bottom += 0.04;
        bars_points[i+4] = right;
        bars_points[i+5] = bottom;
        bars_points[i+6] = left;
        bars_points[i+7] = bottom;
        bottom += 0.04;
    }

    load_vertex_positions(bars, sizeof(bars_points), bars_points);

    GLfloat bars_colors[160] = { 1.0 };
    load_vertex_colors(bars, sizeof(bars_colors), bars_colors);

    GLuint bars_indices[60];

    for (i=0; i<10; i++) {
        bars_indices[i*6] = 0+i*4;
        bars_indices[i*6+1] = 1+i*4;
        bars_indices[i*6+2] = 2+i*4;

        bars_indices[i*6+3] = 2+i*4;
        bars_indices[i*6+4] = 3+i*4;
        bars_indices[i*6+5] = 0+i*4;
    }
    load_vertex_indices(bars, sizeof(bars_indices), bars_indices);

    //background square with texture from screenshot (to simulate a transparent window)
    //GLuint bg[5];
    //glGenVertexArrays(1, &bg[0]);
    //glBindVertexArray(bg[0]);
    //glGenBuffers(3, &bg[1]);
    //glBindBuffer(GL_ARRAY_BUFFER, gb[1]);
    //GLfloat bg_points = { -1.0, -1.0, 1.0, -1.0, 1.0, 1.0, -1.0, 1.0 };
    //glBufferData(GL_ARRAY_BUFFER, sizeof(bg_points), bg_points, GL_STATIC_DRAW);
    //glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    //glEnableVertexAttribArray(0);
    //glBindBuffer(GL_ARRAY_BUFFER, bg[2]);

    //shaders
    GLuint shaderprogram = load_shaders("shader.vert", "shader.frag");
    GLuint ringshader = load_shaders("shader.vert", "ringshader.frag");
    GLuint skipshader = load_shaders("shader.vert", "skipshader.frag");
    //Main Loop
    int quit = 0;
    int quit_block = 0;
    Uint32 next_update = 0;
    Uint32 show_volume_until = 0;
    int pause_state;
    SDL_Event e;
    int drag_right = 0;
    int drag_left = 0;
    char *filename;
    int volume = get_volume();
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
                        double polar[2];
                        cartesic_to_polar(e.button.x, e.button.y, polar);
                        double circle_radius = 0.53125*WINDOW_WIDTH_2; //Translate into pixel coordinates; uses WINDOW_WIDTH_2 because whole OpenGL window is not 1, but 2 wide. TODO put this in some other place
                        if (polar[0] > circle_radius) {
                            polar[1] = polar[1]-M_PI_2; //The progress bar starts on top, not on the x axis
                            if (polar[1] < 0) polar[1] += 2*M_PI; //TODO maybe use % here?
                            polar[1] = 2*M_PI-polar[1]; //The progress bar goes clockwise, so negate the angle
                            int new_pos = polar[1]/(2*M_PI)*100;
                            //printf("Seek to position: %i\%\n", new_pos);
                            char command[] = "seek 000 1\n";
                            command[7] = new_pos%10+48; //+48 for ASCII
                            new_pos /= 10;
                            command[6] = new_pos%10+48;
                            if (new_pos >= 10) command[5] = 49;
                            //printf("command: %s\n", command);
                            fifo_send(command, 11);
                        }
                        else {
                        //printf("pause\n");
                        fifo_send("pause\n", 6);
                        drag_left = 1;
                        show_volume_until = 0; //Make sure the volume display goes away when the user clicks.
                        }
                    }
                    else if (e.button.button == SDL_BUTTON_RIGHT || e.button.button == SDL_BUTTON_MIDDLE) {
                        drag_right = 1;
                    }
                    update_skip_arrows(skip_value, lskip, rskip);
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        drag_left = 0;
                    }
                    else if (e.button.button == SDL_BUTTON_RIGHT) {
                        skip_value = 0;
                        drag_right = 0;
                        update_skip_arrows(skip_value, lskip, rskip);
                    }
                    break;
                case SDL_MOUSEWHEEL:
                    {
                        if (e.wheel.y > 0) {
                            volume += 10;
                        }
                        else if (e.wheel.y < 0) {
                            volume -= 10;
                        }

                        //printf("change vol. to: %i\n", volume);

                        if (volume <= 9) {
                            if (volume < 0) { volume = 0; }
                            char command[] = "volume x 1\n";
                            command[7] = volume+48;
                            //printf("command: %s\n", command);
                            fifo_send(command, 11);
                        }
                        else if (volume <= 99) {
                            char command[] = "volume xx 1\n";
                            int tens = volume/10;
                            command[7] = tens+48;
                            command[8] = volume-tens*10+48;
                            //printf("tens: %i; ones: %i; command: %s\n", tens, volume-tens*10, command);
                            fifo_send(command, 12);
                        }
                        else if (volume >= 100) {
                            volume = 100;
                            char command[] = "volume 100 1\n";
                            //printf("command: %s\n", command);
                            fifo_send(command, 13);
                        }
                        show_volume_until = SDL_GetTicks() + SHOW_VOLUME_DELAY_MS;

                        for (i=0; i<10; i++) {
                            if (i*10 < volume) {
                                for (j=0; j<16; j+=4) {
                                    bars_colors[i*16+j] = 1.0;
                                    bars_colors[i*16+j+1] = 1.0;
                                    bars_colors[i*16+j+2] = 1.0;
                                    bars_colors[i*16+j+3] = 1.0;
                                }
                            }
                            else {
                                for (j=0; j<16; j+=4) {
                                    bars_colors[i*16+j] = 0.0;
                                    bars_colors[i*16+j+1] = 0.0;
                                    bars_colors[i*16+j+2] = 0.0;
                                    bars_colors[i*16+j+3] = 0.5;
                                }
                            }
                        }
                        
                        glBindBuffer(GL_ARRAY_BUFFER, bars[2]);
                        glBufferData(GL_ARRAY_BUFFER, sizeof(bars_colors), bars_colors, GL_STATIC_DRAW);
                    }

                    break;
                case SDL_MOUSEMOTION:
                    if (drag_right == 1) {
                        skip_value += e.motion.xrel;
                        if (skip_value <= -100) {
                            skip_value = 0;
                            drag_right = 0;
                            fifo_send("pt_step -1\n", 11);
                        }

                        if (skip_value >= 100) {
                            skip_value = 0;
                            drag_right = 0;
                            fifo_send("pt_step 1\n", 10);
                        }
                        update_skip_arrows(skip_value, lskip, rskip);
                    }
                    break;
                case SDL_WINDOWEVENT:
                    if (e.window.event == SDL_WINDOWEVENT_LEAVE || e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                        if (drag_left) {
                            quit_block = 2;
                        }
                        else if (quit_block == 0) {
                            quit = 1;
                        }
                        else {
                            quit_block -= 1;
                        }
                    }
                    break;
                case SDL_DROPFILE:
                    //{
                    //filename = e.drop.file;
                    //printf("File: %s, %lu\n", filename, strlen(filename));
                    //int command_length = 12+strlen(filename)+1;
                    //char loadfile_command[command_length];
                    //*loadfile_command = 0;
                    //strcat(loadfile_command, "loadfile ");
                    //strcat(loadfile_command, filename);
                    //strcat(loadfile_command, " 1\n");
                    //printf("%s\n", loadfile_command);
                    //fifo_send(loadfile_command, command_length-1);
                    //SDL_free(filename);
                    //fifo_send("pause\n", 6);
                    //}
                    break;
                default:
                    break;
            }
        }

        if ( next_update < SDL_GetTicks() ) {
            next_update = SDL_GetTicks()+100;
            progress = get_progress();
            if (progress > 99) progress = 99;
            progress += 1;
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
            glBindBuffer(GL_ARRAY_BUFFER, ring[2]);
            glBufferData(GL_ARRAY_BUFFER, sizeof(ring_colors), ring_colors, GL_STATIC_DRAW);

            pause_state = get_pause();
        }
        glUseProgram(shaderprogram);
        glClearColor(0.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        draw_vertex_list(outer_circle);
        glUseProgram(ringshader);
        draw_vertex_list(ring);
        if (drag_right == 0) {
            glUseProgram(shaderprogram);
            if (SDL_GetTicks() < show_volume_until) {
                draw_vertex_list(bars);
            }
            else if (pause_state == 0) {
                draw_vertex_list(play);
            }
            else {
                draw_vertex_list(pause);
            }
        }
        else {
            glUseProgram(skipshader);
            draw_vertex_list(lskip);
            draw_vertex_list(rskip);
        }
        //glDrawArrays(GL_TRIANGLE_FAN, 0, 101);
        SDL_GL_SwapWindow(window);
    }

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    delete_vertex_list(outer_circle);
    delete_vertex_list(pause);
    delete_vertex_list(play);
    delete_vertex_list(ring);
    delete_vertex_list(lskip);
    delete_vertex_list(rskip);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
