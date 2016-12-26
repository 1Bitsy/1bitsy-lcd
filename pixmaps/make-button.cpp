#define SUBPIX

#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "agg_conv_stroke.h"
#include "agg_font_freetype.h"
#include "agg_pixfmt_rgb.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_renderer_base.h"
#include "agg_renderer_scanline.h"
#include "agg_rounded_rect.h"
#ifdef SUBPIX
    #include "agg_pixfmt_rgb24_lcd.h"
#endif

static const char *const font_directories[] = {
    "fonts",
    "pixmaps/fonts",
#ifdef __APPLE__
    "~/Library/Fonts",
    "/Library/Fonts",
    "/Network/Library/Fonts",
    "/System/Library/Fonts",
#else
    #error "add your platform's font path here"
#endif
    NULL
};

enum button_position {
    up, dn
};

typedef agg::rendering_buffer rendering_buf;
typedef agg::pixfmt_rgb24 pixfmt_type;
typedef pixfmt_type::color_type color_type;
typedef agg::renderer_base<pixfmt_type> base_renderer;
typedef agg::renderer_scanline_aa_solid<base_renderer> solid_renderer;
#ifdef SUBPIX
    typedef agg::pixfmt_rgb24_lcd pixfmt_lcd_type;
    typedef agg::renderer_base<pixfmt_lcd_type> base_renderer_lcd;
    typedef agg::renderer_scanline_aa_solid<base_renderer_lcd>
            solid_renderer_lcd;
#endif

typedef agg::rasterizer_scanline_aa<> scanline_rasterizer;

typedef agg::font_engine_freetype_int32 font_engine_type;
typedef agg::font_cache_manager<font_engine_type> font_manager_type;
typedef agg::conv_curve<font_manager_type::path_adaptor_type> font_curves_type;

struct button_params {

    button_position position;

    double          width;
    double          height;
    double          radius;
    color_type      bg_color;
    color_type      outline_color;
    double          outline_width;

    std::string     label_text;
    std::string     label_font;
    double          label_font_size;
    color_type      label_color;

    button_params() {}

    button_params(button_position pos)
        : width(50.0),
          height(20.0),
          radius(4.5),
          bg_color(pos ? color_type(0x6a, 0x6a, 0x6a)
                       : color_type(0xff, 0xff, 0xff)),
          outline_color(pos ? color_type(0x20, 0x20, 0x20)
                            : color_type(0xac, 0xac, 0xac)),
          outline_width(0.5),
          label_font("Ubuntu-C"),
          label_font_size(14),
          label_color(pos ? color_type(0xdf, 0xdf, 0xdf)
                          : color_type(0x9e, 0x9e, 0x9e))
    {}

};

struct label_metrics {
    double width;
    double ascent;
    double descent;
};

static button_params params;
static const char *outfile = "button.ppm";

std::string find_font(const std::string& name)
{
    for (const char *const *dir = font_directories; *dir; dir++) {
        std::string path = *dir;
        if (path.substr(0, 2) == "~/") {
            const char *home = getenv ("HOME");
            if (!home)
                continue;       // -> next font directory
            path = home + path.substr(1);
        }
        path += std::string("/") + name + ".ttf";
        int fd = open(path.c_str(), O_RDONLY);
        if (fd != -1) {
            close(fd);
            return path;
        }
    }
    fprintf(stderr, "make-button: can't find font file %s.ttf\n", name.c_str());
    exit(1);
}

template <class PF, class REN>
label_metrics draw_text(PF& pixf,
                        scanline_rasterizer& ras,
                        REN& renderer,
                        agg::scanline_u8& sl,
                        double subpixel_scale,
                        double initial_x,
                        double initial_y,
                        bool dry_run)
{
    font_engine_type feng;
    font_manager_type fman(feng);
    font_curves_type curves(fman.path_adaptor());
    // const char *font_name = params.label_font.c_str();
    std::string font_path = find_font(params.label_font);
    if (0 || !feng.load_font(font_path.c_str(), 0, agg::glyph_ren_outline)) {
        perror(font_path.c_str());
        exit(1);
    }
    feng.hinting(false);
    feng.height(params.label_font_size);
    feng.width(subpixel_scale * params.label_font_size);
    feng.flip_y(true);

    double x = subpixel_scale * initial_x;
    double y = initial_y;
    for (const char *p = params.label_text.c_str(); *p; p++) {
        const agg::glyph_cache *glyph = fman.glyph(*p);
        if (glyph) {
            fman.add_kerning(&x, &y);
            if (!dry_run) {
                fman.init_embedded_adaptors(glyph, x, y);
                ras.reset();
                ras.add_path(curves);
            }
            x += glyph->advance_x;
            y += glyph->advance_y;
        }
        if (!dry_run) {
            renderer.color(params.label_color);
            agg::render_scanlines(ras, sl, renderer);
        }
    }

    label_metrics m = {
        (x - initial_x) / subpixel_scale,
        feng.ascender(),
        feng.descender(),
    };
    return m;
}

