#include <gtk/gtk.h>
#include <glib.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#ifndef DIALOG_SELECT_FACE_H_
#define DIALOG_SELECT_FACE_H_

  gint
  select_face_dialog_get_index();

  gint
  select_face_dialog_run( GArray *faces );

  void
  select_face_dialog_init();


#endif /* DIALOG_GOSELECT_FACE_ */

/* END */