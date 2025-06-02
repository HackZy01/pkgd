// you need the Oopo ps3libraries to work with freetype

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftglyph.h>
#include "ttf_render.h"

/******************************************************************************************************************************************************/
/* TTF functions to load and convert fonts                                                                                                             */
/******************************************************************************************************************************************************/

static int ttf_inited = 0;

static FT_Library freetype;
static FT_Face face[4];
static int f_face[4] = {0, 0, 0, 0};

// Define the path for the PS3 system font for easier modification
#define PS3_SYSTEM_FONT_PATH "/dev_flash/data/font/SCE-PS3-SR-R-LATIN2.TTF"

int TTFLoadFont(int set, char * path, void * from_memory, int size_from_memory)
{
    FT_Error error;

    if (set < 0 || set >= 4) {
        // Invalid set index
        return -1;
    }

    if (!ttf_inited) {
        error = FT_Init_FreeType(&freetype);
        if (error) {
            // fprintf(stderr, "TTFLoadFont: FT_Init_FreeType failed (error: %d)\n", error);
            return -1; // Failed to initialize FreeType
        }
        ttf_inited = 1;
    }

    // If a face is already loaded in this slot, it should be disposed of first.
    // FT_Done_Face(face[set]); // The original code doesn't do this, implies slots are overwritten or managed by caller.
    // For safety, let's assume FT_New_Face/FT_New_Memory_Face handles replacing.
    // The original code just overwrites face[set] handle. FreeType might leak if not managed.
    // However, to match original behavior closely, we won't add FT_Done_Face here without more context on app lifecycle.

    f_face[set] = 0; // Mark as not loaded initially

    // Special handling for font slot 0:
    // If the application tries to load an "embedded font" (from_memory) into slot 0,
    // we will attempt to load the PS3 system font from its known path instead.
    if (set == 0 && path == NULL && from_memory != NULL) {
        // Attempt to load the system font
        error = FT_New_Face(freetype, PS3_SYSTEM_FONT_PATH, 0, &face[set]);
        if (error == 0) {
            // System font loaded successfully into slot 0
            // fprintf(stdout, "TTFLoadFont: Successfully loaded system font '%s' into slot 0.\n", PS3_SYSTEM_FONT_PATH);
            f_face[set] = 1;
            return 0;
        } else {
            // System font failed to load. Log this and fall back to the original from_memory request.
            // fprintf(stderr, "TTFLoadFont: Failed to load system font '%s' (error: %d). Falling back to provided memory font for slot 0.\n", PS3_SYSTEM_FONT_PATH, error);
            // The original 'from_memory' and 'size_from_memory' will be used by the logic below.
            // No explicit action needed here for fallback, flow continues to general loading logic.
        }
    }

    // General font loading logic (applies to all sets, or set 0 if system font load failed or wasn't attempted)
    if (path) { // Prioritize path if provided
        error = FT_New_Face(freetype, path, 0, &face[set]);
        if (error != 0) {
            // fprintf(stderr, "TTFLoadFont: FT_New_Face failed for path '%s' (error: %d)\n", path, error);
            return -1; // Path load failed
        }
    } else if (from_memory) { // No path, try memory
        error = FT_New_Memory_Face(freetype, (const FT_Byte*)from_memory, size_from_memory, 0, &face[set]);
        if (error != 0) {
            // fprintf(stderr, "TTFLoadFont: FT_New_Memory_Face failed (error: %d)\n", error);
            return -1; // Memory load failed
        }
    } else {
        // Neither path nor memory buffer provided.
        // fprintf(stderr, "TTFLoadFont: No path or memory buffer provided for font set %d.\n", set);
        return -1;
    }

    f_face[set] = 1; // Mark as loaded
    return 0;
}

/* release all */
void TTFUnloadFont()
{
   if(!ttf_inited) return;
   // Properly release faces before FreeType library
   for(int i=0; i<4; ++i) {
       if(f_face[i] && face[i]) {
           FT_Done_Face(face[i]);
           face[i] = NULL;
           f_face[i] = 0;
       }
   }
   FT_Done_FreeType(freetype);
   ttf_inited = 0;
}

