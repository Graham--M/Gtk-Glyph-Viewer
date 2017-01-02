#include <gtk/gtk.h>
#include <glib.h>

#ifndef DIALOG_GOTO_CHAR_H_
#define DIALOG_GOTO_CHAR_H_

gunichar
goto_char_dialog_get_char();

gint
goto_char_dialog_run();

void
goto_char_dialog_init();


#endif /* DIALOG_GOTO_CHAR_H_ */

/* END */