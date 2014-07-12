C Version
=========

I'm porting this to C, for fun, practice, and execution speed. (You don't want slow execution in a programm like this.)

To build: You need to dynamically link in SDL2, OpenGL and (on Linux) math.h. Of course, you also need the OpenGL and SDL2 headers and the SDL2 library (get it from libsdl.org). My gcc call looks something like this: gcc -o mplc main.c -lSDL2 -lGL -lm. But at the time, you won't see much.
