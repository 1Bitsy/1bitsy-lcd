# 1bitsy-lcd

This is a 1Bitsy driver for LCD displays and a rudimentary 2D graphics
library.  It can do full screen animation with 60 Hz refresh.


# Hardware

The specific LCD screen targeted is the
[Adafruit 2090](https://www.adafruit.com/product/2090).  It has a
240x320 resolution, and it uses the Ilitek ILI9341 driver chip.  Note
that this display scans in portrait order -- native orientation makes
it 240 pixels wide and 320 high.

Other screen sizes should be easy to support.  Other driver chips
will require some work.

The interface is 8 bit parallel.  See [doc/pinout](doc/pinout.png) for
details.

The 1Bitsy's MCU, an STM32F415RGT6, makes life interesting by not
having enouth RAM to store a full frame of video.  So the screen
is rendered in sections called `pixtiles`.  Each screen refresh
requires at least three pixtiles to be rendered.


# Graphics &mdash; Overview

(say something here)


## Pixtiles

The basic unit of graphics memory is the `pixtile`.  It represents
a rectangular block of pixels.  Each drawing function draws into
a pixtile.  A pixtile has:

  - an `origin`: integer x and y coordinates for the tile's
    top left corner.
  - a `size`: integer width and height.
  - a `stride`: number of pixels between rows.
  - `pixels`: memory to store *width* &times; *height* pixels
  
The pixel format in a pixtile is RGB 565.  Each pixel is 16 bits.

  - Red has the five most significant bits: 0xF800.
  - Green has the six middle bits: 0x07E0.
  - Blue has the five least significant bits: 0x001F.

Opacity values, also known as `alpha`, are given as `uint8_t`,
where 0 is transparent and 255 is 100% opaque.  Alpha values
are not stored in pixtiles.


## Coordinates

The pixel at coordinates (*x*, *y*) is centered at point
(*x + 1/2*, *y + 1/2*)  So integer coordinates are at pixel
boundaries.

Coordinates are always given in 32 bit floating point format, so they
always have subpixel accuracy.

User coordinates are translated by the pixtile's origin.  Aside from
that, the graphics library does not provide any transformations.
If transformations are needed, do them in application code.

## Drawing

Drawing functions follow a name scheme.

    gfx_[verb]_[primitive]_[modifiers]
    
If a drawing function has several modifiers, they are listed in
lexical order, separated by underscores.

For example, `gfx_fill_triangle_aa_unclipped` fills a triangle with
antialiased edges, without clipping.

### Verbs

The `verb` is either `fill` or `draw`.  Fill means paint the
primitive's interior with a solid color.  `draw` draws a roughly
one pixel wide outline around the primitive.  `draw` emphasizes
speed over quality.

### Primitives

There are five primitives at present: `pixel`, `span`, `line`,
`triangle`, and `trapezoid`.  I would like to add primitives for
circles, ellipses, arcs, aligned rectangles, quadratic and cubic
beziers, general polygons, and more.  But there are five primitives at
present.

A `pixel` is a single dot on the screen.

A `span` is a contiguous span of pixels on a scan line.

A `line` is a straight line.  It does not have an interior, so it
can't be filled, only outlined.

A `triangle` is exactly what it sounds like.

A `trapezoid` is a trapezoid whose parallel sides are scan line
aligned.  It is the most primitive solid shape; triangles are
decomposed into trapezoids to render.  The application can decompose
arbitrary polygons into trapezoids.

### Modifiers

There are several modifiers.  Not all modifiers are implemented
for all primitives.

`aa` means anti-aliased.

`blend` means the primitive will be blended against the background
with a scalar opacity (alpha) value.

`unclipped` means the primitive will not be clipped to the destination
pixtile.  The caller guarantees the primitive does not extend outside
the pixtile; if it does, the program will probably crash.


# Video Output

## The Rendering Loop

The basic rendering loop updates the application's animation state,
renders a a frame, and repeats.  Here is pseudocode.

    void main_loop(void)
    {
        while (1) {
            animate();
            draw_frame();
        }
    }

To draw a frame, the application allocates a pixtile, renders all
primitives that intersecct that pixtile, and repeats until the whole
screen is rendered.  Use `alloc_pixtile` to allocate a pixtile, and
`send_pixtile` to send it to the screen.

Here is more pseudocode.

    static void draw_frame(void)
    {
        size_t h;

        for (size_t y = 0; y < SCREEN_HEIGHT; y += h) {
            h = MIN((size_t)PIXTILE_MAX_HEIGHT, SCREEN_HEIGHT - y);
            pixtile *tile = alloc_pixtile(0, y, SCREEN_WIDTH, h);
            draw_tile(tile);
            send_pixtile(tile);
        }
    }

    static void draw_tile(pixtile *tile)
    {
        /* for each primitive, */
        /*     draw that primitive. */
    }


# Hardware &mdash; Details

The MCU has two on-chip RAM regions.  Close-Coupled Memory (CCM) is
64KB of low latency RAM.  It is not visible to the DMA controllers.
System RAM (SRAM) is 128KB of slightly slower RAM.  It is visible to DMA.

So we store program data (data, heap and stack) in the CCM, and
we use all of SRAM for DMA buffers.  Even so, the DMA buffers are
smaller than the screen, so an application has to render the screen
in pieces.

The good news is that the ILI9341 does store a complete video frame in
memory.  The bad news is that reading it is slow, about 4.6 MB/sec.
You could read some small regions to implement sprite compositing or
something, but you'll give up 60 Hz refresh if you read a significant
part of the screen.

I have not implemented reading ILI9341 memory.

The ILI9341 supports RGB 888 color, aka 24 bit color, but to reduce
memory use, this library renders everything to RGB 565, aka 16 bits.
