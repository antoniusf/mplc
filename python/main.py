import pyglet
from pyglet.gl import *
from math import pi, sin, cos, tan, sqrt
import os

def get_circle(center, radius, nr_of_segs, offset=0):
    step = 2*pi/nr_of_segs
    points = []
    for i in range(nr_of_segs):
        angle = step*i+offset
        a = sin(angle)*radius
        b = cos(angle)*radius
        points.extend([center[0]+a, center[1]+b])
    return points

window = pyglet.window.Window(640, 640, style=pyglet.window.Window.WINDOW_STYLE_BORDERLESS, visible=False)

platform = pyglet.window.get_platform()
display = platform.get_default_display()
screen = display.get_default_screen()
window_x, window_y = screen.width/2-320, screen.height/2-320

#hacky screenshot transparence thingy. definitely not portable.
os.system("scrot -z /tmp/scr.png")
screenshot = pyglet.image.load("/tmp/scr.png")
bg = screenshot.get_region(window_x, window_y+24, window.width, window.height)


ring_config = []
for i in range(100):
    ring_config.extend([i, (i+1)%100, i+100, (i+1)%100+100, i+100, (i+1)%100])#first triangle: (point on outer circle, next point on outer circle, point on inner circle), second triangle: (point on inner circle, next point on inner circle, next point on outer circle)

fpsdisplay = pyglet.clock.ClockDisplay()

STATE_PLAYING, STATE_PAUSED, STATE_STOPPED = 0, 1, 2

def get_stuff(request, answer_header):
    fd = open("/tmp/mplc", "w")
    fd.write(request+"\n")
    fd.close()
    fd = open("/tmp/mplo", "r")
    text = fd.read().split("\n")
    fd.close()
    text.reverse()
    for elem in text:
        if elem[:len(answer_header)] == answer_header:
            return elem[len(answer_header)+1:]

def send_stuff(stuff):
    fd = open("/tmp/mplc", "w")
    fd.write(stuff+"\n")
    fd.close()

def get_progress():
    progress = get_stuff("get_percent_pos", "ANS_PERCENT_POSITION")
    return int(progress)

def get_paused():
    paused = get_stuff("get_property pause", "ANS_pause")
    if paused == "no":
        return STATE_PLAYING
    else:
        return STATE_PAUSED

progress = get_progress()
play_state = get_paused()

skip_state = 0
skip_block = False

exit_block = True#necessary to prevent accidental exit when setting window location

@window.event
def on_draw():
    window.clear()
    bg.blit(0,0)

    #GL stuff for alpha
    glEnable(GL_BLEND)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)

    #draw background circle
    pyglet.graphics.draw(60, GL_POLYGON,
            ('v2f', tuple(get_circle((320, 320), 200, 60))),
            ('c4f', (.7, .7, .7, .5)*60)
            )

    #draw progress bar
    outer_circle = get_circle((window.width/2, window.height/2), 200, 100)
    inner_circle = get_circle((window.width/2, window.height/2), 170, 100)
    pyglet.graphics.draw_indexed(200, GL_TRIANGLES, ring_config,
            ('v2f', tuple(outer_circle)+tuple(inner_circle)),
            ('c4f', (1.0, 1.0, 1.0, 0.5)*progress+(.0, .0, .0, .3)*(100-progress)+(1.0, 1.0, 1.0, 0.5)*progress+(.0, .0, .0, 0.3)*(100-progress))
            )
    
    left = bottom = -120/sqrt(2)
    top = right = -left
    left += window.width/2
    right += window.width/2
    bottom += window.height/2
    top += window.height/2
    middle = 80/sqrt(2)

    if play_state == STATE_PLAYING:
        #draw play
        pyglet.graphics.draw(3, GL_TRIANGLES,
                ('v2f', tuple(get_circle((window.width/2, window.height/2), 120, 3, pi/2)))
                )

    elif play_state == STATE_PAUSED:
        #draw pause
        pyglet.graphics.draw_indexed(8, GL_TRIANGLES, [0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7],
                ('v2f', (left, bottom, left+middle, bottom, left+middle, top, left, top, right-middle, bottom, right, bottom, right, top, right-middle, top))
                )

    elif play_state == STATE_STOPPED:
        #draw stop
        pyglet.graphics.draw_indexed(4, GL_TRIANGLES, [0, 1, 2, 0, 2, 3],
                ('v2f', (left, bottom, right, bottom, right, top, left, top))
                )

    #draw skip background bar
    middle = window.width/2
    top = window.height/2+200
    bottom = window.height/2-200
    edge = middle+skip_state
    pyglet.graphics.draw_indexed(4, GL_TRIANGLES, [0, 1, 2, 0, 2, 3],
            ('v2f', (middle, bottom, edge, bottom, edge, top, middle, top)),
            ('c4f', (.0, .0, .0, .3)*4)
            )

    fpsdisplay.draw()

@window.event
def on_mouse_leave(x, y):
    if exit_block == False:
        window.dispatch_event("on_close")

@window.event
def on_mouse_press(x, y, button, modifiers):
    global play_state
    if button == pyglet.window.mouse.LEFT:
        send_stuff("pause")
        send_stuff("seek -1 0")
        send_stuff("seek 1 0")

@window.event
def on_mouse_release(*args):
    global skip_state
    global skip_block
    skip_state = 0
    skip_block = False

@window.event
def on_mouse_drag(x, y, dx, dy, buttons, modifiers):
    global skip_state
    global skip_block
    if skip_block == False:
        skip_state += dx
        if skip_state >= 200:
            send_stuff("pt_step 1")
            skip_state = 0
            skip_block = True
        if skip_state <= -200:
            send_stuff("pt_step -1")
            skip_state = 0
            skip_block = True

@window.event
def on_mouse_scroll(x, y, scroll_x, scroll_y):
    if scroll_y > 0:
        send_stuff("seek 30 0")
    elif scroll_y < 0:
        send_stuff("seek -30 0")


def update(dt):
    global progress
    global play_state
    global exit_block
    progress = get_progress()
    play_state = get_paused()

def unset_exit_block(dt):
    global exit_block
    exit_block = False

pyglet.clock.schedule_interval(update, 1/10.)
pyglet.clock.schedule_once(unset_exit_block, 0.5)

window.set_visible(True)
window.set_location(window_x, window_y)

pyglet.app.run()
