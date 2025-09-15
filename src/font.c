#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#define ATLAS_WIDTH 1024
#define ATLAS_HEIGHT 1024
#define ASCII_LOW 32
#define ASCII_HIGH 126
#define GLYPH_COUNT (ASCII_HIGH - ASCII_LOW + 1)

typedef struct {
    u32 x, y, w, h;        // Position and size in atlas
    float advance;         // Horizontal advance
    int bearing_x, bearing_y;  // Bearing offsets
} glyph_info_t;

typedef struct {
    stbtt_fontinfo info;
    u8 *font_buffer;
    float font_size;
    float scale;
    int ascent, descent, line_gap;
    
    // Atlas texture data
    u8 *atlas;             // Grayscale atlas bitmap
    u32 atlas_width, atlas_height;
    
    // Glyph lookup table
    glyph_info_t glyphs[GLYPH_COUNT];
    
    // Atlas packing state
    u32 pack_x, pack_y, row_height;
} font_t;

typedef struct {
    font_t *font;
    char *string;
    vec2_t pos;
    float scale;
    color4_t color;
} rendered_text_t;

// Initialize font and bake atlas
font_t* init_font(u8 *font_buffer, float font_size)
{
    font_t *font = malloc(sizeof(font_t));
    if (!font) return NULL;
    
    memset(font, 0, sizeof(font_t));
    font->font_buffer = font_buffer;
    font->font_size = font_size;
    font->atlas_width = ATLAS_WIDTH;
    font->atlas_height = ATLAS_HEIGHT;
    
    if (!stbtt_InitFont(&font->info, font_buffer, 0)) {
        fprintf(stderr, "Failed to initialize font\n");
        free(font);
        return NULL;
    }
    
    font->scale = stbtt_ScaleForPixelHeight(&font->info, font_size);
    stbtt_GetFontVMetrics(&font->info, &font->ascent, &font->descent, &font->line_gap);
    
    font->ascent = (int)(font->ascent * font->scale);
    font->descent = (int)(font->descent * font->scale);
    font->line_gap = (int)(font->line_gap * font->scale);
    
    // Allocate atlas
    font->atlas = calloc(ATLAS_WIDTH * ATLAS_HEIGHT, sizeof(u8));
    if (!font->atlas) {
        free(font);
        return NULL;
    }
    
    // Bake all ASCII glyphs into atlas
    if (!bake_font_atlas(font)) {
        free(font->atlas);
        free(font);
        return NULL;
    }
    
    return font;
}

font_t* load_font_from_file(const char *filename, float font_size)
{
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open font file: %s\n", filename);
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    u8 *buffer = malloc(size);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    fread(buffer, 1, size, file);
    fclose(file);
    
    return init_font(buffer, font_size);
}

// Pack a glyph into the atlas
int pack_glyph(font_t *font, int codepoint, u8 *bitmap, int w, int h, 
               int bearing_x, int bearing_y, float advance)
{
    // Check if glyph fits in current row
    if (font->pack_x + w > font->atlas_width) {
        // Move to next row
        font->pack_x = 0;
        font->pack_y += font->row_height;
        font->row_height = 0;
    }
    
    // Check if we have enough vertical space
    if (font->pack_y + h > font->atlas_height) {
        fprintf(stderr, "Atlas full! Cannot pack more glyphs\n");
        return 0;
    }
    
    int glyph_idx = codepoint - ASCII_LOW;
    if (glyph_idx < 0 || glyph_idx >= GLYPH_COUNT) {
        return 0;
    }
    
    // Store glyph info
    font->glyphs[glyph_idx] = (glyph_info_t){
        .x = font->pack_x,
        .y = font->pack_y,
        .w = w,
        .h = h,
        .bearing_x = bearing_x,
        .bearing_y = bearing_y,
        .advance = advance
    };
    
    // Copy bitmap to atlas
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int atlas_idx = (font->pack_y + y) * font->atlas_width + (font->pack_x + x);
            int bitmap_idx = y * w + x;
            font->atlas[atlas_idx] = bitmap[bitmap_idx];
        }
    }
    
    // Update packing position
    font->pack_x += w + 1; // Add 1 pixel padding
    if (h > font->row_height) {
        font->row_height = h + 1; // Add 1 pixel padding
    }
    
    return 1;
}

// Bake all ASCII characters into the atlas
int bake_font_atlas(font_t *font)
{
    font->pack_x = 0;
    font->pack_y = 0;
    font->row_height = 0;
    
    for (int codepoint = ASCII_LOW; codepoint <= ASCII_HIGH; codepoint++) {
        int advance, left_bearing;
        stbtt_GetCodepointHMetrics(&font->info, codepoint, &advance, &left_bearing);
        
        int w, h, xoff, yoff;
        u8 *bitmap = stbtt_GetCodepointBitmap(&font->info, 0, font->scale,
                                              codepoint, &w, &h, &xoff, &yoff);
        
        if (bitmap) {
            float scaled_advance = advance * font->scale;
            
            if (!pack_glyph(font, codepoint, bitmap, w, h, xoff, yoff, scaled_advance)) {
                stbtt_FreeBitmap(bitmap, NULL);
                return 0;
            }
            
            stbtt_FreeBitmap(bitmap, NULL);
        } else {
            // Handle characters without bitmaps (like space)
            float scaled_advance = advance * font->scale;
            pack_glyph(font, codepoint, NULL, 0, 0, 0, 0, scaled_advance);
        }
    }
    
    return 1;
}

void free_font(font_t *font)
{
    if (!font) return;
    
    if (font->atlas) {
        free(font->atlas);
    }
    
    if (font->font_buffer) {
        free(font->font_buffer);
    }
    
    free(font);
}

