#include "dialog_gotochar.h"
#include "glyphviewerglobals.h"


static GtkDialog *dialog;


gunichar
goto_char_dialog_get_char()
{
  GtkEntry *entry;

  entry = GTK_ENTRY( gtk_builder_get_object( globals.builder,
                                             "dlg_goto_char_entry" ) );

  return g_utf8_get_char_validated( gtk_entry_get_text( entry ), -1 );
}

gint
goto_char_dialog_run()
{
  gint response;
  GtkEntry *entry;

  entry = GTK_ENTRY( gtk_builder_get_object( globals.builder,
                                             "dlg_goto_char_entry" ) );
  /* Clear the entry text */
  gtk_entry_set_text (entry, "");

  response = gtk_dialog_run( dialog );
  gtk_widget_hide( GTK_WIDGET( dialog ) );
  return response;
}


void
goto_char_dialog_init()
{
  dialog = GTK_DIALOG( gtk_builder_get_object( globals.builder,
                                               "dlg_goto_char" ) );
}


/* END */