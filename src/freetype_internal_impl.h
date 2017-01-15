#include <ft2build.h>
#include FT_FREETYPE_H

/*
 * Provide alternatives for internal Freetype functions that aren't public.
 */

#ifndef FREETYPE_INTERNAL_IMPL_H_
#define FREETYPE_INTERNAL_IMPL_H_

#define FT_FREE( ptr ) \
          do { free( ptr ); ptr = 0; } while(0)


#define FT_MEM_SET( dest, byte, count )               \
          ft_memset( dest, byte, (FT_Offset)(count) )


#define FT_MEM_ZERO( dest, count )  FT_MEM_SET( dest, 0, count )


#define FT_ZERO( p )                FT_MEM_ZERO( p, sizeof ( *(p) ) )

/*
 * Good grief!
 * If the (re)allocation works, ptr is nonzero and so error will be set to 0.
 * The error is then also the result of the expression.
 */
#define FT_NEW_ARRAY( ptr, count )                                     \
          error = ( ( ptr = malloc( sizeof(*(ptr)) * count ) ) == 0 )  \
          	        ? FT_Err_Out_Of_Memory : 0


#define FT_RENEW_ARRAY( ptr, dummy, newcnt )                                  \
          error = ( ( ptr = realloc( ptr, sizeof(*(ptr)) * newcnt ) ) == 0 )  \
          	        ? FT_Err_Out_Of_Memory : 0


#define FT_ABS( a )     ( (a) < 0 ? -(a) : (a) )


#define FT_HYPOT( x, y )                 \
          ( x = FT_ABS( x ),             \
            y = FT_ABS( y ),             \
            x > y ? x + ( 3 * y >> 3 )   \
                  : y + ( 3 * x >> 3 ) )

#define FT_THROW( err )     FT_Err_ ## err


#define ft_corner_is_flat impl_ft_corner_is_flat

  FT_Int
  impl_ft_corner_is_flat( FT_Pos  in_x,
                          FT_Pos  in_y,
                          FT_Pos  out_x,
                          FT_Pos  out_y );


#endif /* FREETYPE_INTERNAL_IMPL_H_ */

/* END */