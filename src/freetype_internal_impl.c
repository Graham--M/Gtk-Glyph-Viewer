#include "freetype_internal_impl.h"


  FT_Int
  impl_ft_corner_is_flat( FT_Pos  in_x,
                          FT_Pos  in_y,
                          FT_Pos  out_x,
                          FT_Pos  out_y )
  {
    FT_Pos  ax = in_x + out_x;
    FT_Pos  ay = in_y + out_y;

    FT_Pos  d_in, d_out, d_hypot;

    d_in    = FT_HYPOT(  in_x,  in_y );
    d_out   = FT_HYPOT( out_x, out_y );
    d_hypot = FT_HYPOT(    ax,    ay );

    return ( d_in + d_out - d_hypot ) < ( d_hypot >> 4 );
  }

/* END */