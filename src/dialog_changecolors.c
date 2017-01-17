#include "dialog_changecolors.h"
#include "glyphviewerglobals.h"


  static GtkDialog *dialog;


  static GtkColorButton *
  _get_color_button( gchar *name )
  {
    return GTK_COLOR_BUTTON(
               gtk_builder_get_object( globals.builder, name ) );
  }


  static void
  _set_button_color( GtkColorButton *button,
                     ViewerColor color,
                     gboolean set_alpha )
  {
    GdkColor gdkcolor;
    gdkcolor.red   = color.red * 65535;
    gdkcolor.green = color.green * 65535;
    gdkcolor.blue  = color.blue * 65535;

    gtk_color_button_set_color( button, &gdkcolor );

    if( set_alpha )
      gtk_color_button_set_alpha( button, (guint16) (color.alpha * 65535) );
  }


  static ViewerColor
  _get_button_color( GtkColorButton *button )
  {
    GdkColor gdkcolor;
    ViewerColor color;
    gtk_color_button_get_color( button, &gdkcolor );

    color.red   = gdkcolor.red / 65535.;
    color.green = gdkcolor.green / 65535.;
    color.blue  = gdkcolor.blue / 65535.;

    color.alpha = gtk_color_button_get_alpha( button ) / 65535.0;

    return color;
  }


  gint
  change_colors_dialog_run()
  {
    gint response;

    GtkColorButton *text, *bg, *grid, *origin, *outline, *points,
                   *ctrl_points, *unlinked, *linked, *serif;

    text         =  _get_color_button( "dlg_colors_text" );
    bg           =  _get_color_button( "dlg_colors_bg" );
    grid         =  _get_color_button( "dlg_colors_grid" );
    origin       =  _get_color_button( "dlg_colors_origin" );
    outline      =  _get_color_button( "dlg_colors_outline" );
    points       =  _get_color_button( "dlg_colors_points" );
    ctrl_points  =  _get_color_button( "dlg_colors_ctrl_points" );
    unlinked     =  _get_color_button( "dlg_colors_unlinked" );
    linked       =  _get_color_button( "dlg_colors_linked" );
    serif        =  _get_color_button( "dlg_colors_serif" );

    _set_button_color( text, globals.text_color, FALSE );
    _set_button_color( bg, globals.bg_color, FALSE );
    _set_button_color( grid, globals.grid_color, TRUE );
    _set_button_color( origin, globals.grid_origin_color, TRUE );
    _set_button_color( outline, globals.outline_color, TRUE );
    _set_button_color( points, globals.on_point_color, TRUE );
    _set_button_color( ctrl_points, globals.ctrl_point_color, TRUE );
    _set_button_color( unlinked, globals.edge_unlinked_color, TRUE );
    _set_button_color( linked, globals.edge_linked_color, TRUE );
    _set_button_color( serif, globals.edge_serif_color, TRUE );
    
    response = gtk_dialog_run( dialog );
    gtk_widget_hide( GTK_WIDGET( dialog ) );

    if( response == GTK_RESPONSE_OK )
    {
      globals.text_color           =  _get_button_color( text );
      globals.bg_color             =  _get_button_color( bg );
      globals.grid_color           =  _get_button_color( grid );
      globals.grid_origin_color    =  _get_button_color( origin );
      globals.outline_color        =  _get_button_color( outline );
      globals.on_point_color       =  _get_button_color( points );
      globals.ctrl_point_color     =  _get_button_color( ctrl_points );
      globals.edge_unlinked_color  =  _get_button_color( unlinked );
      globals.edge_linked_color    =  _get_button_color( linked );
      globals.edge_serif_color     =  _get_button_color( serif );
    }

    return response;
  }


  void
  change_colors_dialog_init()
  {
    dialog = GTK_DIALOG( gtk_builder_get_object( globals.builder,
                                                 "dlg_colors" ) );
  }


/* END */