/* function to render the character

chr : character from 0 to 255

bitmap: u8 bitmap passed to render the character character (max 256 x 256 x 1 (8 bits Alpha))

*w : w is the bitmap width as input and the width of the character (used to increase X) as output
*h : h is the bitmap height as input and the height of the character (used to Y correction combined with y_correction) as output

y_correction : the Y correction to display the character correctly in the screen

*/

void TTF_to_Bitmap(u8 chr, u8 * bitmap, short *w, short *h, short *y_correction)
{
    // Ensure FreeType is initialized if it hasn't been by a TTFLoadFont call yet.
    // This could happen if app calls TTF_to_Bitmap without explicit TTFLoadFont.
    if (!ttf_inited) {
        if (FT_Init_FreeType(&freetype)) {
             (*w) = 0; return; // Cannot initialize FreeType
        }
        ttf_inited = 1;
    }

    // Attempt to auto-load default system font into slot 0 if no font is there yet.
    // This ensures that if an app directly calls rendering without loading any font,
    // it tries to use the system font for slot 0.
    if (!f_face[0]) {
        TTFLoadFont(0, PS3_SYSTEM_FONT_PATH, NULL, 0);
        // If loading fails, f_face[0] will remain 0, and subsequent checks will skip it.
    }

    if(f_face[0]) FT_Set_Pixel_Sizes(face[0], (*w), (*h));
    if(f_face[1]) FT_Set_Pixel_Sizes(face[1], (*w), (*h));
    if(f_face[2]) FT_Set_Pixel_Sizes(face[2], (*w), (*h));
    if(f_face[3]) FT_Set_Pixel_Sizes(face[3], (*w), (*h));
    
    FT_GlyphSlot slot = NULL; // Initialize to NULL

    memset(bitmap, 0, (*w) * (*h));

    FT_UInt index;

    if(f_face[0] && face[0] && (index = FT_Get_Char_Index(face[0], (FT_ULong)chr))!=0
        && !FT_Load_Glyph(face[0], index, FT_LOAD_RENDER )) slot = face[0]->glyph;
    else if(f_face[1] && face[1] && (index = FT_Get_Char_Index(face[1], (FT_ULong)chr))!=0
        && !FT_Load_Glyph(face[1], index, FT_LOAD_RENDER )) slot = face[1]->glyph;
    else if(f_face[2] && face[2] && (index = FT_Get_Char_Index(face[2], (FT_ULong)chr))!=0
        && !FT_Load_Glyph(face[2], index, FT_LOAD_RENDER )) slot = face[2]->glyph;
    else if(f_face[3] && face[3] && (index = FT_Get_Char_Index(face[3], (FT_ULong)chr))!=0
        && !FT_Load_Glyph(face[3], index, FT_LOAD_RENDER )) slot = face[3]->glyph;
    else {(*w) = 0; return;}

    // Ensure slot is not NULL before dereferencing
    if (!slot) { (*w) = 0; return; }

    int n, m, ww;

    *y_correction = (*h) - 1 - slot->bitmap_top;
    
    ww = 0;

    for(n = 0; n < slot->bitmap.rows; n++) {
        for (m = 0; m < slot->bitmap.width; m++) {

            if(m >= (*w) || n >= (*h)) continue;
            
            bitmap[m] = (u8) slot->bitmap.buffer[ww + m];
        }
    
    bitmap += *w;

    ww += slot->bitmap.pitch; // Use pitch for safety, though for FT_LOAD_RENDER it's usually width for 8bpp
    }

    *w = ((slot->advance.x + 31) >> 6) + ((slot->bitmap_left < 0) ? -slot->bitmap_left : 0);
    *h = slot->bitmap.rows;
}

