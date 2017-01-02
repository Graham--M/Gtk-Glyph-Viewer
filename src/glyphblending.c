#include "glyphblending.h"
#include "glyphviewerglobals.h"
#include "utils.h"

#include FT_IMAGE_H
#include <math.h>


  void
  calculate_gamma_tables()
  {
    double gamma = globals.gamma;
    double inv_gamma = 1.0 / gamma;

    /* Conversion from gamma encoded to linear (decoded) */
    for( int i = 0; i < 256; i++ )
    {
      gdouble encoded_fraction, decoded_fraction;

      encoded_fraction = i / 255.0;
      decoded_fraction = pow( encoded_fraction, gamma );
      globals.gamma_table[i] =
          (unsigned short)round( decoded_fraction * GAMMA_LINEAR_MAX );
    }

    /* Conversion from linear to gamma encoded */
    for( int i = 0; i < GAMMA_LINEAR_NUM_VALUES; i++ )
    {
      double decoded_fraction, encoded_fraction;

      decoded_fraction = i / (double)GAMMA_LINEAR_MAX;
      encoded_fraction = pow( decoded_fraction, inv_gamma );
      globals.gamma_inv_table[i] =
          (unsigned char)round( encoded_fraction * 255 );
    }
  }


/* -------------------------------------------------------------------------- *\
 *
 *                       == Glyph blending functions ==
 * 
 * There are 4 different blend types to do that can be:
 *   - With or without linear blending
 *   - With alpha channel for each RGB component or a single alpha channel
 *
\* -------------------------------------------------------------------------- */


/*
 * _BLENDING_LOOP
 *
 * Convert the glyph coverage (alpha) map into a rasterized glyph image with
 * specified color.
 * 
 * Params:
 *   dest - the destination bitmap. Should have the same width and height
 *          as the source bitmap (excluding any padding i.e width and pitch
 *          aren't the same).
 *
 *   src - the coverage bitmap output by freetype. The bitmap should either
 *         be a greyscale or RGB bitmap.
 *
 *   r, g, b - RGB color values to draw the glyph with. If using linear
 *             blending, these should be converted to linear values.
 *
 *   func - the blending macro/function to call per pixel.
 *
 * The source FT_Bitmap is expected to have a pixel mode of either
 * FT_PIXEL_MODE_GRAY (byte per pixel) or FT_PIXEL_MODE_LCD (3 bytes per pixel)
 * while the destination cairo surface is expected to be an image surface with
 * a format of CAIRO_FORMAT_RGB24 (4 bytes per pixel 0RGB in the platform's
 * native endian order).
 */
#define _BLENDING_LOOP( dest, src, r, g, b, func )                           \
  do {                                                                       \
    unsigned int width, height, pitch, stride;                               \
    unsigned char *data;                                                     \
    unsigned char is_rgb, src_pix_bytes, _0, _1, _2;                         \
                                                                             \
    is_rgb = ( src->pixel_mode == FT_PIXEL_MODE_LCD ) ? 1 : 0;               \
                                                                             \
    if( is_rgb )                                                             \
    {                                                                        \
      width = src->width / 3;                                                \
      src_pix_bytes = 3;                                                     \
      _0 = 0;                                                                \
      _1 = 1;                                                                \
      _2 = 2;                                                                \
    }                                                                        \
    else                                                                     \
    {                                                                        \
      width = src->width;                                                    \
      src_pix_bytes = 1;                                                     \
      _0 = _1 = _2 = 0;                                                      \
    }                                                                        \
                                                                             \
    height = src->rows;                                                      \
    pitch = (unsigned int) abs( src->pitch );                                \
    stride = (unsigned int) cairo_image_surface_get_stride( dest );          \
    data = cairo_image_surface_get_data( dest );                             \
                                                                             \
    cairo_surface_flush( dest );                                             \
                                                                             \
    for( unsigned int y = 0; y < height; y++ )                               \
    {                                                                        \
      unsigned char* srow = ( (unsigned char*) src->buffer ) + y * pitch;    \
      unsigned int* drow = (unsigned int*) ( data + y * stride );            \
                                                                             \
      for( unsigned int x = 0; x < width; x++ )                              \
      {                                                                      \
        unsigned char* spixel = srow + x * src_pix_bytes;                    \
        unsigned int* dpixel = drow + x;                                     \
        func( dpixel, spixel, _0, _1, _2, r, g, b );                         \
      }                                                                      \
    }                                                                        \
                                                                             \
    cairo_surface_mark_dirty( dest );                                        \
  } while( 0 )


/*
 * _ALPHA_BLEND
 *
 * Perform an alpha blend to generate an RGB pixel value.
 *
 * The macro is agnostic to the type of blending. Both bg and fg should be
 * in the same color space.
 *
 * Params:
 *   bg - the background pixel component to blend over.
 *
 *   a - the alpha (coverage) to blend the foreground over the background with.
 *
 *   fg - the foreground pixel component to blend over.
 *
 * Returns the blended pixel component value. 
 */
#define _ALPHA_BLEND( bg, a, fg ) \
  ( fg * a + bg * ( 255 - a ) ) / 255


/*
 * Macros to deal with RGB components in a cairo surface pixel.
 */
#define _GET_RED( pixel ) \
  (unsigned char)( ( pixel >> 16 ) & 0xFF )

#define _GET_GREEN( pixel ) \
  (unsigned char)( ( pixel >> 8 ) & 0xFF )

#define _GET_BLUE( pixel ) \
  (unsigned char)( pixel & 0xff )

#define _PIXEL( r, g, b ) \
  ( ((unsigned int) r) << 16 | ((unsigned int) g) << 8 | b )


