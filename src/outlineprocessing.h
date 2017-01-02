#include <cairo.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_IMAGE_H

#ifndef OUTLINE_PROCESSING_H_
#define OUTLINE_PROCESSING_H_

  void
  process_outline( cairo_t *cr, FT_Outline *outline );


#endif /* OUTLINE_PROCESSING_H_ */

/* END */