int Render_String_UTF8(u16 * bitmap, int w, int h, u8 *string, int sw, int sh)
{
    // Ensure FreeType is initialized
    if (!ttf_inited) {
        if (FT_Init_FreeType(&freetype)) {
            return 0; // Cannot initialize FreeType
        }
        ttf_inited = 1;
    }

    // Attempt to auto-load default system font into slot 0 if no font is there yet.
    if (!f_face[0]) {
        TTFLoadFont(0, PS3_SYSTEM_FONT_PATH, NULL, 0);
    }

    int posx = 0;
    int n, m, ww, ww2;
    u8 color_val; // Renamed from 'color' to avoid conflict with parameter in display_ttf_string
    u32 ttf_char;

    if(f_face[0]) FT_Set_Pixel_Sizes(face[0], sw, sh);
    if(f_face[1]) FT_Set_Pixel_Sizes(face[1], sw, sh);
    if(f_face[2]) FT_Set_Pixel_Sizes(face[2], sw, sh);
    if(f_face[3]) FT_Set_Pixel_Sizes(face[3], sw, sh);

    FT_GlyphSlot slot = NULL;

    memset(bitmap, 0, w * h * 2);

    while(*string) {

        if(*string == 32 || *string == 9) {posx += sw>>1; string++; continue;}

        if(*string & 128) {
            m = 1;

            if((*string & 0xf8)==0xf0) { // 4 bytes
                ttf_char = (u32) (*(string++) & 3);
                m = 3;
            } else if((*string & 0xE0)==0xE0) { // 3 bytes
                ttf_char = (u32) (*(string++) & 0xf);
                m = 2;
            } else if((*string & 0xE0)==0xC0) { // 2 bytes C0 was a typo, should be C0
                ttf_char = (u32) (*(string++) & 0x1f);
                m = 1;
            } else {string++;continue;} // error!

             for(n = 0; n < m; n++) {
                if(!*string) break; // error!
                    if((*string & 0xc0) != 0x80) break; // error!
                    ttf_char = (ttf_char <<6) |((u32) (*(string++) & 63));
             }
           
            if((n != m) && !*string) break; // check if processing finished correctly
        
        } else ttf_char = (u32) *(string++);

        if(ttf_char == 13 || ttf_char == 10) ttf_char='/'; // Simplified newline handling for this function

        FT_UInt index;
        slot = NULL; // Reset slot for each char

        if(f_face[0] && face[0] && (index = FT_Get_Char_Index(face[0], ttf_char))!=0
            && !FT_Load_Glyph(face[0], index, FT_LOAD_RENDER )) slot = face[0]->glyph;
        else if(f_face[1] && face[1] && (index = FT_Get_Char_Index(face[1], ttf_char))!=0
            && !FT_Load_Glyph(face[1], index, FT_LOAD_RENDER )) slot = face[1]->glyph;
        else if(f_face[2] && face[2] && (index = FT_Get_Char_Index(face[2], ttf_char))!=0
            && !FT_Load_Glyph(face[2], index, FT_LOAD_RENDER )) slot = face[2]->glyph;
        else if(f_face[3] && face[3] && (index = FT_Get_Char_Index(face[3], ttf_char))!=0
            && !FT_Load_Glyph(face[3], index, FT_LOAD_RENDER )) slot = face[3]->glyph;
        // If no glyph found or loading failed, slot remains NULL

        if(slot && slot->bitmap.buffer) { // Check slot and buffer
            ww = ww2 = 0;

            int y_correction = sh - 1 - slot->bitmap_top;
            if(y_correction < 0) y_correction = 0;
            
            // Offset to start drawing in the target bitmap, considering y_correction
            // Ensure ww2 does not cause out-of-bounds access if y_correction is large for a small target 'h'
            if (y_correction < h) {
                 ww2 = y_correction * w;
            } else {
                // y_correction is too large, character would be entirely outside the bitmap vertically
                posx += slot->bitmap.width; // Advance position, but don't draw
                if (posx > w) posx = w; // Cap posx to prevent overflow if string is too long for bitmap
                continue;
            }


            for(n = 0; n < slot->bitmap.rows; n++) {
                if(n + y_correction >= h) break; // Stop if character row goes beyond target bitmap height
                for (m = 0; m < slot->bitmap.width; m++) {

                    if(m + posx >= w) break; // Stop if character pixel goes beyond target bitmap width
                    
                    color_val = (u8) slot->bitmap.buffer[ww + m];
                    
                    if(color_val) bitmap[posx + m + ww2] = (color_val<<8) | 0xfff; // A4R4G4B4 format
                }
            
            ww2 += w; // Advance to next line in target bitmap

            ww += slot->bitmap.pitch; // Advance to next line in glyph bitmap
            }
            posx += slot->bitmap.width; // Advance horizontal position by rendered width
            if (posx > w) posx = w; // Cap posx
        } else {
            // Character not found or no bitmap, advance by a default space or sw/2
            posx += sw >> 1;
            if (posx > w) posx = w;
        }
    }
    return posx;
}

