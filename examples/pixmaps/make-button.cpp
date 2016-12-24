#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agg_conv_contour.h"
#include "agg_conv_stroke.h"
#include "agg_font_freetype.h"
#include "agg_pixfmt_rgb.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_renderer_base.h"
#include "agg_renderer_scanline.h"
#include "agg_rounded_rect.h"

enum {
    canvas_width  = 100,
    canvas_height = 100,
};

typedef agg::rendering_buffer rbuf_type;
typedef agg::pixfmt_rgb24 pixfmt_type;
typedef pixfmt_type::color_type color_type;
typedef agg::renderer_base<pixfmt_type> base_renderer;
typedef agg::renderer_scanline_aa_solid<base_renderer>  solid_renderer;

typedef agg::rasterizer_scanline_aa<> scanline_rasterizer;
// typedef agg::rasterizer_outline<primitives_renderer> outline_rasterizer;

typedef agg::font_engine_freetype_int32 font_engine_type;
typedef agg::font_cache_manager<font_engine_type> font_manager_type;
typedef agg::conv_curve<font_manager_type::path_adaptor_type> font_curves_type;
typedef agg::conv_contour<font_curves_type> font_contour_type;

void render_button(base_renderer& rbase)
{
    scanline_rasterizer ras;
    // pixfmt pixf(rbuf_window());
    // base_renderer rb(pixf);
    solid_renderer solid(rbase);
    
    // white fill
    solid.color(agg::rgba8(0xFF, 0xFF, 0xFF));
    // draft_renderer draft(rbase);
    agg::scanline_u8 sl;
    agg::rounded_rect button_shape(0.5, 0.5, 50 - 0.5, 20 - 0.5, 5);
    ras.add_path(button_shape);
    agg::render_scanlines(ras, sl, solid);

    // black outline
    agg::conv_stroke<agg::rounded_rect> outline(button_shape);
    outline.width(0.2);
    ras.add_path(outline);
    solid.color(agg::rgba8(0, 0, 0));
    agg::render_scanlines(ras, sl, solid);

    font_engine_type feng;
    font_manager_type fman(feng);
    font_curves_type curves(fman.path_adaptor());
    font_contour_type contour(curves);
    if (!feng.load_font("fonts/Ubuntu-C.ttf", 0, agg::glyph_ren_outline)) {
        exit(1);
    }
    feng.hinting(false);
    feng.height(15);
    feng.width(15);
    feng.flip_y(true);
    contour.width(0);

    double x = 5;
    double y = 20 - 4.5;
    for (const char *p = "Cancel"; *p; p++) {
        const agg::glyph_cache *glyph = fman.glyph(*p);
        if (glyph) {
            fman.add_kerning(&x, &y);
            fman.init_embedded_adaptors(glyph, x, y);
            ras.reset();
            ras.add_path(contour);
            x += glyph->advance_x;
            y += glyph->advance_y;
        }
        solid.color(agg::rgba8(0x9e, 0x9e, 0x9e));
        agg::render_scanlines(ras, sl, solid);
    }
    printf("xy = (%g, %g)\n", x, y);
    printf("ascender = %g, descender = %g\n", feng.ascender(), feng.descender());
}

static void write_ppm_file(const agg::rendering_buffer& rbuf, FILE *out)
{
    unsigned width = rbuf.width();
    unsigned height = rbuf.height();
    fprintf(out, "P6 %u %u 255 ", width, height);
    fwrite(rbuf.buf(), 1, width * height * 3, out);
}

static void save_button(const agg::rendering_buffer& rbuf, const char *outfile)
{
    FILE *f = fopen(outfile, "wb");
    if (!f)
        perror(outfile), exit(1);
    write_ppm_file(rbuf, f);
    fclose(f);
}

static void make_button(void)
{
    // Raw memory buffer
    const size_t buffer_bytes = canvas_width * canvas_height * 3;
    agg::int8u buffer[buffer_bytes];

    // Erase to gray88
    memset(buffer, 0xe0, buffer_bytes);

    {
        // Rendering Buffer: rows and row stride.
        agg::rendering_buffer rbuf(buffer,
                                   canvas_width,
                                   canvas_height,
                                   canvas_width * 3);

        {
            // Pixfmt Renderer: pixel format.
            pixfmt_type pixf(rbuf);

            {
                // Renderer Base: bounds clipping.
                agg::renderer_base<typeof pixf> rbase(pixf);

                {
                    render_button(rbase);
                }
            }
        }

        save_button(rbuf, "output.ppm");
    }
}

int main()
{
    make_button();
}