// Fast atlas sampling for glyph rendering
void render_glyph_to_buffer(rendered_text_t *text, int codepoint,
                           image_view_t const *color_buf,
                           u32 dst_x, u32 dst_y)
{
    int glyph_idx = codepoint - ASCII_LOW;
    if (glyph_idx < 0 || glyph_idx >= GLYPH_COUNT) {
        return;
    }
    
    const glyph_info_t *glyph = &text->font->glyphs[glyph_idx];
    
    // Skip empty glyphs (like space)
    if (glyph->w == 0 || glyph->h == 0) {
        return;
    }
    
    u32 scaled_w = (u32)(glyph->w * text->scale);
    u32 scaled_h = (u32)(glyph->h * text->scale);
    
    int render_x = (int)dst_x + (int)(glyph->bearing_x * text->scale);
    int render_y = (int)dst_y + text->font->ascent + (int)(glyph->bearing_y * text->scale);
    
    // Bounds checking
    if (render_x >= (int)color_buf->width || render_y >= (int)color_buf->height ||
        render_x + (int)scaled_w <= 0 || render_y + (int)scaled_h <= 0) {
        return;
    }
    
    u32 x_start = (render_x < 0) ? 0 : render_x;
    u32 y_start = (render_y < 0) ? 0 : render_y;
    u32 x_end = (render_x + (int)scaled_w > (int)color_buf->width) ? 
                color_buf->width : render_x + scaled_w;
    u32 y_end = (render_y + (int)scaled_h > (int)color_buf->height) ? 
                color_buf->height : render_y + scaled_h;
    
    float x_scale = (float)glyph->w / scaled_w;
    float y_scale = (float)glyph->h / scaled_h;
    
    // Sample from atlas
    for (u32 y = y_start; y < y_end; y++) {
        for (u32 x = x_start; x < x_end; x++) {
            u32 rel_x = x - render_x;
            u32 rel_y = y - render_y;
            
            u32 atlas_x = glyph->x + (u32)(rel_x * x_scale);
            u32 atlas_y = glyph->y + (u32)(rel_y * y_scale);
            
            if (atlas_x < glyph->x + glyph->w && atlas_y < glyph->y + glyph->h) {
                u32 atlas_idx = atlas_y * text->font->atlas_width + atlas_x;
                u8 alpha = text->font->atlas[atlas_idx];
                
                if (alpha > 0) {
                    // Alpha blending
                    float a = alpha / 255.0f;
                    color4_t dst_pixel = BUF_AT(color_buf, x, y);
                    
                    color4_t blended = {
                        (u32)(text->color.r * a + dst_pixel.r * (1.0f - a)),
                        (u32)(text->color.g * a + dst_pixel.g * (1.0f - a)),
                        (u32)(text->color.b * a + dst_pixel.b * (1.0f - a)),
                        255
                    };
                    
                    BUF_AT(color_buf, x, y) = blended;
                }
            }
        }
    }
}

void render_string(image_view_t const *color_buf, rendered_text_t *text)
{
    if (!text || !text->string || !text->font) return;
    
    char *string = text->string;
    float x = text->pos.x;
    float y = text->pos.y;
    float original_x = x;
    
    int line_height = text->font->ascent - text->font->descent + text->font->line_gap;
    
    while (*string) {
        if (*string == '\n') {
            y += line_height * text->scale;
            x = original_x;
            string++;
            continue;
        }
        
        if (*string == '\t') {
            // Tab handling - advance by 4 spaces worth
            int space_idx = ' ' - ASCII_LOW;
            if (space_idx >= 0 && space_idx < GLYPH_COUNT) {
                x += text->font->glyphs[space_idx].advance * text->scale * 4;
            }
            string++;
            continue;
        }
        
        if (*string >= ASCII_LOW && *string <= ASCII_HIGH) {
            render_glyph_to_buffer(text, *string, color_buf, (u32)x, (u32)y);
            
            int glyph_idx = *string - ASCII_LOW;
            x += text->font->glyphs[glyph_idx].advance * text->scale;
        }
        
        string++;
    }
}

// Convenience function for simple text rendering
void render_text_simple(image_view_t const *color_buf, font_t *font, 
                       const char *text, float x, float y, float scale, 
                       color4_t color)
{
    rendered_text_t render_info = {
        .font = font,
        .string = (char*)text,
        .pos = {x, y},
        .scale = scale,
        .color = color
    };
    
    render_string(color_buf, &render_info);
}

// Get string width for layout purposes
float get_string_width(font_t *font, const char *text, float scale)
{
    if (!text || !font) return 0.0f;
    
    float width = 0.0f;
    const char *p = text;
    
    while (*p) {
        if (*p == '\n') {
            break; // Only measure until newline
        }
        
        if (*p >= ASCII_LOW && *p <= ASCII_HIGH) {
            int glyph_idx = *p - ASCII_LOW;
            width += font->glyphs[glyph_idx].advance * scale;
        }
        p++;
    }
    
    return width;
}

int get_line_height(font_t *font)
{
    return font->ascent - font->descent + font->line_gap;
}

// Optional: Save atlas to file for debugging
void save_atlas_to_file(font_t *font, const char *filename)
{
    FILE *file = fopen(filename, "wb");
    if (!file) return;
    
    // Simple PGM format
    fprintf(file, "P5\n%d %d\n255\n", font->atlas_width, font->atlas_height);
    fwrite(font->atlas, 1, font->atlas_width * font->atlas_height, file);
    fclose(file);
}