// constructor dinamico de fuentes 32 x 32

typedef struct ttf_dyn {
    u32 ttf;
    u16 *text;
    u32 r_use;
    u16 y_start;
    u16 width;
    u16 height;
    u16 flags;

} ttf_dyn;

#define MAX_CHARS 1600

static ttf_dyn ttf_font_datas[MAX_CHARS];

static u32 dyn_r_use= 0; // Renamed from r_use to avoid conflict with global r_use if any

float Y_ttf = 0.0f;
float Z_ttf = 0.0f;

static int Win_X_ttf = 0;
static int Win_Y_ttf = 0;
static int Win_W_ttf = 848;
static int Win_H_ttf = 512;


static u32 Win_flag = 0;

void set_ttf_window(int x, int y, int width, int height, u32 mode)
{
    Win_X_ttf = x;
    Win_Y_ttf = y;
    Win_W_ttf = width;
    Win_H_ttf = height;
    Win_flag = mode;
    Y_ttf = 0.0f;
    Z_ttf = 0.0f;
   
}

u16 * init_ttf_table(u16 *texture)
{
    int n;

    dyn_r_use= 0;
    for(n= 0; n <  MAX_CHARS; n++) {
        memset(&ttf_font_datas[n], 0, sizeof(ttf_dyn));
        ttf_font_datas[n].text = texture;

        texture+= 32*32;
    }

    return texture;

}

void reset_ttf_frame(void)
{
    int n;

    for(n= 0; n <  MAX_CHARS; n++) {
        
        ttf_font_datas[n].flags &= 1; // Keep permanent flag, clear 'in_use_this_frame' flag

    }

    dyn_r_use++;

}

// Stubs for tiny3d functions if not linking against tiny3d for compilation test
#ifndef TINY3D_H // Basic check if tiny3d.h was included (actual content varies)
typedef enum { TINY3D_QUADS } TINY3D_POLYGON_TYPE;
void tiny3d_SetPolygon(TINY3D_POLYGON_TYPE type) {}
void tiny3d_VertexPos(float x, float y, float z) {}
void tiny3d_VertexColor(u32 rgba) {}
void tiny3d_VertexTexture(float u, float v) {}
void tiny3d_End() {}
void tiny3d_SetTextureWrap(int unit, u16* offset, int w, int h, int stride, int format, int wrap_s, int wrap_t, int filter) {}
u16* tiny3d_TextureOffset(void* tex) { return (u16*)tex; }
#define TINY3D_TEX_FORMAT_A4R4G4B4 0
#define TEXTWRAP_CLAMP 0
#define TEXTURE_LINEAR 0
#endif


static void DrawBox_ttf(float x, float y, float z, float w, float h, u32 rgba)
{
    tiny3d_SetPolygon(TINY3D_QUADS);
    
   
    tiny3d_VertexPos(x    , y    , z);
    tiny3d_VertexColor(rgba);

    tiny3d_VertexPos(x + w, y    , z);
    tiny3d_VertexColor(rgba); // Color needs to be set for each vertex if not global

    tiny3d_VertexPos(x + w, y + h, z);
    tiny3d_VertexColor(rgba);

    tiny3d_VertexPos(x    , y + h, z);
    tiny3d_VertexColor(rgba);

    tiny3d_End();
}

static void DrawTextBox_ttf(float x, float y, float z, float w, float h, u32 rgba, float tx, float ty)
{
    tiny3d_SetPolygon(TINY3D_QUADS);
    
   
    tiny3d_VertexPos(x    , y    , z);
    tiny3d_VertexColor(rgba);
    tiny3d_VertexTexture(0.0f , 0.0f);

    tiny3d_VertexPos(x + w, y    , z);
    tiny3d_VertexColor(rgba); // Color needs to be set for each vertex
    tiny3d_VertexTexture(tx, 0.0f);

    tiny3d_VertexPos(x + w, y + h, z);
    tiny3d_VertexColor(rgba);
    tiny3d_VertexTexture(tx, ty);

    tiny3d_VertexPos(x    , y + h, z);
    tiny3d_VertexColor(rgba);
    tiny3d_VertexTexture(0.0f , ty);

    tiny3d_End();
}


#define TTF_UX 30
#define TTF_UY 24


