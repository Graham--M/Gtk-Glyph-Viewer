#include "dialog_gotoindex.h"
#include "glyphviewerglobals.h"


static GtkDialog *dialog;


unsigned int
goto_index_dialog_get_index()
{
  GtkSpinButton *spin_button;

  spin_button = GTK_SPIN_BUTTON(
                    gtk_builder_get_object( globals.builder,
                                            "dlg_goto_index_index" ) );

  return (unsigned int)gtk_spin_button_get_value_as_int( spin_button );
}


gint
goto_index_dialog_run()
{
  gint response;
  GtkSpinButton *spin_button;
  GtkAdjustment *adjustment;

  spin_button = GTK_SPIN_BUTTON(
                    gtk_builder_get_object( globals.builder,
                                            "dlg_goto_index_index" ) );

  /* Set the max val for the spin button */
  adjustment = gtk_spin_button_get_adjustment( spin_button );
  gtk_adjustment_set_upper( adjustment, globals.face->num_glyphs - 1 );

  /* Set the initial value for the spin button */
  gtk_spin_button_set_value( spin_button, globals.glyph_index );
  
  response = gtk_dialog_run( dialog );
  gtk_widget_hide( GTK_WIDGET( dialog ) );
  return response;
}


void
goto_index_dialog_init()
{
  dialog = GTK_DIALOG( gtk_builder_get_object( globals.builder,
                                               "dlg_goto_index" ) );
}


/* END */