#include <stdio.h>
#include <string.h>
#include "agg_rendering_buffer.h"

enum {
    canvas_width  = 100,
    canvas_height = 100,
};

static void render_button(void)
{
    unsigned char *buffer = new unsigned char[canvas_width * canvas_height * 3];
    memset(buffer, 0xFF, canvas_width * canvas_height * 3);
    
    agg::rendering_buffer rbuf(buffer,
                               canvas_width,
                               canvas_height,
                               canvas_width * 3);
}

static void save_button(void)
{
}

static void make_button(void)
{
    render_button();
    save_button();
}

int main()
{
    make_button();
}
