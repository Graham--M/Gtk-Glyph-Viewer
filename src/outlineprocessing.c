#include "outlineprocessing.h"
#include FT_OUTLINE_H


  static int
  _move_to( const FT_Vector *to, void *user )
  {
    cairo_move_to( (cairo_t *)user, to->x / 64.0, to->y / 64.0 );
    return 0;
  }


  static int
  _line_to( const FT_Vector *to, void *user )
  {
    cairo_line_to( (cairo_t *)user, to->x / 64.0, to->y / 64.0 );
    return 0;
  }


  static int
  _conic_to( const FT_Vector *ctl,
             const FT_Vector *to,
             void            *user )
  {
    /*
     * Cairo only uses cubic bezier curves (two control points).
     * Need to convert from quatratic (one control point) to cubic using:
     *
     * Quatratic - from:Q0, ctl:Q1, to:Q2
     * Cubic - from: C0, control 1: C1, control 2:C2, to:C3
     *
     * C0 = Q0
     * C3 = Q2
     * C1 = Q0 + 2/3*(Q1-Q0)
     * C2 = Q2 + 2/3*(Q1-Q2)
     *
     * See http://fontforge.github.io/bezier.html
     */
    double cur_x,  cur_y,
           ctl_x,  ctl_y,
            to_x,   to_y,
            c1_x,   c1_y,
            c2_x,   c2_y,
            twothird;

    cairo_t *cr = (cairo_t *)user;
    twothird = 2/3.0;

    cairo_get_current_point( cr, &cur_x, &cur_y );

    ctl_x = ctl->x / 64.0;
    ctl_y = ctl->y / 64.0;
     to_x =  to->x / 64.0;
     to_y =  to->y / 64.0;

    c1_x = cur_x + twothird * ( ctl_x - cur_x );
    c1_y = cur_y + twothird * ( ctl_y - cur_y );
    c2_x =  to_x + twothird * ( ctl_x -  to_x );
    c2_y =  to_y + twothird * ( ctl_y -  to_y );

    cairo_curve_to( cr, c1_x, c1_y, c2_x, c2_y, to_x, to_y);

    return 0;
  }


  static int
  _cubic_to( const FT_Vector *c1,
             const FT_Vector *c2,
             const FT_Vector *to,
             void            *user )
  {
    cairo_curve_to( (cairo_t *)user, c1->x / 64.0, c1->y / 64.0,
                                     c2->x / 64.0, c2->y / 64.0,
                                     to->x / 64.0, to->y / 64.0 );
    return 0;
  }


  static FT_Outline_Funcs funcs =
  {
    &_move_to,
    &_line_to,
    &_conic_to,
    &_cubic_to,
    0, 0 /* Shift, Delta */
  };


  /* Convert the outline into a series of drawing commands and pass these */
  /* to the cairo context.                                                */
  void
  process_outline( cairo_t *cr, FT_Outline *outline )
  {
    FT_Outline_Decompose( outline, &funcs, (void*)cr );
  }


/* END */
