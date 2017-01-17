#include "glyphviewerglobals.h"
#include "controls.h"
#include "dialog_gotoindex.h"
#include "dialog_gotochar.h"
#include "dialog_selectface.h"
#include "dialog_changecolors.h"
#include "utils.h"
#include "analysis.h"

#include <gdk/gdkkeysyms.h> /* Not included by GDK */
#include FT_LCD_FILTER_H


  static struct MenuWidgets
  {
    GtkWidget *font_open_item;
    GtkWidget *font_size_inc;
    GtkWidget *font_size_dec;
    GtkWidget *font_hinting_none;
    GtkWidget *font_hinting_light;
    GtkWidget *font_hinting_normal;
    GtkWidget *font_force_autohint;
    GtkWidget *glyph_index_inc;
    GtkWidget *glyph_index_dec;

    GtkWidget *gamma_correct;
    GtkWidget *gamma_inc;
    GtkWidget *gamma_dec;
    GtkWidget *show_grid;
    GtkWidget *show_outline;
    GtkWidget *lcd_filter_submenu_entry;
    GtkWidget *lcd_filter_none;
    GtkWidget *lcd_filter_light;
    GtkWidget *lcd_filter_normal;
    GtkWidget *change_colors;

    GtkWidget *zoom_inc;
    GtkWidget *zoom_dec;
    GtkWidget *view_reset;
    GtkWidget *view_subpixel;
    GtkWidget *show_subpixel_mask;

    GtkWidget *goto_glyph_index;
    GtkWidget *goto_char;

    GtkWidget *mode_normal;
    GtkWidget *mode_edges;
    GtkWidget *mode_uniform_scaling;
    GtkWidget *mode_custom_hinting;
  } _menu_widgets;


  static struct ControlStatus
  {
    gboolean   mouse_grabbed;
    gdouble    grab_x;
    gdouble    grab_y;
    int        start_x;
    int        start_y;
  } _control_status;


  static void
  _menu_font_size_set_enabled( gboolean enabled );

  static void
  _menu_glyph_index_set_enabled( gboolean enabled );

  static void
  _menu_gamma_change_set_enabled( gboolean enabled );

  static void
  _menu_view_controls_enabled( gboolean enabled );

  static void
  _menu_goto_glyph_index_enabled( gboolean enabled );

  static void
  _menu_goto_char_enabled( gboolean enabled );


  /* -------------------------------------------------------------------------- *\
   *
   *                             == Helpers ==
   *
  \* -------------------------------------------------------------------------- */

  static void
  _toggle_user_flag( char *setting )
  {
    if( *setting & 0x01 )
      *setting &= ~0x01;
    else
      *setting |= 0x01;
  }


  static int
  _get_user_flag( char setting )
  {
    return setting & 0x01;
  }


  static void
  _force_flag( char *setting, char flag_set )
  {
    *setting |= 0x02;

    if( flag_set )
      *setting |= 0x04;
    else
      *setting &= ~0x04;
  }

  static void
  _release_flag( char *setting )
  {
    *setting &= ~0x06;
  }


  /* -------------------------------------------------------------------------- *\
   *
   *                             == Handlers ==
   *
  \* -------------------------------------------------------------------------- */

  /*
   * Open font handler
   */

  static gboolean
  _do_open_index( char *filename, FT_Long num_faces, FT_Face *face )
  {
    GArray *faces;
    gint index;

    faces = g_array_sized_new( FALSE, FALSE, sizeof( FT_Face ), num_faces );

    for( int i = 0; i < num_faces; i++ )
    {
      FT_Face _f;

      if( FT_New_Face( globals.library, filename, i, &_f ) )
        panic( "Couldn't load face index: %d, of %s", i, filename );

      g_array_append_val( faces, _f );
    }

    if( select_face_dialog_run( faces ) != GTK_RESPONSE_OK )
      return TRUE;

    index = select_face_dialog_get_index();

    if( index == -1 )
      panic( "_do_open_index: No active index was set" );

    for( int i = 0; i < num_faces; i++ )
    {
      FT_Face *_f = &g_array_index( faces, FT_Face, i );

      if( i != index )
        FT_Done_Face( *_f );
      else
        *face = *_f;
    }

    g_array_free( faces, TRUE );
    return FALSE;
  }

  static void
  _do_open_font()
  {
    GtkWidget *chooser = gtk_file_chooser_dialog_new(
                             "Open",
                             GTK_WINDOW( globals.window ),
                             GTK_FILE_CHOOSER_ACTION_OPEN,
                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                             GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                             NULL );

    if( gtk_dialog_run( GTK_DIALOG( chooser ) ) == GTK_RESPONSE_ACCEPT )
    {
      char *filename;
      GtkWidget *message_box;
      FT_Error error;
      FT_Face face;

      gboolean cancelled = FALSE;

      filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( chooser ) );

      error = FT_New_Face( globals.library, filename, 0, &face );

      if( error == 0 && face->num_faces > 1 )
      {
        FT_Long num_faces = face->num_faces;

        FT_Done_Face( face );
        cancelled = _do_open_index( filename, num_faces, &face );
      }

      if( error == 0 && !cancelled )
      {
        switch_font( face );
        _menu_font_size_set_enabled( TRUE );
        _menu_glyph_index_set_enabled( TRUE );
        _menu_goto_glyph_index_enabled( TRUE );
        _menu_view_controls_enabled( TRUE );

        error = FT_Select_Charmap( globals.face, FT_ENCODING_UNICODE );
        if( error )
        {
          char *msg = "No unicode charmap found in the font.";
          message_box = gtk_message_dialog_new ( GTK_WINDOW( globals.window ),
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       msg );

          gtk_dialog_run( GTK_DIALOG( message_box ) );
          gtk_widget_destroy( message_box );
        }
        else
        {
          _menu_goto_char_enabled( TRUE );
        }

        set_status( "Loaded font" );

        g_free( filename );
      }
      else if( !cancelled )
      {
        message_box = gtk_message_dialog_new ( GTK_WINDOW( globals.window ),
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       "Error Opening File." );

        gtk_dialog_run( GTK_DIALOG( message_box ) );
        gtk_widget_destroy( message_box );
      }
    }

    gtk_widget_destroy( chooser );
  }

  static void
  _menu_open_font( GtkMenuItem *menuitem, gpointer user_data )
  {
    _do_open_font();
  }


  /*************************************************************************/

  /*
   * Font size handlers
   */

  static void
  _menu_font_size_change( GtkMenuItem *menuitem, gpointer user_data )
  {
    struct MenuWidgets *mw = &_menu_widgets;
    int size = ( (int)globals.text_size );
    
    if( ((void*)menuitem) == ((void*)(mw->font_size_inc)) )
      size += 1;

    else if( ((void*)menuitem) == ((void*)(mw->font_size_dec)) )
      size -= 1;

    else
      return;

    if( size < 2 || size > 100) return;

    globals.text_size = (unsigned int)size;

    set_face_size();
    set_status( NULL );
    analysis_detect_face_edges();
    setup_glyph();
  }

  static void
  _menu_font_size_set_enabled( gboolean enabled )
  {
    gtk_widget_set_sensitive( _menu_widgets.font_size_inc, enabled );
    gtk_widget_set_sensitive( _menu_widgets.font_size_dec, enabled );
  }


  /*************************************************************************/

  /*
   * Font hinting handlers
   */

  static void
  _menu_font_hinting_mode( GtkMenuItem *menuitem, gpointer user_data )
  {
    struct MenuWidgets *mw = &_menu_widgets;
    HintingMode mode;
    
    if( ((void*)menuitem) == ((void*)(mw->font_hinting_none)) )
    {
      mode = HINTING_MODE_NONE;
      set_status( "Hinting disabled" );
    }

    else if( ((void*)menuitem) == ((void*)(mw->font_hinting_light)) )
    {
      mode = HINTING_MODE_LIGHT;
      set_status( "Hinting set to light" );
    }

    else if( ((void*)menuitem) == ((void*)(mw->font_hinting_normal)) )
    {
      mode = HINTING_MODE_NORMAL;
      set_status( "Hinting set to normal" );
    }

    else
      return;

    globals.hinting_mode = mode;

    if( globals.face )
      setup_glyph();
  }

  static void
  _menu_font_force_autohint( GtkMenuItem *menuitem, gpointer user_data )
  {
    globals.force_autohint = globals.force_autohint ? FALSE : TRUE;

    set_status( "Autohinter [%s]", globals.force_autohint ? "on" : "off" );

    if( globals.face )
      setup_glyph();
  }

  static void
  _menu_font_hinting_set_enabled( gboolean enabled )
  {
    gtk_widget_set_sensitive( _menu_widgets.font_hinting_none, enabled );
    gtk_widget_set_sensitive( _menu_widgets.font_hinting_light, enabled );
    gtk_widget_set_sensitive( _menu_widgets.font_hinting_normal, enabled );
    gtk_widget_set_sensitive( _menu_widgets.font_force_autohint, enabled );
  }


  /*************************************************************************/

  /*
   * Glyph index handlers
   */

  static void
  _menu_glyph_index_change( GtkMenuItem *menuitem, gpointer user_data )
  {
    struct MenuWidgets *mw = &_menu_widgets;
    int index = globals.glyph_index;
    
    if( ((void*)menuitem) == ((void*)(mw->glyph_index_inc)) )
      index += 1;

    else if( ((void*)menuitem) == ((void*)(mw->glyph_index_dec)) )
      index -= 1;

    else
      return;

    if( index < 0 || index >= globals.face->num_glyphs)
      return;

    globals.glyph_index = index;
    analysis_detect_face_edges();
    setup_glyph();
  }

  static void
  _menu_glyph_index_set_enabled( gboolean enabled )
  {
    gtk_widget_set_sensitive( _menu_widgets.glyph_index_inc, enabled );
    gtk_widget_set_sensitive( _menu_widgets.glyph_index_dec, enabled );
  }


  /*************************************************************************/

  /*
   * Gamma Correction handlers
   */

  static void
  _menu_gamma_toggle( GtkMenuItem *menuitem, gpointer user_data )
  {
    if( globals.linear_blending )
    {
      globals.linear_blending = FALSE;
      _menu_gamma_change_set_enabled( FALSE );
      set_status( "Gamma correct blending disabled" );
    }
    else
    {
      globals.linear_blending = TRUE;
      _menu_gamma_change_set_enabled( TRUE );
      set_status( "Gamma correct blending enabled" );
    }

    if( globals.face )
      setup_glyph();
  }

  static void
  _menu_gamma_change( GtkMenuItem *menuitem, gpointer user_data )
  {
    double gamma = globals.gamma;
    if( ((void*)menuitem) == ((void*)(_menu_widgets.gamma_inc)) )
      gamma += 0.1;
    else if( ((void*)menuitem) == ((void*)(_menu_widgets.gamma_dec)) )
      gamma -= 0.1;
    else
      return;

    if( gamma < 0.1 || gamma > 3 )
      return;

    globals.gamma = gamma;
    calculate_gamma_tables();

    set_status( "Gamma set to %.1f", globals.gamma );

    if( globals.face )
      setup_glyph();
  }

  static void
  _menu_gamma_change_set_enabled( gboolean enabled )
  {
    gtk_widget_set_sensitive( _menu_widgets.gamma_inc, enabled );
    gtk_widget_set_sensitive( _menu_widgets.gamma_dec, enabled );
  }


  /*************************************************************************/

  /*
   * Display Option handlers
   */

  static void
  _menu_toggle_grid( GtkMenuItem *menuitem, gpointer user_data )
  {
    _toggle_user_flag( &globals.draw_grid );

    set_status( "Draw grid [%s]",
                _get_user_flag( globals.draw_grid ) ? "on" : "off" );

    if( globals.face )
      invalidate_drawing_area();
  }

  static void
  _menu_toggle_grid_enabled( gboolean enabled )
  {
    gtk_widget_set_sensitive( _menu_widgets.show_grid, enabled );
  }


  static void
  _menu_toggle_outline( GtkMenuItem *menuitem, gpointer user_data )
  {
    _toggle_user_flag( &globals.draw_outline );

    set_status( "Draw outline [%s]",
                _get_user_flag( globals.draw_outline ) ? "on" : "off" );

    if( globals.face )
      invalidate_drawing_area();
  }

  static void
  _menu_toggle_outline_enabled( gboolean enabled )
  {
    gtk_widget_set_sensitive( _menu_widgets.show_outline, enabled );
  }


  /*************************************************************************/

  /*
   * LCD Filter handlers
   */

  static void
  _menu_lcd_filter( GtkMenuItem *menuitem, gpointer user_data )
  {
    FT_LcdFilter filter;
    struct MenuWidgets *mw = &_menu_widgets;

    if( ((void*)menuitem) == ((void*)(mw->lcd_filter_none)) )
    {
      filter = FT_LCD_FILTER_NONE;
      set_status( "LCD filter disabled" );
    }
    else if( ((void*)menuitem) == ((void*)(mw->lcd_filter_light)) )
    {
      filter = FT_LCD_FILTER_LIGHT;
      set_status( "LCD filter set to light" );
    }
    else if( ((void*)menuitem) == ((void*)(mw->lcd_filter_normal)) )
    {
      filter = FT_LCD_FILTER_DEFAULT;
      set_status( "LCD filter set to Default" );
    }
    else
      return;

    FT_Library_SetLcdFilter( globals.library, filter );

    if( globals.face )
      setup_glyph();
  }

  static void
  _menu_lcd_filter_enabled( gboolean enabled )
  {
    struct MenuWidgets *mw = &_menu_widgets;
    gtk_widget_set_sensitive( mw->lcd_filter_submenu_entry, enabled );
    gtk_widget_set_sensitive( mw->lcd_filter_none, enabled );
    gtk_widget_set_sensitive( mw->lcd_filter_light, enabled );
    gtk_widget_set_sensitive( mw->lcd_filter_normal, enabled );
  }


  /*************************************************************************/

  /*
   * Change Color handler
   */

  static void
  _menu_change_colors( GtkMenuItem *menuitem, gpointer user_data )
  {
    if( change_colors_dialog_run() == GTK_RESPONSE_OK )
      if( globals.face )
        setup_glyph();
      else
        invalidate_drawing_area();
  }


  /*************************************************************************/

  /*
   * Zoom/reset Handlers
   */

  static void
  _menu_zoom_change( GtkMenuItem *menuitem, gpointer user_data )
  {
    FT_F26Dot6 scale = globals.scale;

    if( ((void*)menuitem) == ((void*)(_menu_widgets.zoom_inc)) )
    {
      scale = (int) (scale * 1.1);

      /* Truncated back to the same scale, add a whole number instead */
      if (scale == globals.scale) scale += 1;

      if( scale > globals.scale_0 * 10 )
        return;
    }
    else if( ((void*)menuitem) == ((void*)(_menu_widgets.zoom_dec)) )
    {
      scale =  (int) (scale * 0.9);

      /* Truncated back to the same scale, subtract a whole number instead */
      if (scale == globals.scale) scale -= 1;

      if( scale < globals.scale_0 * 0.5 )
        return;
    }
    else
      return;

    globals.scale = scale;
    setup_glyph();
  }

  static void
  _menu_view_reset( GtkMenuItem *menuitem, gpointer user_data )
  {
    globals.scale = globals.scale_0;
    globals.x_origin = globals.x_origin_0;
    globals.y_origin = globals.y_origin_0;
    setup_glyph();
  }

  static void
  _menu_view_controls_enabled( gboolean enabled )
  {
    gtk_widget_set_sensitive( _menu_widgets.zoom_inc, enabled );
    gtk_widget_set_sensitive( _menu_widgets.zoom_dec, enabled );
    gtk_widget_set_sensitive( _menu_widgets.view_reset, enabled );
  }


  /*************************************************************************/

  /*
   * Subpixel/normal render mode handlers
   */

  static void
  _menu_toggle_subpixel( GtkMenuItem *menuitem, gpointer user_data )
  {
    globals.lcd_rendering = globals.lcd_rendering ? FALSE : TRUE;

    if( globals.face )
      setup_glyph();
  }

  static void
  _menu_toggle_subpixel_mask( GtkMenuItem *menuitem, gpointer user_data )
  {
    globals.show_subpixel_mask = globals.show_subpixel_mask ? FALSE : TRUE;

    if( globals.face )
      setup_glyph();
    else
      invalidate_drawing_area(); /* The background gets changed. */
  }

  static void
  _menu_view_subpixel_enabled( gboolean enabled )
  {
    gtk_widget_set_sensitive( _menu_widgets.view_subpixel, enabled );
    gtk_widget_set_sensitive( _menu_widgets.show_subpixel_mask, enabled );
  }


  /*************************************************************************/

  /*
   * Misc tool menu handlers
   */

  static void
  _menu_goto_glyph_index( GtkMenuItem *menuitem, gpointer user_data )
  {
    if( goto_index_dialog_run() != GTK_RESPONSE_OK )
      return;

    unsigned int index = goto_index_dialog_get_index();

    if( globals.glyph_index == index )
      return;

    globals.glyph_index = index;
    analysis_detect_face_edges();
    setup_glyph();
  }

  static void
  _menu_goto_glyph_index_enabled( gboolean enabled )
  {
    gtk_widget_set_sensitive( _menu_widgets.goto_glyph_index, enabled );
  }

  static void
  _menu_goto_char( GtkMenuItem *menuitem, gpointer user_data )
  {
    gunichar unichar;

    if( goto_char_dialog_run() != GTK_RESPONSE_OK )
      return;

    unichar = goto_char_dialog_get_char();
    if( unichar == (gunichar)-1 || unichar == (gunichar)-2 )
      return;

    globals.glyph_index = FT_Get_Char_Index( globals.face, (FT_ULong)unichar );
    analysis_detect_face_edges();
    setup_glyph();
  }

  static void
  _menu_goto_char_enabled( gboolean enabled )
  {
    gtk_widget_set_sensitive( _menu_widgets.goto_char, enabled );
  }


  /*************************************************************************/

  /*
   * Misc tool menu handlers
   */

  static void
  _menu_mode_change( GtkMenuItem *menuitem, gpointer user_data )
  {
    struct MenuWidgets *mw = &_menu_widgets;

    if( ((void*)menuitem) == ((void*)(mw->mode_normal)) )
    {
      _menu_toggle_outline_enabled( TRUE );
      _menu_font_hinting_set_enabled( TRUE );
      _release_flag( &globals.draw_outline );
      globals.viewer_mode = VIEWER_MODE_NORMAL;
    }
    else if( ((void*)menuitem) == ((void*)(mw->mode_edges)) )
    {
      _menu_toggle_outline_enabled( FALSE );
      _menu_font_hinting_set_enabled( FALSE );
      _force_flag( &globals.draw_outline, 1 );
      globals.viewer_mode = VIEWER_MODE_EDGES;
    }
    else if( ((void*)menuitem) == ((void*)(mw->mode_uniform_scaling)) )
    {
      _menu_toggle_outline_enabled( TRUE );
      _menu_font_hinting_set_enabled( FALSE );
      _release_flag( &globals.draw_outline );
      globals.viewer_mode = VIEWER_MODE_UNIFORM_SCALING;
    }
    else if( ((void*)menuitem) == ((void*)(mw->mode_custom_hinting)) )
    {
      _menu_toggle_outline_enabled( TRUE );
      _menu_font_hinting_set_enabled( FALSE );
      _release_flag( &globals.draw_outline );
      globals.viewer_mode = VIEWER_MODE_CUSTOM_HINTING;
    }
    else
      return;

    if( globals.face )
      setup_glyph();
  }



  /*************************************************************************/

  /*
   * Drag Handlers
   */

  static void
  _drawing_area_button_down( GtkWidget      *event_box,
                             GdkEventButton *event,
                             gpointer        data )
  {
    if( globals.face && event->button == 1 &&
        !_control_status.mouse_grabbed )
    {
      GdkGrabStatus status;
      status = gdk_pointer_grab( gtk_widget_get_window( event_box ),
                                 FALSE,
                                 GDK_POINTER_MOTION_MASK |
                                 GDK_BUTTON_RELEASE_MASK,
                                 NULL,
                                 NULL,
                                 event->time);
      if( status != GDK_GRAB_SUCCESS)
        return;

      _control_status.grab_x = event->x;
      _control_status.grab_y = event->y;

      _control_status.start_x = globals.x_origin;
      _control_status.start_y = globals.y_origin;

      _control_status.mouse_grabbed = TRUE;
    }
  }

  static void
  _drawing_area_button_up( GtkWidget      *event_box,
                           GdkEventButton *event,
                           gpointer        data )
  {
    if( event->button == 1 && _control_status.mouse_grabbed )
    {
      gdk_pointer_ungrab( event->time );

      _control_status.mouse_grabbed = FALSE;
    }
  }

  static void
  _drawing_area_grab_broken( GtkWidget      *event_box,
                             GdkEventButton *event,
                             gpointer        data )
  {
    _control_status.mouse_grabbed = FALSE;
  }

  static void
  _drawing_area_grab_motion( GtkWidget      *event_box,
                             GdkEventButton *event,
                             gpointer        data )
  {
    if( _control_status.mouse_grabbed )
    {
      GtkAllocation alloc;
      gtk_widget_get_allocation (globals.drawing_area, &alloc);

      int x_origin = _control_status.start_x;
      int y_origin = _control_status.start_y;

      int x_delta = (int)( event->x - _control_status.grab_x );
      int y_delta = (int)( event->y - _control_status.grab_y );

      /* Prevent the origin from moving more than 1.5 times the window
         dimension away. Hopefully this gives enough ability to pan
         larger sizes. */

      if( x_origin + x_delta > alloc.width * 1.5 )
        x_delta = (int)( alloc.width * 1.5 ) - x_origin;
      else if ( x_origin + x_delta < -alloc.width * 1.5 )
        x_delta = (int)( -alloc.width * 1.5 ) - x_origin;

      if( y_origin + y_delta > alloc.height * 1.5 )
        y_delta = (int)( alloc.height * 1.5 ) - y_origin;
      else if ( y_origin + y_delta < -alloc.height * 1.5 )
        y_delta = (int)( -alloc.height * 1.5 ) - y_origin;

      globals.x_origin = _control_status.start_x + x_delta;
      globals.y_origin = _control_status.start_y + y_delta;

      invalidate_drawing_area();
    }
  }


  /* -------------------------------------------------------------------------- *\
   *
   *                       == Main Menu Functions ==
   *
  \* -------------------------------------------------------------------------- */


  static inline void
  _activate_handler( void *item, void *handler )
  {
    g_signal_connect( G_OBJECT( item ), "activate", G_CALLBACK( handler ),
                                                    NULL );
  }


  void
  setup_menu_items()
  {
    struct MenuWidgets *mw = &_menu_widgets;

    /* --------- */
    /* Font Menu */
    /* --------- */

    /* Open Font Dialog */
    mw->font_open_item = get_builder_widget( "font_open_item" );
    _activate_handler( mw->font_open_item, _menu_open_font );

    /* Increase Font Size */
    mw->font_size_inc = get_builder_widget( "font_size_inc" );
    _activate_handler( mw->font_size_inc, _menu_font_size_change );

    gtk_widget_add_accelerator( mw->font_size_inc,
                                "activate",
                                globals.accels, 
                                GDK_Up,
                                GDK_CONTROL_MASK, /* Can't use key without */
                                GTK_ACCEL_VISIBLE );

    /* Decrease Font Size */
    mw->font_size_dec = get_builder_widget( "font_size_dec" );
    _activate_handler( mw->font_size_dec, _menu_font_size_change );

    gtk_widget_add_accelerator( mw->font_size_dec,
                                "activate",
                                globals.accels, 
                                GDK_Down,
                                GDK_CONTROL_MASK,
                                GTK_ACCEL_VISIBLE );

    /* Hinting None  */
    mw->font_hinting_none = get_builder_widget( "font_hinting_none" );
    _activate_handler( mw->font_hinting_none, _menu_font_hinting_mode );

    /* Hinting Light */
    mw->font_hinting_light = get_builder_widget( "font_hinting_light" );
    _activate_handler( mw->font_hinting_light, _menu_font_hinting_mode );

    /* Hinting Normal */
    mw->font_hinting_normal = get_builder_widget( "font_hinting_normal" );
    _activate_handler( mw->font_hinting_normal, _menu_font_hinting_mode );

    /* Force Autohint */
    mw->font_force_autohint = get_builder_widget( "font_force_autohint" );
    _activate_handler( mw->font_force_autohint, _menu_font_force_autohint );

    /* Increase Glyph Index */
    mw->glyph_index_inc = get_builder_widget( "glyph_index_inc" );
    _activate_handler( mw->glyph_index_inc, _menu_glyph_index_change );

    gtk_widget_add_accelerator( mw->glyph_index_inc,
                                "activate",
                                globals.accels, 
                                GDK_Right,
                                GDK_CONTROL_MASK,
                                GTK_ACCEL_VISIBLE );

    /* Decrease Glyph Index */
    mw->glyph_index_dec = get_builder_widget( "glyph_index_dec" );
    _activate_handler( mw->glyph_index_dec, _menu_glyph_index_change );

    gtk_widget_add_accelerator( mw->glyph_index_dec,
                                "activate",
                                globals.accels, 
                                GDK_Left,
                                GDK_CONTROL_MASK,
                                GTK_ACCEL_VISIBLE );


    /* ------------- */
    /* Settings Menu */
    /* ------------- */

    /* Use Gamma Correction */
    mw->gamma_correct = get_builder_widget( "gamma_correct" );
    _activate_handler( mw->gamma_correct, _menu_gamma_toggle );

    gtk_widget_add_accelerator( mw->gamma_correct,
                                "activate",
                                globals.accels, 
                                GDK_g,
                                GDK_SHIFT_MASK,
                                GTK_ACCEL_VISIBLE);

    /* Increase Gamma */
    mw->gamma_inc = get_builder_widget( "gamma_inc" );
    _activate_handler( mw->gamma_inc, _menu_gamma_change );

    gtk_widget_add_accelerator( mw->gamma_inc,
                                "activate",
                                globals.accels, 
                                GDK_g,
                                0,
                                GTK_ACCEL_VISIBLE);

    /* Decrease Gamma */
    mw->gamma_dec = get_builder_widget( "gamma_dec" );
    _activate_handler( mw->gamma_dec, _menu_gamma_change );

    gtk_widget_add_accelerator( mw->gamma_dec,
                                "activate",
                                globals.accels, 
                                GDK_v,
                                0,
                                GTK_ACCEL_VISIBLE);

    /* Show Pixel Grid */
    mw->show_grid = get_builder_widget( "show_grid" );
    _activate_handler( mw->show_grid, _menu_toggle_grid );

    /* Show Outline */
    mw->show_outline = get_builder_widget( "show_outline" );
    _activate_handler( mw->show_outline, _menu_toggle_outline );

    /* LCD Filter */
    mw->lcd_filter_none = get_builder_widget( "lcd_filter_none" );
    _activate_handler( mw->lcd_filter_none, _menu_lcd_filter );

    mw->lcd_filter_light = get_builder_widget( "lcd_filter_light" );
    _activate_handler( mw->lcd_filter_light, _menu_lcd_filter );

    mw->lcd_filter_normal = get_builder_widget( "lcd_filter_normal" );
    _activate_handler( mw->lcd_filter_normal, _menu_lcd_filter );

    /* Change Colors */
    mw->change_colors = get_builder_widget( "change_colors" );
    _activate_handler( mw->change_colors, _menu_change_colors );


    /* --------- */
    /* View Menu */
    /* --------- */

    /* Increase Zoom */
    mw->zoom_inc = get_builder_widget( "zoom_inc" );
    _activate_handler( mw->zoom_inc, _menu_zoom_change );

    gtk_widget_add_accelerator( mw->zoom_inc,
                                "activate",
                                globals.accels, 
                                GDK_KEY_bracketright,
                                0,
                                GTK_ACCEL_VISIBLE );

    /* Decrease Zoom */
    mw->zoom_dec = get_builder_widget( "zoom_dec" );
    _activate_handler( mw->zoom_dec, _menu_zoom_change );

    gtk_widget_add_accelerator( mw->zoom_dec,
                                "activate",
                                globals.accels, 
                                GDK_KEY_bracketleft,
                                0,
                                GTK_ACCEL_VISIBLE );

    /* Reset View */
    mw->view_reset = get_builder_widget( "view_reset" );
    _activate_handler( mw->view_reset, _menu_view_reset );

    gtk_widget_add_accelerator( mw->view_reset,
                                "activate",
                                globals.accels, 
                                GDK_0,
                                GDK_CONTROL_MASK,
                                GTK_ACCEL_VISIBLE );

    /* Subpixel Rendering */
    mw->view_subpixel = get_builder_widget( "view_subpixel" );
    _activate_handler( mw->view_subpixel, _menu_toggle_subpixel );

    /* Show Subpixel Mask */
    mw->show_subpixel_mask = get_builder_widget( "show_subpixel_mask" );
    _activate_handler( mw->show_subpixel_mask, _menu_toggle_subpixel_mask );


    /* ---------- */
    /* Tools Menu */
    /* ---------- */

    /* Goto Glyph Index */
    mw->goto_glyph_index = get_builder_widget( "goto_glyph_index" );
    _activate_handler( mw->goto_glyph_index, _menu_goto_glyph_index );

    /* Goto Char */
    mw->goto_char = get_builder_widget( "goto_char" );
    _activate_handler( mw->goto_char, _menu_goto_char );


    /* --------- */
    /* Mode Menu */
    /* --------- */

    /* Mode Normal */
    mw->mode_normal = get_builder_widget( "mode_normal" );
    _activate_handler( mw->mode_normal, _menu_mode_change );

    /* Mode Edge Detect */
    mw->mode_edges = get_builder_widget( "mode_edges" );
    _activate_handler( mw->mode_edges, _menu_mode_change );

    /* Mode Uniform Scaling */
    mw->mode_uniform_scaling = get_builder_widget( "mode_uniform_scaling" );
    _activate_handler( mw->mode_uniform_scaling, _menu_mode_change );

    /* Mode Custom Hinting */
    mw->mode_custom_hinting = get_builder_widget( "mode_custom_hinting" );
    _activate_handler( mw->mode_custom_hinting, _menu_mode_change );
  }


  void
  setup_drag_handlers()
  {
    /* Drawing area isn't set to react to general user input by default */
    gtk_widget_add_events( globals.drawing_area, GDK_BUTTON_PRESS_MASK |
                                                 GDK_BUTTON_RELEASE_MASK );

    g_signal_connect( G_OBJECT( globals.drawing_area ), "button_press_event",
                      G_CALLBACK( _drawing_area_button_down ), NULL );

    g_signal_connect( G_OBJECT( globals.drawing_area ), "button_release_event",
                      G_CALLBACK( _drawing_area_button_up ), NULL );

    g_signal_connect( G_OBJECT( globals.drawing_area ), "grab-broken-event",
                      G_CALLBACK( _drawing_area_grab_broken ), NULL );

    g_signal_connect( G_OBJECT( globals.drawing_area ), "motion-notify-event",
                      G_CALLBACK( _drawing_area_grab_motion ), NULL );
  }


  void
  init_controls()
  {
    struct ControlStatus *cs = &_control_status;

    cs->mouse_grabbed = FALSE;

    setup_menu_items();
    setup_drag_handlers();

    goto_index_dialog_init();
    goto_char_dialog_init();
    select_face_dialog_init();
    change_colors_dialog_init();
  }


/* END */
