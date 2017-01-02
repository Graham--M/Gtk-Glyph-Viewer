#include "glyphviewerglobals.h"
#include "utils.h"
#include "outlineprocessing.h"
#include "controls.h"
#include "interface.glade.h"

#include <math.h> /* for M_PI */


  static void
  _calculate_initial_scale();


  void
  switch_font( FT_Face face )
  {
    if( globals.face )
    {
      FT_Done_Face( globals.face );
      globals.face = 0;
    }

    globals.face = face;
    globals.glyph_index = 0;

    set_face_size();
    setup_glyph();
  }


  void
  set_face_size()
  {
    if(FT_Set_Char_Size( globals.face,
                         globals.text_size * 64 / 2,
                         globals.text_size * 64 / 2,
                         globals.resolution,
                         globals.resolution ) )
      panic( "Couldn't set font size on face" );

    _calculate_initial_scale();
  }


  static void
  _calculate_initial_scale()
  {
    FT_F26Dot6 x_scale, y_scale;
    GtkAllocation alloc;
    int x_origin, y_origin;

    FT_F26Dot6 scale, old_scale = globals.scale;

    FT_Face face = globals.face;
    int margin = 10; /* in px from the edges */

    /*
     * Get the maximum extents that the face's glyphs reach. It's in font units
     * so it needs scaled to correct size for the text.
     */
    int xmin = FT_MulFix( face->bbox.xMin, face->size->metrics.x_scale );
    int ymin = FT_MulFix( face->bbox.yMin, face->size->metrics.y_scale );
    int xmax = FT_MulFix( face->bbox.xMax, face->size->metrics.x_scale );
    int ymax = FT_MulFix( face->bbox.yMax, face->size->metrics.y_scale );

    /* Count fractional pixels as a whole pixel. */
    xmin &= ~63;
    ymin &= ~63;
    xmax  = ( xmax + 63 ) & ~63;
    ymax  = ( ymax + 63 ) & ~63;

    /* Get the size of the area to draw into */
    gtk_widget_get_allocation (globals.drawing_area, &alloc);

    /* Calculate needed scale to fit the area. */
    /* Minimum size of 1px. */
    if ( xmax - xmin )
        x_scale = ( alloc.width - 2 * margin ) * 64 / ( xmax - xmin );
    else
      x_scale = 64;

    if ( ymax - ymin )
      y_scale = ( alloc.height - 2 * margin ) * 64 / ( ymax - ymin );
    else
      y_scale = 64;

    scale = ( x_scale <= y_scale ) ? x_scale : y_scale;

    /*
     * Drawing area coordinate system starts from the top left (unlike the
     * glyph's coords which start in the bottom left). Position the glyph
     * origin so that the bounding box minimums are in the bottom left corner
     * (centering it is a little strange).
     */
    x_origin = margin * 64 - xmin * scale;
    y_origin = ( alloc.height - margin ) * 64 + ymin * scale;

    /* Keep in pixel units */
    x_origin >>= 6;
    y_origin >>= 6;

    /* Don't overwrite if scale already set */
    if( old_scale == 0 )
    {
      globals.x_origin = x_origin;
      globals.y_origin = y_origin;
      globals.scale = scale;
    }

    /* Keep the initial values in case the view needs reset. */
    globals.scale_0 = scale;
    globals.x_origin_0 = x_origin;
    globals.y_origin_0 = y_origin;
  }


  static void
  _clear_background( cairo_t *cr )
  {
    ViewerColor bg = (ViewerColor){1, 1, 1};
    if( !globals.show_subpixel_mask )
      bg = globals.bg_color;

    cairo_set_source_rgb( cr, bg.red, bg.green, bg.blue );
    cairo_paint( cr );
  }


  static void
  _draw_glyph_bitmap( cairo_t *cr )
  {
    cairo_pattern_t *pattern;

    int x_offset = globals.x_origin + globals.face->glyph->bitmap_left * globals.scale;
    int y_offset = globals.y_origin - globals.face->glyph->bitmap_top * globals.scale;

    /* Transformations need to be set so they can be applied to the source. */
    cairo_translate( cr, x_offset, y_offset );
    cairo_scale( cr, globals.scale, globals.scale );

    /* Use a pattern for the source so the scaling method can be set. */
    pattern = cairo_pattern_create_for_surface( globals.glyph_surface );
    cairo_pattern_set_filter( pattern, CAIRO_FILTER_NEAREST );

    cairo_set_source( cr, pattern );
    cairo_paint( cr );

    cairo_pattern_destroy( pattern );
  }


  /*
   * Draw the individual subpixels of the glyph image so we can see their
   * intensities instead of a colored pixel.
   * This assumes a black on white glyph_surface or the background won't
   * match the window background.
   */
  static void
  _draw_glyph_subpixel_mask( cairo_t *cr )
  {
    cairo_surface_t *surface;
    cairo_pattern_t *pattern;

    int src_width = cairo_image_surface_get_width( globals.glyph_surface );
    int src_height = cairo_image_surface_get_height( globals.glyph_surface );
    int src_stride = cairo_image_surface_get_stride( globals.glyph_surface );

    unsigned char *src_data =
        cairo_image_surface_get_data( globals.glyph_surface );

    surface = cairo_image_surface_create( CAIRO_FORMAT_RGB24, src_width * 3,
                                                              src_height );

    unsigned char *dst_data = cairo_image_surface_get_data( surface );

    int dst_stride = cairo_image_surface_get_stride( surface );

    /* Probably unnecessary but just to be safe */
    cairo_surface_flush( surface );

    for( int row = 0; row < src_height; row++ )
    {
      /* Stride is the row size in BYTES */
      unsigned char *src_row = src_data + row * src_stride;
      unsigned char *dst_row = dst_data + row * dst_stride;

      for( int px = 0; px < src_width; px++ )
      {
        unsigned int *src_px = (unsigned int*) src_row + px;
        unsigned int *dst_p  = (unsigned int*) dst_row + px * 3;

        unsigned int r = *src_px >> 16 & 0xFF;
        unsigned int g = *src_px >>  8 & 0xFF;
        unsigned int b = *src_px >>  0 & 0xFF;

        dst_p[0] = r << 16 | r << 8 | r;
        dst_p[1] = g << 16 | g << 8 | g;
        dst_p[2] = b << 16 | b << 8 | b;
      }
    }

    cairo_surface_mark_dirty( surface );

    /* This almost the same as _draw_glyph_bitmap() at this point */

    int x_offset = globals.x_origin + globals.face->glyph->bitmap_left * globals.scale;
    int y_offset = globals.y_origin - globals.face->glyph->bitmap_top * globals.scale;

    cairo_translate( cr, x_offset, y_offset );
    cairo_scale( cr, globals.scale / 3.0, globals.scale );

    /* Use a pattern for the source so the scaling method can be set. */
    pattern = cairo_pattern_create_for_surface( surface );
    cairo_pattern_set_filter( pattern, CAIRO_FILTER_NEAREST );

    cairo_set_source( cr, pattern );
    cairo_paint( cr );

    cairo_pattern_destroy( pattern );
    cairo_surface_destroy( surface );
  }


  static void
  _draw_grid_lines( cairo_t *cr )
  {
    GtkAllocation alloc;
    double x_origin, y_origin, scale;

    ViewerColor c = globals.grid_color;

    /* Subtract -0.5 so the line doesn't straddle the edge of two pixels */
    x_origin = globals.x_origin - 0.5;
    y_origin = globals.y_origin - 0.5;

    scale    = globals.scale;

    /* Get the size of the area to draw into */
    gtk_widget_get_allocation( globals.drawing_area, &alloc );

    cairo_set_source_rgba( cr, c.red, c.green, c.blue, 0.3 );
    cairo_set_line_width( cr, 1 );

    for( double x = x_origin - scale; x > 0; x -= scale )
    {
      cairo_move_to( cr, x, 0 );
      cairo_line_to( cr, x, alloc.height );
    }

    for( double x = x_origin + scale; x < alloc.width; x += scale )
    {
      cairo_move_to( cr, x, 0 );
      cairo_line_to( cr, x, alloc.height );
    }

    for( double y = y_origin - scale; y > 0; y -= scale )
    {
      cairo_move_to( cr, 0, y );
      cairo_line_to( cr, alloc.width, y );
    }

    for( double y = y_origin + scale; y < alloc.height; y += scale)
    {
      cairo_move_to( cr, 0, y );
      cairo_line_to( cr, alloc.width, y );
    }

    cairo_stroke( cr );

    /* Origin lines are a seperate color. */
    cairo_set_source_rgba( cr, c.red, c.green, c.blue, 0.8 );
    cairo_move_to( cr, x_origin, 0 );
    cairo_line_to( cr, x_origin, alloc.height );
    cairo_move_to( cr, 0, y_origin );
    cairo_line_to( cr, alloc.width, y_origin );

    cairo_stroke( cr );
  }


  static void
  _draw_outline( cairo_t *cr )
  {
    ViewerColor c = globals.outline_color;
    cairo_set_source_rgb( cr, c.red, c.green, c.blue );
    cairo_set_line_width( cr, 1 );

    cairo_translate( cr, globals.x_origin, globals.y_origin );
    cairo_scale( cr, globals.scale, -globals.scale );

    cairo_new_path( cr );
    process_outline( cr, &globals.face->glyph->outline );
    cairo_close_path( cr );

    /* Reset transformation matrix so the stroke width won't be scaled. */
    cairo_identity_matrix( cr );

    cairo_stroke( cr );
  }


  static void
  _draw_points( cairo_t *cr )
  {
    FT_Outline *outline = &globals.face->glyph->outline;

    ViewerColor c_on = globals.on_point_color;
    ViewerColor c_ctl = globals.ctrl_point_color;

    for( int i = 0; i < outline->n_points; i++ )
    {
      if( outline->tags[i] & 1 ) /* Point on curve */
        cairo_set_source_rgb( cr, c_on.red, c_on.green, c_on.blue );

      else /* Control point */
        cairo_set_source_rgb( cr, c_ctl.red, c_ctl.green, c_ctl.blue );

      FT_Vector *p = outline->points + i;
      cairo_arc( cr, globals.x_origin + p->x * globals.scale / 64.0,
                     globals.y_origin - p->y * globals.scale / 64.0,
                     2, 0, 2 * M_PI );
      cairo_fill( cr );
    }
  }


  static gboolean
  _test_setting_flags( char *setting )
  {
    /* Use the user's preference (bit 0) if no override is set (bit 1), */
    /* Otherwise if an override is set, use the forced setting (bit 2) */
    return ( ( *setting & 0x03 ) == 0x01 ||
             ( *setting & 0x06 ) == 0x06 )  ? TRUE : FALSE;
  }


  static gboolean
  _on_expose_event( GtkWidget *widget,
                   GdkEventExpose *event,
                   gpointer data )
  {
    cairo_t *cr;

    cr = gdk_cairo_create( widget->window );

    RESTORE_AFTER( cr, _clear_background( cr ) );

    if( globals.face )
    {
      if( !globals.show_subpixel_mask )
        RESTORE_AFTER( cr, _draw_glyph_bitmap( cr ) );
      else
        RESTORE_AFTER( cr, _draw_glyph_subpixel_mask( cr ) );

      if( _test_setting_flags( &globals.draw_grid ) )
        RESTORE_AFTER( cr, _draw_grid_lines( cr ) );

      if( _test_setting_flags( &globals.draw_outline ) )
      {
        RESTORE_AFTER( cr, _draw_outline( cr ) );
        RESTORE_AFTER( cr, _draw_points( cr ) );
      }
    }

    cairo_destroy( cr );

    return FALSE;
  }


  void
  invalidate_drawing_area()
  {
    gtk_widget_queue_draw( globals.drawing_area );
  }


  GtkWidget *
  get_builder_widget( const gchar *name )
  {
    return (GtkWidget*) gtk_builder_get_object( globals.builder, name);
  }


  static void
  _setup_window()
  {
    globals.builder = gtk_builder_new();
    gtk_builder_add_from_string( globals.builder, GLADE_STRING, -1, NULL );

    globals.window        = get_builder_widget( "window" );
    globals.drawing_area  = get_builder_widget( "drawing_area" );
    globals.menu_bar      = get_builder_widget( "menu_bar" );


    g_signal_connect( G_OBJECT( globals.drawing_area ), "expose-event",
                      G_CALLBACK( _on_expose_event ), NULL );
    g_signal_connect( globals.window, "destroy",
                      G_CALLBACK( gtk_main_quit ), NULL );


    gtk_widget_set_size_request( globals.window, 600, 450 );

    globals.accels = gtk_accel_group_new();
    gtk_window_add_accel_group( GTK_WINDOW( globals.window ), globals.accels );
  }


  void
  setup_glyph()
  {
    if( globals.glyph_surface )
    {
      cairo_surface_destroy( globals.glyph_surface );
      globals.glyph_surface = 0;
    }


    FT_Int32 load_flags = FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP;

    if( globals.hinting_mode == HINTING_MODE_NONE )
      load_flags |= FT_LOAD_NO_HINTING;

    else if( globals.hinting_mode == HINTING_MODE_LIGHT )
      load_flags |= FT_LOAD_TARGET_LIGHT;

    else if( globals.hinting_mode == HINTING_MODE_NORMAL )
      load_flags |= globals.lcd_rendering ? FT_LOAD_TARGET_LCD 
                                          : FT_LOAD_TARGET_NORMAL;

    if( globals.hinting_mode != HINTING_MODE_NONE && globals.force_autohint )
      load_flags |= FT_LOAD_FORCE_AUTOHINT;


    if( FT_Load_Glyph( globals.face, globals.glyph_index, load_flags ) )
      panic( "Couldn't load glyph index: %d", globals.glyph_index );

    if (globals.face->glyph->format != FT_GLYPH_FORMAT_OUTLINE)
      panic( "Glyph format isn't an outline, format %d",
             globals.face->glyph->format );


    FT_Render_Mode render_mode = globals.lcd_rendering ? FT_RENDER_MODE_LCD
                                                       : FT_RENDER_MODE_NORMAL;

    if( FT_Render_Glyph( globals.face->glyph, render_mode ) )
      panic( "Couldn't render glyph" );

    globals.glyph_surface =
        create_surface_for_ft_bitmap_dimensions( 
            &globals.face->glyph->bitmap );

    cairo_t *cr = cairo_create( globals.glyph_surface );

    ViewerColor bg = (ViewerColor){1, 1, 1};
    if( !globals.show_subpixel_mask )
      bg = globals.bg_color;

    cairo_set_source_rgb( cr, bg.red, bg.green, bg.blue );
    cairo_paint( cr );
    cairo_destroy( cr );

    ViewerColor fg = (ViewerColor){0, 0, 0};
    if( !globals.show_subpixel_mask )
      fg = globals.text_color;

    blend_glyph_to_surface( &globals.face->glyph->bitmap,
                            globals.glyph_surface,
                            fg.red, fg.green, fg.blue,
                            globals.linear_blending );

    invalidate_drawing_area();
  }


  int
  main( int argc, char *argv[] )
  {
    gtk_init( &argc, &argv );
    
    if( FT_Init_FreeType( &globals.library ) )
      panic( "Couldn't initalize Freetype" );

    /* Initialise defaults */
    {
      globals.face               = 0;
      globals.text_size          = 18;
      globals.resolution         = 96;
      globals.hinting_mode       = 0;
      globals.force_autohint     = FALSE;
      globals.lcd_rendering      = FALSE;
      globals.linear_blending    = FALSE;
      globals.show_subpixel_mask = FALSE;
      globals.gamma              = 1.8;
      globals.glyph_surface      = 0;
      globals.scale              = 0;
      globals.draw_grid          = 1;
      globals.draw_outline       = 1;
      globals.text_color         = (ViewerColor){0, 0, 0};
      globals.bg_color           = (ViewerColor){1, 1, 1};
      globals.grid_color         = (ViewerColor){0, 0, 0};
      globals.outline_color      = (ViewerColor){1, 0, 0};
      globals.on_point_color     = globals.outline_color;
      globals.ctrl_point_color   = (ViewerColor){0, 0.7, 0};

      calculate_gamma_tables();
    }

    _setup_window();
    init_controls();
    gtk_widget_show_all( globals.window );

    gtk_main();

    return 0;
  }


/* END */