static void render_button(rendering_buf& rbuf)
{
#ifdef SUBPIX
    const int subpixel_scale = 3.0;
#else
    const int subpixel_scale = 1.0;
#endif

    // Getting AGG ready to draw is tedious.
    pixfmt_type pixf(rbuf);

    // Renderer Base: bounds clipping.
    base_renderer rbase(pixf);
    
    // Rasterizer: converst polygons to spans
    scanline_rasterizer ras;

    // Scanline: stores a scanline
    agg::scanline_u8 sl;

    // solid renderer: renders solid colored regions
    solid_renderer solid(rbase);

#ifdef SUBPIX
    agg::lcd_distribution_lut lut(3./9., 2./9., 1./9.);
    pixfmt_lcd_type pixf_lcd(rbuf, lut);
    base_renderer_lcd rbase_lcd(pixf_lcd);
    solid_renderer_lcd solid_lcd(rbase_lcd);
#endif
    
    // button shape
    typedef agg::rounded_rect button_shape_type;
    typedef agg::conv_stroke<button_shape_type> stroke_converter_type;

    double w = params.width;
    double h = params.height;
    double r = params.radius;
    button_shape_type button_shape(0.5, 0.5, w - 0.5, h - 0.5, r);

    // background fill
    ras.add_path(button_shape);
    solid.color(params.bg_color);
    agg::render_scanlines(ras, sl, solid);

    // black outline
    stroke_converter_type outline(button_shape);
    outline.width(params.outline_width);
    ras.add_path(outline);
    solid.color(params.outline_color);
    agg::render_scanlines(ras, sl, solid);

#ifdef SUBPIX
    label_metrics m = draw_text(pixf_lcd,
                                ras,
                                solid_lcd,
                                sl,
                                subpixel_scale,
                                0, 0,
                                true);
#else
    label_metrics m = draw_text(pixf,
                                ras,
                                solid,
                                sl,
                                subpixel_scale,
                                0, 0,
                                true);
#endif

    double x = (params.width - m.width) / 2;
    double y = round((2 * params.height - m.ascent - m.descent) / 2);
#ifdef SUBPIX
    (void)draw_text(pixf_lcd, ras, solid_lcd, sl, subpixel_scale, x, y, false);
#else
    (void)draw_text(pixf, ras, solid, sl, subpixel_scale, x, y, false);
#endif
}

static void write_ppm_file(const rendering_buf& rbuf, FILE *out)
{
    unsigned width = rbuf.width();
    unsigned height = rbuf.height();
    fprintf(out, "P6 %u %u 255 ", width, height);
    fwrite(rbuf.buf(), 1, width * height * 3, out);
}

static void save_button(const rendering_buf& rbuf, const char *outfile)
{
    FILE *f = fopen(outfile, "wb");
    if (!f)
        perror(outfile), exit(1);
    write_ppm_file(rbuf, f);
    fclose(f);
}

// Getting AGG ready to draw is tedious.
static void make_button(void)
{
    // Raw memory buffer
    const size_t width = params.width;
    const size_t height = params.height;
    const size_t buffer_bytes = width * height * 3;
    agg::int8u buffer[buffer_bytes];

    // Erase to gray88
    memset(buffer, 0xe0, buffer_bytes);

    {
        // Rendering Buffer: rows and row stride.
        rendering_buf rbuf(buffer, width, height, width * 3);

        render_button(rbuf);
        save_button(rbuf, outfile);
    }
}

// use: make-button [opts] "label" up | down

static void usage(FILE *out)
{
    fprintf(out, "use: make-button label in|out\n");
    if (out == stderr)
        exit(1);
}

static void parse_args(int argc, char *argv[])
{
    static const option longopts[] = {
        // someday I'll get ambitious and make it possible to
        // override most of the params.
        { "output", required_argument, NULL, 'o' },
        { NULL,     0,                 NULL, 0   },
    };
    int ch;
    while ((ch = getopt_long(argc, argv, "o:", longopts, NULL)) != -1) {
        switch (ch) {

        case 'o':
            outfile = optarg;
            break;

        default:
            fprintf(stderr, "getopt returned %d\n", ch);
            exit(1);
        }
    }
    if (optind != argc - 2)
        usage(stderr);
    const char *updown = argv[optind + 1];
    if (!strcmp(updown, "up"))
        params = button_params(up);
    else if (!strcmp(updown, "down"))
        params = button_params(dn);
    else
        usage(stderr);
    params.label_text = argv[optind];
}


int main(int argc, char *argv[])
{
    parse_args(argc, argv);
    make_button();
}