int display_ttf_string(int posx, int posy, const char *string, u32 color, u32 bkcolor, int sw, int sh)
{
    // Ensure FreeType is initialized
    if (!ttf_inited) {
        if (FT_Init_FreeType(&freetype)) {
            return posx; // Cannot initialize FreeType
        }
        ttf_inited = 1;
    }
    // Attempt to auto-load default system font into slot 0 if no font is there yet.
    if (!f_face[0]) {
        TTFLoadFont(0, PS3_SYSTEM_FONT_PATH, NULL, 0);
    }


    int l,n, m, ww, ww2;
    u8 colorc;
    u32 ttf_char;
    u8 *ustring = (u8 *) string;

    int lenx = 0;

    while(*ustring) {

        if(posy >= Win_H_ttf && !(Win_flag & WIN_AUTO_LF)) break; // Stop if past window height unless auto LF can fix

        if(*ustring == 32 || *ustring == 9) {posx += sw>>1; ustring++; continue;}

        if(*ustring & 128) {
            m = 1; // num_extra_bytes

            if((*ustring & 0xf8)==0xf0) { // 4 bytes (1 lead + 3 trail)
                ttf_char = (u32) (*(ustring++) & 0x07); // Mask 00000111
                m = 3;
            } else if((*ustring & 0xE0)==0xE0) { // 3 bytes (1 lead + 2 trail)
                ttf_char = (u32) (*(ustring++) & 0x0f); // Mask 00001111
                m = 2;
            } else if((*ustring & 0xC0)==0xC0) { // 2 bytes (1 lead + 1 trail)
                ttf_char = (u32) (*(ustring++) & 0x1f); // Mask 00011111
                m = 1;
            } else {ustring++;continue;} // Invalid UTF-8 start byte

             for(n = 0; n < m; n++) {
                if(!*ustring) break; // Unexpected end of string
                if((*ustring & 0xc0) != 0x80) break; // Invalid trailing byte
                ttf_char = (ttf_char << 6) | ((u32) (*(ustring++) & 0x3f)); // Mask 00111111
             }
           
            if(n != m) { // Error in UTF-8 sequence
                if (!*ustring) break; // End of string during malformed char
                // Malformed UTF-8, skip or replace with '?'
                ttf_char = '?'; 
            }
        
        } else ttf_char = (u32) *(ustring++);


        if(Win_flag & WIN_SKIP_LF) {
            if(ttf_char == '\r' || ttf_char == '\n') ttf_char='/';
        } else {
            if(Win_flag & WIN_DOUBLE_LF) {
                if(ttf_char == '\r') {if(posx > lenx) lenx = posx; posx = 0;continue;}
                if(ttf_char == '\n') {posy += sh;continue;}
            } else { // Default: \n is newline, \r is ignored or treated as space/char
                if(ttf_char == '\n') {if(posx > lenx) lenx = posx; posx = 0;posy += sh;continue;}
                if(ttf_char == '\r') continue; // Skip CR if not WIN_SKIP_LF
            }
        }

        if(ttf_char < 32 && ttf_char != '\t') ttf_char='?'; // Replace non-printable chars (tab already handled)

        // Search ttf_char in dynamic cache
        if(ttf_char < 128) { // ASCII range directly maps
            n = ttf_char;
        } else { // Extended chars, search or allocate in cache
            int best_candidate_idx = 0; // Index for LRU replacement
            int oldest_r_use_diff = -1;
            
            for(l= 128; l < MAX_CHARS; l++) { // Start search from non-ASCII part of cache
                if(ttf_font_datas[l].ttf == ttf_char && (ttf_font_datas[l].flags & 1)) { // Found and valid
                    n = l; 
                    goto found_char_in_cache;
                }
                
                // For LRU: find an unused slot or the least recently used one
                if (!(ttf_font_datas[l].flags & 1)) { // Not valid (never used or invalidated)
                     if (best_candidate_idx < 128 || !(ttf_font_datas[best_candidate_idx].flags & 1)) { // Prefer unused over LRU
                        best_candidate_idx = l;
                     }
                } else if ((ttf_font_datas[l].flags & 2) == 0) { // Valid but not used this frame yet
                    int current_diff = dyn_r_use - ttf_font_datas[l].r_use;
                    if (best_candidate_idx < 128 || current_diff > oldest_r_use_diff) {
                        oldest_r_use_diff = current_diff;
                        best_candidate_idx = l;
                    }
                }
            }
            // If no exact match, 'n' is not set. Use 'best_candidate_idx' (must be >= 128)
            if (best_candidate_idx < 128) best_candidate_idx = 128; // Ensure it's in the dynamic part
            n = best_candidate_idx;
            ttf_font_datas[n].flags &= ~1; // Mark as not (yet) valid for the new char
        }
    found_char_in_cache:;

        if(n >= MAX_CHARS) n = (ttf_char == '?') ? '?' : 128; // Fallback if cache logic fails (should not happen)
        
        l=n; // 'l' is the cache index to use

        u16 * char_bitmap_cache = ttf_font_datas[l].text;
        
        // Building the character if not in cache or if it's a different char
        if(!(ttf_font_datas[l].flags & 1) || ttf_font_datas[l].ttf != ttf_char) {

            // Set pixel sizes for the font faces
            // These are fixed sizes for texture atlas. Dynamic scaling happens at render time.
            if(f_face[0]) FT_Set_Pixel_Sizes(face[0], TTF_UX, TTF_UY);
            if(f_face[1]) FT_Set_Pixel_Sizes(face[1], TTF_UX, TTF_UY);
            if(f_face[2]) FT_Set_Pixel_Sizes(face[2], TTF_UX, TTF_UY);
            if(f_face[3]) FT_Set_Pixel_Sizes(face[3], TTF_UX, TTF_UY);

            FT_GlyphSlot slot = NULL;

            memset(char_bitmap_cache, 0, 32 * 32 * 2); // Clear 32x32 texture region

            FT_UInt glyph_index;
            u32 char_to_render = ttf_char; // Use a mutable variable for character to render

            // Try loading glyph from available font faces
            if(f_face[0] && face[0] && (glyph_index = FT_Get_Char_Index(face[0], char_to_render))!=0
                && !FT_Load_Glyph(face[0], glyph_index, FT_LOAD_RENDER )) slot = face[0]->glyph;
            else if(f_face[1] && face[1] && (glyph_index = FT_Get_Char_Index(face[1], char_to_render))!=0
                && !FT_Load_Glyph(face[1], glyph_index, FT_LOAD_RENDER )) slot = face[1]->glyph;
            else if(f_face[2] && face[2] && (glyph_index = FT_Get_Char_Index(face[2], char_to_render))!=0
                && !FT_Load_Glyph(face[2], glyph_index, FT_LOAD_RENDER )) slot = face[2]->glyph;
            else if(f_face[3] && face[3] && (glyph_index = FT_Get_Char_Index(face[3], char_to_render))!=0
                && !FT_Load_Glyph(face[3], glyph_index, FT_LOAD_RENDER )) slot = face[3]->glyph;
            else {
                // Glyph not found. If original char was not '?', try rendering '?'
                if (char_to_render != '?') {
                    char_to_render = '?'; // Try to render '?' as fallback
                    if(f_face[0] && face[0] && (glyph_index = FT_Get_Char_Index(face[0], char_to_render))!=0
                        && !FT_Load_Glyph(face[0], glyph_index, FT_LOAD_RENDER )) slot = face[0]->glyph;
                    // Add other font fallbacks for '?' if needed
                }
            }
            
            ttf_font_datas[l].flags = 0; // Reset flags before setting new ones

            if(slot && slot->bitmap.buffer) {
                ww = ww2 = 0;

                int y_correction = TTF_UY - 1 - slot->bitmap_top;
                if(y_correction < 0) y_correction = 0;
                // Ensure y_correction does not exceed character cell height (32)
                if(y_correction >= 32) y_correction = (TTF_UY > 0) ? TTF_UY -1 : 0; 
                                                          // if TTF_UY is small, y_correction might make it invisible. Max it to a line less.

                ttf_font_datas[l].y_start = y_correction; // This is y-offset within the 32x32 cell
                ttf_font_datas[l].height = slot->bitmap.rows;
                ttf_font_datas[l].width = slot->bitmap.width;
                ttf_font_datas[l].ttf = char_to_render; // Store the actual char rendered (could be '?')
                
                // Copy glyph to 32x32 cache texture
                // Note: y_correction is where the TOP of the glyph aligns.
                // The actual drawing into cache should start at (cache_y_offset + y_correction) * 32
                // The original code seems to imply y_start is an offset for rendering FROM cache, not INTO cache.
                // Assuming original intent: y_start is glyph's baseline alignment relative to cell top.
                // The glyph is rendered at top of its 32x32 cell.

                for(n = 0; n < slot->bitmap.rows; n++) {
                    if(n >= 32) break; // Don't overflow 32-pixel height of cache cell
                    for (m = 0; m < slot->bitmap.width; m++) {
                        if(m >= 32) break; // Don't overflow 32-pixel width
                        
                        colorc = (u8) slot->bitmap.buffer[ww + m];
                        
                        if(colorc) char_bitmap_cache[m + ww2] = (colorc << 8) | 0xfff; // A4R4G4B4 format
                    }
                ww2 += 32; // Next line in 32x32 cache cell
                ww += slot->bitmap.pitch; // Next line in glyph's buffer
                }
                ttf_font_datas[l].flags = 1; // Mark as valid/loaded
            } else {
                // Could not render char, mark cache slot as invalid or store minimal info for 'empty'
                ttf_font_datas[l].ttf = char_to_render; // Store what we tried to render
                ttf_font_datas[l].width = sw / 2; // Default width for missing char
                ttf_font_datas[l].height = 0;
                ttf_font_datas[l].y_start = 0;
                ttf_font_datas[l].flags = 1; // Mark as "valid" but it's an empty/placeholder char
            }
        }

        // Displaying the character from cache
        ttf_font_datas[l].flags |= 2; // Mark as 'in use this frame'
        ttf_font_datas[l].r_use = dyn_r_use;

        float char_render_width = ( (float)ttf_font_datas[l].width * sw ) / TTF_UX ; // Scale cached width
        if (char_render_width <= 0 && ttf_font_datas[l].flags & 1) char_render_width = (float)sw / 2.0f; // Min width for valid but zero-width chars


        if((Win_flag & WIN_AUTO_LF) && (posx + char_render_width) > Win_W_ttf) {
            if (posx > lenx) lenx = posx; // Update max line length before wrapping
            posx = 0;
            posy += sh;
            if (posy >= Win_H_ttf) break; // Stop if wrapped past window height
        }

        u32 current_char_color = color;
        
        // Skip if out of window vertically (horizontally handled by WIN_AUTO_LF or clipping)
        if(posy + sh <= 0 || posy >= Win_H_ttf) { // Completely outside top/bottom
             current_char_color = 0; // Don't draw
        }
        // Also consider horizontal clipping if not auto-wrapping
        if (!(Win_flag & WIN_AUTO_LF) && (posx >= Win_W_ttf || posx + char_render_width <= 0)) {
            current_char_color = 0; // Don't draw if completely outside horizontally
        }


        if(current_char_color && (ttf_font_datas[l].flags & 1)) { // If color is not transparent and char is valid
            // Texture is 32x32, char_bitmap_cache points to the specific 32x32 region for this char
            tiny3d_SetTextureWrap(0, tiny3d_TextureOffset(char_bitmap_cache), 32, 32, 32 * 2, // Stride is 32*sizeof(u16)
                TINY3D_TEX_FORMAT_A4R4G4B4, TEXTWRAP_CLAMP, TEXTWRAP_CLAMP, TEXTURE_LINEAR);

            float y_draw_pos = (float)(Win_Y_ttf + posy) + ((float)ttf_font_datas[l].y_start * sh) / TTF_UY;
            // The y_start is relative to TTF_UY, scale it to current 'sh'

            if (bkcolor != 0) {
                DrawBox_ttf((float) (Win_X_ttf + posx), y_draw_pos, Z_ttf,
                            char_render_width, (float) sh, bkcolor);
            }
            DrawTextBox_ttf((float) (Win_X_ttf + posx), y_draw_pos, Z_ttf,
                            char_render_width, (float) sh, current_char_color,
                            (float)ttf_font_datas[l].width / 32.0f, // Use actual glyph width for texture coord
                            (float)ttf_font_datas[l].height / 32.0f); // Use actual glyph height
        }

        posx += char_render_width;
    }

    Y_ttf = (float) posy + sh; // Update current Y position after string

    if(posx > lenx) lenx = posx; // Final check for max line length
    return lenx;
}