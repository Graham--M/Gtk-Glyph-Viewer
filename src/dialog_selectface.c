#include "dialog_selectface.h"
#include "glyphviewerglobals.h"


  static GtkDialog     *dialog;
  static GtkListStore  *store;
  static GtkComboBox   *combo;


  gint
  select_face_dialog_get_index()
  {
    return gtk_combo_box_get_active( combo );
  }


  gint
  select_face_dialog_run( GArray *faces )
  {
    gint response;
    GtkTreeIter iter;
    GString *s = g_string_new( "" );

    /* Setup the new list of faces */

    gtk_list_store_clear( store );

    for( int i = 0; i < faces->len; i++ )
    {
      FT_Face *face = &g_array_index( faces, FT_Face, i );

      /* Make name string, old string is truncated by printf */
      g_string_printf( s, "%s %s", face[0]->family_name, face[0]->style_name );

      /* Add new element */
      gtk_list_store_append( store, &iter );
      gtk_list_store_set( store, &iter, 0, s->str, -1 );
    }

    g_string_free( s, TRUE );
    gtk_combo_box_set_active( combo, 0 );

    response = gtk_dialog_run( dialog );
    gtk_widget_hide( GTK_WIDGET( dialog ) );

    return response;
  }


  void select_face_dialog_init()
  {
    dialog = GTK_DIALOG( gtk_builder_get_object( globals.builder,
                                                 "dlg_select_face" ) );

    store = GTK_LIST_STORE( gtk_builder_get_object( globals.builder,
                                                    "dlg_select_face_ls" ) );

    combo = GTK_COMBO_BOX( gtk_builder_get_object( globals.builder,
                                                    "dlg_select_face_combo" ) );
  }


/* END */