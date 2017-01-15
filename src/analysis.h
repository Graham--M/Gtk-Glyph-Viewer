#include <cairo.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#ifndef ANALYSIS_H_
#define ANALYSIS_H_

  void
  analysis_init();

  void
  analysis_detect_face_edges();

  void
  analysis_draw_face_edges( cairo_t *cr );

  void
  analysis_load_hinted();


#endif /* ANALYSIS_H_ */

/* END */