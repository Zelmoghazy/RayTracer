#ifndef BASE_GR_H_
#define BASE_GR_H_

#include "util.h"

typedef struct image_view_t
{
    color4_t    *pixels;
    u32         width;
    u32         height;
}image_view_t;

#define BUF_AT(C,x,y)   (C)->pixels[(x)+(y)*C->width]

#define FONT_ROWS           6   
#define FONT_COLS           18

#define ASCII_LOW           32
#define ASCII_HIGH          126

typedef struct font_t
{
    u32 const       *font_pixels;
    u32              font_width;
    u32              font_height;
    u32              font_char_width;
    u32              font_char_height;
    rect_t           glyph_table[ASCII_HIGH - ASCII_LOW + 1];
}font_t;

typedef struct rendered_text_t
{
    font_t           *font;
    char             *string;
    u32              size;
    vec2_t           pos;       
    color4_t         color;     
    u32              scale;     
}rendered_text_t;

typedef struct scissor_region_t
{
    i32 x,y,w,h;
    bool enabled;
}scissor_region_t;


#define RGBA_TO_U32(r, g, b, a)  ((unsigned)(r) | ((unsigned)(g) << 8) | ((unsigned)(b) << 16) | ((unsigned)(a) << 24))
#define HEX_TO_COLOR4(hex) (color4_t){((hex >> 16) & 0xFF), ((hex >> 8) & 0xFF), (hex & 0xFF), 255}   
#define HEX_TO_RGBA(s,hex)  \
s.r = ((hex >> 16) & 0xFF); \
s.g = ((hex >> 8) & 0xFF);  \
s.b = (hex & 0xFF);         \
s.a = 255     

#define COLOR_RED           (color4_t){255, 0, 0, 255}
#define COLOR_GREEN         (color4_t){0, 255, 0, 255}
#define COLOR_BLUE          (color4_t){0, 0, 255, 255}
#define COLOR_WHITE         (color4_t){255, 255, 255, 255}
#define COLOR_BLACK         (color4_t){0, 0, 0, 255}
#define COLOR_YELLOW        (color4_t){255, 255, 0, 255}
#define COLOR_CYAN          (color4_t){0, 255, 255, 255}
#define COLOR_MAGENTA       (color4_t){255, 0, 255, 255}
#define COLOR_ORANGE        (color4_t){255, 165, 0, 255}
#define COLOR_PURPLE        (color4_t){128, 0, 128, 255}
#define COLOR_PINK          (color4_t){255, 192, 203, 255}
#define COLOR_GRAY          (color4_t){128, 128, 128, 255}
#define COLOR_LIGHT_GRAY    (color4_t){211, 211, 211, 255}
#define COLOR_DARK_GRAY     (color4_t){169, 169, 169, 255}
#define COLOR_BROWN         (color4_t){139, 69, 19, 255}
#define COLOR_NAVY          (color4_t){0, 0, 128, 255}
#define COLOR_LIME          (color4_t){0, 255, 0, 255}
#define COLOR_TEAL          (color4_t){0, 128, 128, 255}
#define COLOR_MAROON        (color4_t){128, 0, 0, 255}
#define COLOR_OLIVE         (color4_t){128, 128, 0, 255}


#define MAX_SCISSOR_STACK 16

extern scissor_region_t scissor_stack[MAX_SCISSOR_STACK];
extern u32 scissor_stack_size;
extern rect_t current_scissor;
extern bool scissor_enabled;

color4_t to_color4(vec3f_t const c);
vec3f_t linear_to_gamma(vec3f_t color);
inline void set_pixel(image_view_t const *img, i32 x, i32 y, color4_t color);
inline color4_t get_pixel(image_view_t const *img, i32 x, i32 y);
inline color4_t blend_pixel(color4_t dst, color4_t src);
inline void set_pixel_blend(image_view_t const *img, i32 x, i32 y, color4_t color);
inline void set_pixel_weighted(image_view_t *img, i32 x, i32 y, color4_t color, i8 weight);
void draw_pixel(image_view_t *img, i32 x, i32 y, color4_t color);
void clear_screen(image_view_t const *color_buf, color4_t const color);
void draw_hline(image_view_t const *color_buf, i32 y, i32 x0, i32 x1, color4_t const color);
void draw_vline(image_view_t const *color_buf, i32 x, i32 y0, i32 y1, color4_t const color);
void draw_line(image_view_t const *color_buf, i32 x0, i32 y0, i32 x1, i32 y1, color4_t const color);
void draw_aaline(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, color4_t color);
void draw_rect_outline_wh(image_view_t const *color_buf, int x0, int y0, int w , int h , color4_t const color);
void draw_rect_outline(image_view_t const *color_buf, int x0, int y0, int x1, int y1, color4_t const color);
void draw_rect_solid_wh(image_view_t const *color_buf, int x0, int y0, int w , int h , color4_t const color);
void draw_rect_solid(image_view_t const *color_buf, int x0, int y0, int x1, int y1, color4_t const color);
void draw_circle(image_view_t *img, i32 cx, i32 cy, i32 radius, color4_t color);
void fill_circle(image_view_t *img, i32 cx, i32 cy, i32 radius, color4_t color);
void draw_ellipse(image_view_t *img, i32 cx, i32 cy, i32 rx, i32 ry, color4_t color);
void fill_ellipse(image_view_t *img, i32 cx, i32 cy, i32 rx, i32 ry, color4_t color);
void draw_triangle(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 x3, i32 y3, color4_t color);
void fill_triangle_scanline(image_view_t *img, i32 y, i32 x1, i32 x2, color4_t color);
void fill_triangle(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 x3, i32 y3, color4_t color);
void draw_arc_quadrant(image_view_t *img, i32 cx, i32 cy, i32 radius, i32 quadrant, color4_t color);
void fill_arc_quadrant(image_view_t *img, i32 cx, i32 cy, i32 radius, int quadrant, color4_t color);
void draw_rounded_rectangle(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 radius, color4_t color);
void fill_rounded_rectangle_wh(image_view_t *img, i32 x, i32 y, i32 width, i32 height, i32 radius, color4_t color);
void export_image(image_view_t const *color_buf, const char *filename);

void render_glyph_to_buffer(rendered_text_t *text, u32 glyph_idx,
                               image_view_t const *color_buf,
                               u32 dst_x, u32 dst_y);
void render_n_string_abs(image_view_t const *color_buf, rendered_text_t *text);
font_t* init_font(u32 *font_pixels);


bool inside_rect(i32 x, i32 y, rect_t *s);
rect_t intersect_rects(const rect_t *a, const rect_t *b);
void update_current_scissor(void);
bool push_scissor(i32 x, i32 y, u32 width, u32 height);
bool pop_scissor();
bool pixel_in_current_scissor(i32 x, i32 y);
bool rect_intersects_current_scissor(i32 x, i32 y, u32 width, u32 height);
void clip_rect_to_current_scissor(i32 *x, i32 *y, u32 *width, u32 *height);
void set_pixel_scissored(image_view_t const *img, i32 x, i32 y, color4_t color);
void draw_rect_scissored(image_view_t const *img, i32 x, i32 y, u32 width, u32 height, color4_t color);
void clear_scissor_stack();

#endif