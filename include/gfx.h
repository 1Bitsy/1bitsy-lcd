#ifndef GFX_included
#define GFX_included

#include <gfx-types.h>

// Pixels
extern void gfx_fill_pixel                         (gfx_pixtile *tile,
                                                    int x, int y,
                                                    gfx_rgb888 color);
extern void gfx_fill_pixel_blend                   (gfx_pixtile *tile,
                                                    int x, int y,
                                                    gfx_rgb888 color,
                                                    gfx_alpha8 alpha);
extern void gfx_fill_pixel_unclipped               (gfx_pixtile *tile,
                                                    int x, int y,
                                                    gfx_rgb888 color);
extern void gfx_fill_pixel_blend_unclipped         (gfx_pixtile *tile,
                                                    int x, int y,
                                                    gfx_rgb888 color,
                                                    gfx_alpha8 alpha);

// Pixel Spans
// Fill from (x0, y) to (x1, y) inclusive.
extern void gfx_fill_span                          (gfx_pixtile *tile,
                                                    int x0, int x1, int y,
                                                    gfx_rgb888 color);
extern void gfx_fill_span_blend                    (gfx_pixtile *tile,
                                                    int x0, int x1, int y,
                                                    gfx_rgb888 color,
                                                    gfx_alpha8 alpha);
extern void gfx_fill_span_unclipped                (gfx_pixtile *tile,
                                                    int x0, int x1, int y,
                                                    gfx_rgb888 color);
extern void gfx_fill_span_blend_unclipped          (gfx_pixtile *tile,
                                                    int x0, int x1, int y,
                                                    gfx_rgb888 color,
                                                    gfx_alpha8 alpha);

// Lines
// Draw from (x0, y0) to (y0, y1) with subpixel positioning.
extern void gfx_draw_line                          (gfx_pixtile *tile,
                                                    float x0, float y0,
                                                    float x1, float y1,
                                                    gfx_rgb888 color);
extern void gfx_draw_line_aa                       (gfx_pixtile *tile,
                                                    float x0, float y0,
                                                    float x1, float y1,
                                                    gfx_rgb888 color);
extern void gfx_draw_line_blend                    (gfx_pixtile *tile,
                                                    float x0, float y0,
                                                    float x1, float y1,
                                                    gfx_rgb888 color,
                                                    gfx_alpha8 alpha);
extern void gfx_draw_line_aa_blend                 (gfx_pixtile *tile,
                                                    float x0, float y0,
                                                    float x1, float y1,
                                                    gfx_rgb888 color,
                                                    gfx_alpha8 alpha);
extern void gfx_draw_line_unclipped                (gfx_pixtile *tile,
                                                    float x0, float y0,
                                                    float x1, float y1,
                                                    gfx_rgb888 color);
extern void gfx_draw_line_aa_unclipped             (gfx_pixtile *tile,
                                                    float x0, float y0,
                                                    float x1, float y1,
                                                    gfx_rgb888 color);
extern void gfx_draw_line_blend_unclipped          (gfx_pixtile *tile,
                                                    float x0, float y0,
                                                    float x1, float y1,
                                                    gfx_rgb888 color,
                                                    gfx_alpha8 alpha);
extern void gfx_draw_line_aa_blend_unclipped       (gfx_pixtile *tile,
                                                    float x0, float y0,
                                                    float x1, float y1,
                                                    gfx_rgb888 color,
                                                    gfx_alpha8 alpha);

// Trapezoids
extern void gfx_fill_trapezoids                    (gfx_pixtile *tile,
                                                    gfx_trapezoid *zoids,
                                                    size_t count,
                                                    gfx_rgb888 color);
extern void gfx_fill_trapezoids_aa                 (gfx_pixtile *tile,
                                                    gfx_trapezoid *zoids,
                                                    size_t count,
                                                    gfx_rgb888 color);
extern void gfx_fill_trapezoids_blend              (gfx_pixtile *tile,
                                                    gfx_trapezoid *zoids,
                                                    size_t count,
                                                    gfx_rgb888 color,
                                                    gfx_alpha8 alpha);
extern void gfx_fill_trapezoids_aa_blend           (gfx_pixtile *tile,
                                                    gfx_trapezoid *zoids,
                                                    size_t count,
                                                    gfx_rgb888 color,
                                                    gfx_alpha8 alpha);
extern void gfx_fill_trapezoids_unclipped          (gfx_pixtile *tile,
                                                    gfx_trapezoid *zoids,
                                                    size_t count,
                                                    gfx_rgb888 color);
extern void gfx_fill_trapezoids_aa_unclipped       (gfx_pixtile *tile,
                                                    gfx_trapezoid *zoids,
                                                    size_t count,
                                                    gfx_rgb888 color);
extern void gfx_fill_trapezoids_blend_unclipped    (gfx_pixtile *tile,
                                                    gfx_trapezoid *zoids,
                                                    size_t count,
                                                    gfx_rgb888 color,
                                                    gfx_alpha8 alpha);
extern void gfx_fill_trapezoids_aa_blend_unclipped (gfx_pixtile *tile,
                                                    gfx_trapezoid *zoids,
                                                    size_t count,
                                                    gfx_rgb888 color,
                                                    gfx_alpha8 alpha);

// Triangles
extern void gfx_fill_triangle                      (gfx_pixtile *tile,
                                                    gfx_trapezoid *zoids,
                                                    size_t count,
                                                    gfx_rgb888 color);
extern void gfx_fill_triangle_aa                   (gfx_pixtile *tile,
                                                    gfx_trapezoid *zoids,
                                                    size_t count,
                                                    gfx_rgb888 color);
extern void gfx_fill_triangle_blend                (gfx_pixtile *tile,
                                                    gfx_trapezoid *zoids,
                                                    size_t count,
                                                    gfx_rgb888 color,
                                                    gfx_alpha8 alpha);
extern void gfx_fill_triangle_aa_blend             (gfx_pixtile *tile,
                                                    gfx_trapezoid *zoids,
                                                    size_t count,
                                                    gfx_rgb888 color,
                                                    gfx_alpha8 alpha);
extern void gfx_fill_triangle_unclipped            (gfx_pixtile *tile,
                                                    gfx_trapezoid *zoids,
                                                    size_t count,
                                                    gfx_rgb888 color);
extern void gfx_fill_triangle_aa_unclipped         (gfx_pixtile *tile,
                                                    gfx_trapezoid *zoids,
                                                    size_t count,
                                                    gfx_rgb888 color);
extern void gfx_fill_triangle_blend_unclipped      (gfx_pixtile *tile,
                                                    gfx_trapezoid *zoids,
                                                    size_t count,
                                                    gfx_rgb888 color,
                                                    gfx_alpha8 alpha);
extern void gfx_fill_triangle_aa_blend_unclipped   (gfx_pixtile *tile,
                                                    gfx_trapezoid *zoids,
                                                    size_t count,
                                                    gfx_rgb888 color,
                                                    gfx_alpha8 alpha);

#endif /* !GFX_included */
