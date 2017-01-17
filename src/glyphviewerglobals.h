#include "glyphblending.h"

#include <gtk/gtk.h>
#include <glib.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdarg.h>


#ifndef GLYPH_VIEWER_GLOBALS_H_
#define GLYPH_VIEWER_GLOBALS_H_


  typedef struct ViewerColorRec_
  {
    double red;
    double green;
    double blue;
    double alpha;
  } ViewerColor;


  typedef enum
  {
    HINTING_MODE_NONE,
    HINTING_MODE_LIGHT,
    HINTING_MODE_NORMAL
  } HintingMode;


  typedef enum
  {
    /* Regular mode; draw outline, bitmap, points ect */
    VIEWER_MODE_NORMAL,
    /* In addition, draw detected edges */
    VIEWER_MODE_EDGES,
    /* Scale glyph ourselves with no pixel rounding for subglyph positions */
    VIEWER_MODE_UNIFORM_SCALING,
    /* Use custom Hinting instead of the built-in modes */
    VIEWER_MODE_CUSTOM_HINTING
  } ViewerMode;


  typedef struct GlyphViewerGlobalsRec_
  {
    /* Construct GTK Widgets from definition string */
    GtkBuilder        *builder;

    /* The main window GTK widget */
    GtkWidget         *window;

    /* Keyboard accelerators/shortcuts for menu */
    GtkAccelGroup     *accels;

    /* The area reserved for the bitmaps to be drawn on */
    GtkWidget         *drawing_area;

    /* Main Menu */
    GtkWidget         *menu_bar;

    /* Freetype Library Instance */
    FT_Library         library;

    /* Current font face loaded */
    FT_Face            face;

    /* The currently set text size (in half points 9pt = 18) */
    unsigned int       text_size;

    /* The display pixel density (dots per inch) */
    unsigned int       resolution;

    /* The index of the glyph in the font file to draw */
    FT_UInt            glyph_index;

    /* What to draw */
    ViewerMode         viewer_mode;

    /* The type of hinting to apply to the glyph */
    HintingMode        hinting_mode;

    /* Should pass the force autohint flag */
    gboolean           force_autohint;

    /* Should use subpixel rendering (also use lcd mode for normal hinting) */
    gboolean           lcd_rendering;

    /* Draw each subpixel as a greyscale trio instead of a RGB pixel */
    gboolean           show_subpixel_mask;

    /* Should use linear blending/gamma correction */
    gboolean           linear_blending;

    /* The currently set gamma correction factor when doing linear blending */
    double             gamma;

    /* Table to convert from the normal RGB colorspace to linear */
    unsigned short     gamma_table[256];

    /* Table to convert from linear back to the normal RGB colorspace */
    unsigned char      gamma_inv_table[GAMMA_LINEAR_NUM_VALUES];

    /* Blended glyph bitmap so it doesn't need rasterized each time */
    cairo_surface_t   *glyph_surface;

    /* Scale factor to inflate the glyph outline and bitmap by */
    FT_F26Dot6         scale;

    /* Position of the glyph origin in the drawing area */
    int                x_origin;
    int                y_origin;

    /* Initial Scale and glyph origin position */
    FT_F26Dot6         scale_0;
    int                x_origin_0;
    int                y_origin_0;

    /* Should grid be drawn */
    /* Bit 0 - user pref, 1 - has forced setting, 2 - forced setting  */
    char               draw_grid;

    /* Should outline and points be drawn - same flags as above */
    char               draw_outline;

    GString           *status_text;

    /* Glyph bitmap color (alpha ignored) */
    ViewerColor        text_color;
    /* Window background color (alpha ignored) */
    ViewerColor        bg_color;
    /* Standard Grid Line Color */
    ViewerColor        grid_color;
    /* Axis Line Color */
    ViewerColor        grid_origin_color;
    /* Glyph outline color */
    ViewerColor        outline_color;
    /* Regular outline point color */
    ViewerColor        on_point_color;
    /* Outline bezier control point color */
    ViewerColor        ctrl_point_color;
    /* Outline single edge color */
    ViewerColor        edge_unlinked_color;
    /* Outline linked edge pair color */
    ViewerColor        edge_linked_color;
    /* Outline additional (serif) linked edge color */
    ViewerColor        edge_serif_color;
  } GlyphViewerGlobals;


  GlyphViewerGlobals globals;


  void
  switch_font( FT_Face face );

  void
  set_face_size();

  void
  setup_glyph();

  void
  invalidate_drawing_area();

  GtkWidget *
  get_builder_widget( const gchar *name );

  void
  set_status( char *format, ... );


#endif /* GLYPH_VIEWER_GLOBALS_H_ */

/* END */