/*
 * _BLEND_SIMPLE_FUNC
 *
 * Blend the RGB color specified onto the destination pixel without doing any
 * gamma correction/conversion to linear.
 * 
 * Params:
 *   dest - pointer to the destination pixel.
 *
 *   src - pointer to the source alpha value(s).
 *
 *   _0, _1, _2 - indices to access the source alpha map RGB values. For
 *                greyscale maps these should all be set to 0.
 *
 *   r, g, b - RGB color values to draw the glyph with.
 */
#define _BLEND_SIMPLE_FUNC( dest, src, _0, _1, _2, r, g, b )     \
  do {                                                           \
    unsigned char pix_r, pix_g, pix_b;                           \
                                                                 \
    pix_r = _GET_RED( dest[0] );                                 \
    pix_g = _GET_GREEN( dest[0] );                               \
    pix_b = _GET_BLUE( dest[0] );                                \
                                                                 \
    pix_r = _ALPHA_BLEND( pix_r, src[ _0 ], r );                 \
    pix_g = _ALPHA_BLEND( pix_g, src[ _1 ], g );                 \
    pix_b = _ALPHA_BLEND( pix_b, src[ _2 ], b );                 \
                                                                 \
    dest[0] = _PIXEL( pix_r, pix_g, pix_b );                     \
  } while( 0 )


/*
 * _BLEND_LINEAR_FUNC
 *
 * Blend the RGB color specified onto the destination pixel in linear space.
 * 
 * Params:
 *   dest - pointer to the destination pixel.
 *
 *   src - pointer to the source alpha value(s).
 *
 *   _0, _1, _2 - indices to access the source alpha map RGB values. For
 *                greyscale maps these should all be set to 0.
 *
 *   r, g, b - RGB color values to draw the glyph with. These should specify
 *             a color in linear space.
 */
#define _BLEND_LINEAR_FUNC( dest, src, _0, _1, _2, r, g, b )      \
  do {                                                            \
    unsigned char pix_r, pix_g, pix_b;                            \
    unsigned int lin_r, lin_g, lin_b;                             \
                                                                  \
    pix_r = _GET_RED( dest[0] );                                  \
    pix_g = _GET_GREEN( dest[0] );                                \
    pix_b = _GET_BLUE( dest[0] );                                 \
                                                                  \
    lin_r = globals.gamma_table[pix_r];                           \
    lin_g = globals.gamma_table[pix_g];                           \
    lin_b = globals.gamma_table[pix_b];                           \
                                                                  \
    lin_r = _ALPHA_BLEND( lin_r, src[ _0 ], r );                  \
    lin_g = _ALPHA_BLEND( lin_g, src[ _1 ], g );                  \
    lin_b = _ALPHA_BLEND( lin_b, src[ _2 ], b );                  \
                                                                  \
    pix_r = globals.gamma_inv_table[(unsigned short) lin_r];      \
    pix_g = globals.gamma_inv_table[(unsigned short) lin_g];      \
    pix_b = globals.gamma_inv_table[(unsigned short) lin_b];      \
                                                                  \
    dest[0] = _PIXEL( pix_r, pix_g, pix_b );                      \
  } while( 0 )


/*
 * Blending function to use. They point to the real function/macro because they
 * are expanded in the macro arguments unless you concatinate them like
 * func ## _FUNC which gives the same result anyway. This way, tools like CTags
 * can pick up the macro names easier.
 */
#define _BLEND_LINEAR _BLEND_LINEAR_FUNC
#define _BLEND_SIMPLE _BLEND_SIMPLE_FUNC


  static void
  _simple_blend( cairo_surface_t  *dest_bitmap,
                 FT_Bitmap        *src_bitmap,
                 unsigned char     red,
                 unsigned char     green,
                 unsigned char     blue )
  {
    _BLENDING_LOOP( dest_bitmap, src_bitmap, red, green, blue, _BLEND_SIMPLE );
  }


  static void
  _linear_blend( cairo_surface_t  *dest_bitmap,
                 FT_Bitmap        *src_bitmap,
                 unsigned char     red,
                 unsigned char     green,
                 unsigned char     blue )
  {
    unsigned short c_r, c_g, c_b;

    c_r = globals.gamma_table[red];
    c_g = globals.gamma_table[green];
    c_b = globals.gamma_table[blue];

    _BLENDING_LOOP( dest_bitmap, src_bitmap, c_r, c_g, c_b, _BLEND_LINEAR );
  }


  cairo_surface_t *
  create_surface_for_ft_bitmap_dimensions( FT_Bitmap *bitmap )
  {
    int width = ( bitmap->pixel_mode == FT_PIXEL_MODE_LCD )
                ? bitmap->width / 3 : bitmap->width;

    return cairo_image_surface_create( CAIRO_FORMAT_RGB24,
                                       width,
                                       bitmap->rows);
  }


  void
  blend_glyph_to_surface( FT_Bitmap        *bitmap,
                          cairo_surface_t  *surface,
                          double           red,
                          double           green,
                          double           blue,
                          int              blend_linear )
  {
    unsigned char r, g, b;
    int is_rgb = 0;

    r = (unsigned char)( red   * 255 );
    g = (unsigned char)( green * 255 );
    b = (unsigned char)( blue  * 255 );

    if( bitmap->pixel_mode != FT_PIXEL_MODE_GRAY &&
        bitmap->pixel_mode != FT_PIXEL_MODE_LCD )
      panic("blend_glyph_to_surface: pixel mode is %d", bitmap->pixel_mode );

    else if( cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_IMAGE )
      panic("blend_glyph_to_surface: surface type is %d", bitmap->pixel_mode );

    if( blend_linear )
      _linear_blend( surface, bitmap, r, g, b );
    else
      _simple_blend( surface, bitmap, r, g, b );
  }


/* END */
