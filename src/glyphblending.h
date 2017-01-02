#include <cairo.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#ifndef GLYPH_BLENDING_H_
#define GLYPH_BLENDING_H_

/*
 * Gamma correct linear blending
 *
 * Convert from gamma encoded color space (RGB values most people work with -
 * sRGB space) to linear (real world color instensities on screen) in order to 
 * reduce inaccuracies (artificial darkening) that happen when performing alpha
 * blending in gamma encoded space.
 */

/* The number of bits used to represent linear values when doing */
/* linear blending during gamma correction.                      */
#define GAMMA_LINEAR_BITS 12

/* The range of linear values to represent the pixel/subpixel intensity */
#define GAMMA_LINEAR_NUM_VALUES ( 1 << GAMMA_LINEAR_BITS )

/* The value that 0xFF will map to in linear space. */
#define GAMMA_LINEAR_MAX ( GAMMA_LINEAR_NUM_VALUES - 1 )


  void
  calculate_gamma_tables();

  cairo_surface_t *
  create_surface_for_ft_bitmap_dimensions( FT_Bitmap *bitmap );

  void
  blend_glyph_to_surface( FT_Bitmap        *bitmap,
                          cairo_surface_t  *surface,
                          double           red,
                          double           green,
                          double           blue,
                          int              blend_linear );


#endif /* GLYPH_BLENDING_H_ */

/* END */