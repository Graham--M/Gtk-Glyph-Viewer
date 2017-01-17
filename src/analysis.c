#include "analysis.h"
#include "utils.h"
#include "glyphviewerglobals.h"

#include FT_OUTLINE_H

#include "freetype_internal_impl.h"

  /* ----------------------------------------------------------------------- *\
   *
   *                      == Macro & Type Declarations ==
   *
  \* ----------------------------------------------------------------------- */

  typedef enum  Direction_
  {
    DIR_NONE  =  4,
    DIR_RIGHT =  1,
    DIR_LEFT  = -1,
    DIR_UP    =  2,
    DIR_DOWN  = -2

  } Direction;


  /* point control type flags */
#define POINT_FLAG_NONE    0
#define POINT_FLAG_CONIC    ( 1U << 0 )
#define POINT_FLAG_CUBIC    ( 1U << 1 )
#define POINT_FLAG_CONTROL  ( POINT_FLAG_CONIC | POINT_FLAG_CUBIC )

#define POINT_FLAG_DONE     ( 1U << 2 )

  /* candidates for weak interpolation have this flag set */
#define POINT_FLAG_WEAK_INTERPOLATION  ( 1U << 4 )

  /* the distance to the next point is very small */
#define POINT_FLAG_NEAR  ( 1U << 5 )

  /* edge hint flags */
#define EDGE_NORMAL  0
#define EDGE_ROUND    ( 1U << 0 )
#define EDGE_SERIF    ( 1U << 1 )
#define EDGE_DONE     ( 1U << 2 )


  typedef struct PointRec_*     Point;
  typedef struct SegmentRec_*   Segment;
  typedef struct EdgeRec_*      Edge;
  typedef struct AnalysisRec_*  Analysis;


  typedef struct  PointRec_
  {
    FT_UShort  flags;    /* point flags                  */
    FT_Char    in_dir;   /* direction of inwards vector  */
    FT_Char    out_dir;  /* direction of outwards vector */

    FT_Pos     ox, oy;   /* original, scaled position                   */
    FT_Short   fx, fy;   /* original, unscaled position (in font units) */
    FT_Pos     x, y;     /* current position                            */

    /*
     * The U,V fields are a holdover from the Freetype autohinter code
     * which has been adapted to work on the Y axis only. They are also
     * used to hold offsets to the prev/next non near points in that code
     * and this ought to be factored out into a seperate field so they can
     * be completely removed.
     */
    FT_Pos     u, v;     /* current (x,y) or (y,x) depending on context */

    Point      next;     /* next point in contour     */
    Point      prev;     /* previous point in contour */

  } PointRec;


  typedef struct  SegmentRec_
  {
    FT_Byte     flags;       /* edge/segment flags for this segment */
    FT_Char     dir;         /* segment direction                   */
    FT_Short    pos;         /* position of segment                 */
    FT_Short    delta;       /* deviation from segment position     */
    FT_Short    min_coord;   /* minimum coordinate of segment       */
    FT_Short    max_coord;   /* maximum coordinate of segment       */
    FT_Short    height;      /* segment height                      */

    Edge        edge;        /* the segment's parent edge           */
    Segment     edge_next;   /* link to next segment in parent edge */

    Segment     link;        /* (stem) link segment        */
    Segment     serif;       /* primary segment for serifs */
    FT_Pos      score;       /* used during stem matching  */
    FT_Pos      len;         /* used during stem matching  */

    Point       first;       /* first point in edge segment */
    Point       last;        /* last point in edge segment  */

  } SegmentRec;


  typedef struct  EdgeRec_
  {
    FT_Short    fpos;       /* original, unscaled position (in font units) */
    FT_Pos      opos;       /* original, scaled position                   */
    FT_Pos      pos;        /* current position                            */

    FT_Byte     flags;      /* edge flags                                   */
    FT_Char     dir;        /* edge direction                               */
    FT_Fixed    scale;      /* used to speed up interpolation between edges */

    Edge        link;       /* link edge                       */
    Edge        serif;      /* primary edge for serifs         */
    FT_Int      score;      /* used during stem matching       */

    Segment     first;      /* first segment in edge */
    Segment     last;       /* last segment in edge  */

  } EdgeRec;


#define POINTS_EMBEDDED    96   /* number of embedded points   */
#define CONTOURS_EMBEDDED   8   /* number of embedded contours */
#define SEGMENTS_EMBEDDED  18   /* number of embedded segments */
#define EDGES_EMBEDDED     12   /* number of embedded edges    */

  typedef struct  AnalysisRec_
  {
    FT_Face          face;

    FT_Fixed         x_scale;
    FT_Fixed         y_scale;

    FT_Int           max_points;    /* number of allocated points */
    FT_Int           num_points;    /* number of used points      */
    Point            points;        /* points array               */

    FT_Int           max_contours;  /* number of allocated contours */
    FT_Int           num_contours;  /* number of used contours      */
    Point*           contours;      /* contours array               */

    FT_Int           num_segments;  /* number of used segments      */
    FT_Int           max_segments;  /* number of allocated segments */
    Segment          segments;      /* segments array               */

    FT_Int           num_edges;     /* number of used edges      */
    FT_Int           max_edges;     /* number of allocated edges */
    Edge             edges;         /* edges array               */

    Direction        y_major_dir;   /* vertical outline fill dir */

    /* Arrays to avoid allocation penalty.                */
    /* The `embedded' structure must be the last element! */

    /* The `hints` structure this was derived from in the */
    /* freetype autohinter was destroyed at the end of    */
    /* the glyph hinting so this avoided a call to        */
    /* malloc on each glyph load. */
    struct
    {
      Point          contours[CONTOURS_EMBEDDED];
      PointRec       points[POINTS_EMBEDDED];
      SegmentRec     segments[SEGMENTS_EMBEDDED];
      EdgeRec        edges[EDGES_EMBEDDED];
    } embedded;

  } AnalysisRec;


  /***************************************************************************/


  /* ----------------------------------------------------------------------- *\
   *
   *                        == Forward Declarations ==
   *
  \* ----------------------------------------------------------------------- */


  static Direction
  _direction_compute( FT_Pos  dx, FT_Pos  dy );


  static void
  _analysis_done( Analysis analysis );


  static FT_Error
  _analysis_reload( Analysis analysis, FT_Face face );


  static void
  _analysis_write_glyph( Analysis analysis );


  static FT_Error
  _analysis_new_segment( Analysis analysis, Segment *asegment );


  static FT_Error
  _analysis_new_edge( Analysis      analysis,
                      FT_Int        fpos,
                      Direction     dir,
                      Edge         *anedge );


  static FT_Error
  _analysis_compute_segments( Analysis analysis );


  static void
  _analysis_link_segments( Analysis analysis, FT_Pos linking_limit );


  static FT_Error
  _analysis_compute_edges( Analysis analysis );

  static FT_Pos
  _calculate_stem_linking_limit( Analysis analysis, FT_Face face );


  /***************************************************************************/


  /* ----------------------------------------------------------------------- *\
   *
   *                            == Functions ==
   *
  \* ----------------------------------------------------------------------- */


  static Direction
  _direction_compute( FT_Pos dx, FT_Pos dy )
  {
    FT_Pos        ll, ss;  /* long and short arm lengths */
    Direction     dir;     /* candidate direction        */


    if ( dy >= dx )
    {
      if ( dy >= -dx )
      {
        dir = DIR_UP;
        ll  = dy;
        ss  = dx;
      }
      else
      {
        dir = DIR_LEFT;
        ll  = -dx;
        ss  = dy;
      }
    }
    else /* dy < dx */
    {
      if ( dy >= -dx )
      {
        dir = DIR_RIGHT;
        ll  = dx;
        ss  = dy;
      }
      else
      {
        dir = DIR_DOWN;
        ll  = -dy;
        ss  = dx;
      }
    }

    /* return no direction if arm lengths do not differ enough       */
    /* (value 14 is heuristic, corresponding to approx. 4.1 degrees) */
    /* the long arm is never negative                                */
    if ( ll <= 14 * FT_ABS( ss ) )
      dir = DIR_NONE;

    return dir;
  }


  /* ----------------------------------------------------------------------- */


  static void
  _analysis_done( Analysis analysis )
  {
    if( !analysis )
      return;

    analysis->num_segments = 0;
    analysis->max_segments = 0;
    if( analysis->segments != analysis->embedded.segments )
      FT_FREE( analysis->segments );

    analysis->num_edges = 0;
    analysis->max_edges = 0;
    if( analysis->edges != analysis->embedded.edges )
      FT_FREE( analysis->edges );

    analysis->num_contours = 0;
    analysis->max_contours = 0;
    if( analysis->contours != analysis->embedded.contours )
      FT_FREE( analysis->contours );

    analysis->num_points = 0;
    analysis->max_points = 0;
    if( analysis->points != analysis->embedded.points )
      FT_FREE( analysis->points );
  }


  /* ----------------------------------------------------------------------- */


  static FT_Error
  _analysis_reload( Analysis analysis, FT_Face face )
  {
    Point        points;
    FT_UInt      old_max, new_max;

    FT_Error     error    =  FT_Err_Ok;
    FT_Outline*  outline  = &face->glyph->outline;
    FT_Fixed     x_scale  =  face->size->metrics.x_scale;
    FT_Fixed     y_scale  =  face->size->metrics.y_scale;


    analysis->face         = face;
    analysis->x_scale      = x_scale;
    analysis->y_scale      = y_scale;
    analysis->num_points   = 0;
    analysis->num_contours = 0;
    analysis->num_segments = 0;
    analysis->num_edges    = 0;

    /* first of all, reallocate the contours array if necessary */
    new_max = (FT_UInt)outline->n_contours;
    old_max = (FT_UInt)analysis->max_contours;

    if( new_max <= CONTOURS_EMBEDDED )
    {
      if( analysis->contours == NULL )
      {
        analysis->contours     = analysis->embedded.contours;
        analysis->max_contours = CONTOURS_EMBEDDED;
      }
    }
    else if( new_max > old_max )
    {
      if( analysis->contours == analysis->embedded.contours )
        analysis->contours = NULL;

      new_max = ( new_max + 3 ) & ~3U; /* round up to a multiple of 4 */

      if( FT_RENEW_ARRAY( analysis->contours, old_max, new_max ) )
        goto Exit;

      analysis->max_contours = (FT_Int)new_max;
    }

    /*
     *  then reallocate the points arrays if necessary --
     *  note that we reserve two additional point positions, used to
     *  hint metrics appropriately
     */
    new_max = (FT_UInt)( outline->n_points /*+ 2*/ ); /* Disabling this, can't see where it's used */
    old_max = (FT_UInt)analysis->max_points;

    if( new_max <= POINTS_EMBEDDED )
    {
      if( analysis->points == NULL )
      {
        analysis->points     = analysis->embedded.points;
        analysis->max_points = POINTS_EMBEDDED;
      }
    }
    else if( new_max > old_max )
    {
      if( analysis->points == analysis->embedded.points )
        analysis->points = NULL;

      new_max = ( new_max + 2 + 7 ) & ~7U; /* round up to a multiple of 8 */

      if( FT_RENEW_ARRAY( analysis->points, old_max, new_max ) )
        goto Exit;

      analysis->max_points = (FT_Int)new_max;
    }

    analysis->num_points   = outline->n_points;
    analysis->num_contours = outline->n_contours;

    /* We can't rely on the value of `FT_Outline.flags' to know the fill   */
    /* direction used for a glyph, given that some fonts are broken (e.g., */
    /* the Arphic ones).  We thus recompute it each time we need to.       */
    /*                                                                     */
    analysis->y_major_dir = DIR_LEFT;

    if( FT_Outline_Get_Orientation( outline ) == FT_ORIENTATION_POSTSCRIPT )
      analysis->y_major_dir = DIR_RIGHT;

    points = analysis->points;
    if( analysis->num_points == 0 )
      goto Exit;

    {
      Point  point;
      Point  point_limit = points + analysis->num_points;

      /* value 20 in `near_limit' is heuristic */
      FT_UInt  units_per_em = analysis->face->units_per_EM;
      FT_Int   near_limit   = 20 * units_per_em / 2048;


      /* compute coordinates & Bezier flags, next and prev */
      {
        FT_Vector*  vec           = outline->points;
        char*       tag           = outline->tags;
        FT_Short    endpoint      = outline->contours[0];
        Point       end           = points + endpoint;
        Point       prev          = end;
        FT_Int      contour_index = 0;


        for( point = points; point < point_limit; point++, vec++, tag++ )
        {
          FT_Pos  out_x, out_y;


          point->in_dir  = (FT_Char)DIR_NONE;
          point->out_dir = (FT_Char)DIR_NONE;

          point->fx = (FT_Short)vec->x;
          point->fy = (FT_Short)vec->y;
          point->ox = point->x = FT_MulFix( vec->x, x_scale );
          point->oy = point->y = FT_MulFix( vec->y, y_scale );

          end->fx = (FT_Short)outline->points[endpoint].x;
          end->fy = (FT_Short)outline->points[endpoint].y;

          switch( FT_CURVE_TAG( *tag ) )
          {
          case FT_CURVE_TAG_CONIC:
            point->flags = POINT_FLAG_CONIC;
            break;
          case FT_CURVE_TAG_CUBIC:
            point->flags = POINT_FLAG_CUBIC;
            break;
          default:
            point->flags = POINT_FLAG_NONE;
          }

          out_x = point->fx - prev->fx;
          out_y = point->fy - prev->fy;

          if( FT_ABS( out_x ) + FT_ABS( out_y ) < near_limit )
            prev->flags |= POINT_FLAG_NEAR;

          point->prev = prev;
          prev->next  = point;
          prev        = point;

          if( point == end )
          {
            if( ++contour_index < outline->n_contours )
            {
              endpoint = outline->contours[contour_index];
              end      = points + endpoint;
              prev     = end;
            }
          }
        }
      }

      /* set up the contours array */
      {
        Point*  contour          = analysis->contours;
        Point*  contour_limit    = contour + analysis->num_contours;
        short*     end           = outline->contours;
        short      idx           = 0;


        for( ; contour < contour_limit; contour++, end++ )
        {
          contour[0] = points + idx;
          idx        = (short)( end[0] + 1 );
        }
      }

      {
        /*
         *  Compute directions of `in' and `out' vectors.
         *
         *  Note that distances between points that are very near to each
         *  other are accumulated.  In other words, the auto-hinter either
         *  prepends the small vectors between near points to the first
         *  non-near vector, or the sum of small vector lengths exceeds a
         *  threshold, thus `grouping' the small vectors.  All intermediate
         *  points are tagged as weak; the directions are adjusted also to
         *  be equal to the accumulated one.
         */

        FT_Int  near_limit2 = 2 * near_limit - 1;

        Point*  contour;
        Point*  contour_limit = analysis->contours + analysis->num_contours;


        for( contour = analysis->contours; contour < contour_limit; contour++ )
        {
          Point  first = *contour;
          Point  next, prev, curr;

          FT_Pos  out_x, out_y;


          /* since the first point of a contour could be part of a */
          /* series of near points, go backwards to find the first */
          /* non-near point and adjust `first'                     */

          point = first;
          prev  = first->prev;

          while( prev != first )
          {
            out_x = point->fx - prev->fx;
            out_y = point->fy - prev->fy;

            /*
             *  We use Taxicab metrics to measure the vector length.
             *
             *  Note that the accumulated distances so far could have the
             *  opposite direction of the distance measured here.  For this
             *  reason we use `near_limit2' for the comparison to get a
             *  non-near point even in the worst case.
             */
            if( FT_ABS( out_x ) + FT_ABS( out_y ) >= near_limit2 )
              break;

            point = prev;
            prev  = prev->prev;
          }

          /* adjust first point */
          first = point;

          /* now loop over all points of the contour to get */
          /* `in' and `out' vector directions               */

          curr = first;

          /*
           *  We abuse the `u' and `v' fields to store index deltas to the
           *  next and previous non-near point, respectively.
           *
           *  To avoid problems with not having non-near points, we point to
           *  `first' by default as the next non-near point.
           *
           */
          curr->u  = (FT_Pos)( first - curr );
          first->v = -curr->u;

          out_x = 0;
          out_y = 0;

          next = first;
          do
          {
            Direction  out_dir;


            point = next;
            next  = point->next;

            out_x += next->fx - point->fx;
            out_y += next->fy - point->fy;

            if( FT_ABS( out_x ) + FT_ABS( out_y ) < near_limit )
            {
              next->flags |= POINT_FLAG_WEAK_INTERPOLATION;
              continue;
            }

            curr->u = (FT_Pos)( next - curr );
            next->v = -curr->u;

            out_dir = _direction_compute( out_x, out_y );

            /* adjust directions for all points inbetween; */
            /* the loop also updates position of `curr'    */
            curr->out_dir = (FT_Char)out_dir;
            for( curr = curr->next; curr != next; curr = curr->next )
            {
              curr->in_dir  = (FT_Char)out_dir;
              curr->out_dir = (FT_Char)out_dir;
            }
            next->in_dir = (FT_Char)out_dir;

            curr->u  = (FT_Pos)( first - curr );
            first->v = -curr->u;

            out_x = 0;
            out_y = 0;

          } while( next != first );
        }

        /*
         *  The next step is to `simplify' an outline's topology so that we
         *  can identify local extrema more reliably: A series of
         *  non-horizontal or non-vertical vectors pointing into the same
         *  quadrant are handled as a single, long vector.  From a
         *  topological point of the view, the intermediate points are of no
         *  interest and thus tagged as weak.
         */

        for( point = points; point < point_limit; point++ )
        {
          if( point->flags & POINT_FLAG_WEAK_INTERPOLATION )
            continue;

          if( point->in_dir  == DIR_NONE &&
              point->out_dir == DIR_NONE )
          {
            /* check whether both vectors point into the same quadrant */

            FT_Pos  in_x, in_y;
            FT_Pos  out_x, out_y;

            Point  next_u = point + point->u;
            Point  prev_v = point + point->v;


            in_x = point->fx - prev_v->fx;
            in_y = point->fy - prev_v->fy;

            out_x = next_u->fx - point->fx;
            out_y = next_u->fy - point->fy;

            if( ( in_x ^ out_x ) >= 0 && ( in_y ^ out_y ) >= 0 )
            {
              /* yes, so tag current point as weak */
              /* and update index deltas           */

              point->flags |= POINT_FLAG_WEAK_INTERPOLATION;

              prev_v->u = (FT_Pos)( next_u - prev_v );
              next_u->v = -prev_v->u;
            }
          }
        }

        /*
         *  Finally, check for remaining weak points.  Everything else not
         *  collected in edges so far is then implicitly classified as strong
         *  points.
         */

        for( point = points; point < point_limit; point++ )
        {
          if( point->flags & POINT_FLAG_WEAK_INTERPOLATION )
            continue;

          if( point->flags & POINT_FLAG_CONTROL )
          {
            /* control points are always weak */
          Is_Weak_Point:
            point->flags |= POINT_FLAG_WEAK_INTERPOLATION;
          }
          else if( point->out_dir == point->in_dir )
          {
            if( point->out_dir != DIR_NONE )
            {
              /* current point lies on a horizontal or          */
              /* vertical segment (but doesn't start or end it) */
              goto Is_Weak_Point;
            }

            {
              Point  next_u = point + point->u;
              Point  prev_v = point + point->v;


              if( ft_corner_is_flat( point->fx  - prev_v->fx,
                                     point->fy  - prev_v->fy,
                                     next_u->fx - point->fx,
                                     next_u->fy - point->fy ) )
              {
                /* either the `in' or the `out' vector is much more  */
                /* dominant than the other one, so tag current point */
                /* as weak and update index deltas                   */

                prev_v->u = (FT_Pos)( next_u - prev_v );
                next_u->v = -prev_v->u;

                goto Is_Weak_Point;
              }
            }
          }
          else if( point->in_dir == -point->out_dir )
          {
            /* current point forms a spike */
            goto Is_Weak_Point;
          }
        }
      }
    }

  Exit:
    return error;
  }


  /* ----------------------------------------------------------------------- */


  static void
  _analysis_write_glyph( Analysis analysis )
  {
  	FT_Outline*  outline = &analysis->face->glyph->outline;
    Point        point   = analysis->points;
    Point        limit   = point + analysis->num_points;
    FT_Vector*   vec     = outline->points;
    char*        tag     = outline->tags;


    for ( ; point < limit; point++, vec++, tag++ )
    {
      vec->x = point->x;
      vec->y = point->y;

      if( point->flags & POINT_FLAG_CONIC )
        tag[0] = FT_CURVE_TAG_CONIC;
      else if( point->flags & POINT_FLAG_CUBIC )
        tag[0] = FT_CURVE_TAG_CUBIC;
      else
        tag[0] = FT_CURVE_TAG_ON;
    }
  }


  /* ----------------------------------------------------------------------- */


  static FT_Error
  _analysis_new_segment( Analysis analysis, Segment *asegment )
  {
    FT_Error    error   = FT_Err_Ok;
    Segment     segment = NULL;


    if( analysis->num_segments < SEGMENTS_EMBEDDED )
    {
      if( analysis->segments == NULL )
      {
        analysis->segments     = analysis->embedded.segments;
        analysis->max_segments = SEGMENTS_EMBEDDED;
      }
    }
    else if( analysis->num_segments >= analysis->max_segments )
    {
      FT_Int  old_max = analysis->max_segments;
      FT_Int  new_max = old_max;
      FT_Int  big_max = (FT_Int)( FT_INT_MAX / sizeof ( *segment ) );


      if( old_max >= big_max )
      {
        error = FT_THROW( Out_Of_Memory );
        goto Exit;
      }

      new_max += ( new_max >> 2 ) + 4;
      if( new_max < old_max || new_max > big_max )
        new_max = big_max;

      if( analysis->segments == analysis->embedded.segments )
      {
        if( FT_NEW_ARRAY( analysis->segments, new_max ) )
          goto Exit;
        ft_memcpy( analysis->segments, analysis->embedded.segments,
                   sizeof ( analysis->embedded.segments ) );
      }
      else
      {
        if( FT_RENEW_ARRAY( analysis->segments, old_max, new_max ) )
          goto Exit;
      }

      analysis->max_segments = new_max;
    }

    segment = analysis->segments + analysis->num_segments++;

  Exit:
    *asegment = segment;
    return error;
  }


  /* ----------------------------------------------------------------------- */


  static FT_Error
  _analysis_new_edge( Analysis      analysis,
                      FT_Int        fpos,
                      Direction     dir,
                      Edge         *anedge )
  {
    FT_Error  error = FT_Err_Ok;
    Edge      edge  = NULL;
    Edge      edges;


    if( analysis->num_edges < EDGES_EMBEDDED )
    {
      if( analysis->edges == NULL )
      {
        analysis->edges     = analysis->embedded.edges;
        analysis->max_edges = EDGES_EMBEDDED;
      }
    }
    else if( analysis->num_edges >= analysis->max_edges )
    {
      FT_Int  old_max = analysis->max_edges;
      FT_Int  new_max = old_max;
      FT_Int  big_max = (FT_Int)( FT_INT_MAX / sizeof ( *edge ) );


      if( old_max >= big_max )
      {
        error = FT_THROW( Out_Of_Memory );
        goto Exit;
      }

      new_max += ( new_max >> 2 ) + 4;
      if( new_max < old_max || new_max > big_max )
        new_max = big_max;

      if( analysis->edges == analysis->embedded.edges )
      {
        if( FT_NEW_ARRAY( analysis->edges, new_max ) )
          goto Exit;
        ft_memcpy( analysis->edges, analysis->embedded.edges,
                   sizeof ( analysis->embedded.edges ) );
      }
      else
      {
        if ( FT_RENEW_ARRAY( analysis->edges, old_max, new_max ) )
          goto Exit;
      }

      analysis->max_edges = new_max;
    }

    edges = analysis->edges;
    edge  = edges + analysis->num_edges;

    while( edge > edges )
    {
      if( edge[-1].fpos < fpos )
        break;

      /* we want the edge with same position and minor direction */
      /* to appear before those in the major one in the list     */
      if( edge[-1].fpos == fpos && dir == analysis->y_major_dir )
        break;

      edge[0] = edge[-1];
      edge--;
    }

    analysis->num_edges++;

  Exit:
    *anedge = edge;
    return error;
  }


  /* ----------------------------------------------------------------------- */


  /* needed for computation of round vs. flat segments */
#define FLAT_THRESHOLD( x )  ( x / 14 )


  static FT_Error
  _analysis_compute_segments( Analysis analysis )
  {
    FT_Error         error         = FT_Err_Ok;
    Segment          segment       = NULL;
    SegmentRec       seg0;
    Point*           contour       = analysis->contours;
    Point*           contour_limit = contour + analysis->num_contours;
    Direction        major_dir, segment_dir;

    FT_Pos  flat_threshold = FLAT_THRESHOLD( analysis->face->units_per_EM );


    FT_ZERO( &seg0 );
    seg0.score = 32000;
    seg0.flags = EDGE_NORMAL;

    major_dir   = (Direction)FT_ABS( analysis->y_major_dir );
    segment_dir = major_dir;

    analysis->num_segments = 0;

    /* set up (u,v) in each point */
    Point  point = analysis->points;
    Point  limit = point + analysis->num_points;

    for( ; point < limit; point++ )
    {
      point->u = point->fy;
      point->v = point->fx;
    }

    /* do each contour separately */
    for( ; contour < contour_limit; contour++ )
    {
      Point     point   = contour[0];
      Point     last    = point->prev;
      int       on_edge = 0;

      /* we call values measured along a segment (point->v)    */
      /* `coordinates', and values orthogonal to it (point->u) */
      /* `positions'                                           */
      FT_Pos     min_pos      =  32000;
      FT_Pos     max_pos      = -32000;
      FT_Pos     min_coord    =  32000;
      FT_Pos     max_coord    = -32000;
      FT_UShort  min_flags    =  POINT_FLAG_NONE;
      FT_UShort  max_flags    =  POINT_FLAG_NONE;
      FT_Pos     min_on_coord =  32000;
      FT_Pos     max_on_coord = -32000;

      FT_Bool  passed;

      Segment  prev_segment = NULL;

      FT_Pos     prev_min_pos      = min_pos;
      FT_Pos     prev_max_pos      = max_pos;
      FT_Pos     prev_min_coord    = min_coord;
      FT_Pos     prev_max_coord    = max_coord;
      FT_UShort  prev_min_flags    = min_flags;
      FT_UShort  prev_max_flags    = max_flags;
      FT_Pos     prev_min_on_coord = min_on_coord;
      FT_Pos     prev_max_on_coord = max_on_coord;


      if( FT_ABS( last->out_dir )  == major_dir &&
          FT_ABS( point->out_dir ) == major_dir )
      {
        /* we are already on an edge, try to locate its start */
        last = point;

        for(;;)
        {
          point = point->prev;
          if( FT_ABS( point->out_dir ) != major_dir )
          {
            point = point->next;
            break;
          }
          if ( point == last )
            break;
        }
      }

      last   = point;
      passed = 0;

      for(;;)
      {
        FT_Pos  u, v;


        if( on_edge )
        {
          /* get minimum and maximum position */
          u = point->u;
          if( u < min_pos )
            min_pos = u;
          if( u > max_pos )
            max_pos = u;

          /* get minimum and maximum coordinate together with flags */
          v = point->v;
          if( v < min_coord )
          {
            min_coord = v;
            min_flags = point->flags;
          }
          if( v > max_coord )
          {
            max_coord = v;
            max_flags = point->flags;
          }

          /* get minimum and maximum coordinate of `on' points */
          if( !( point->flags & POINT_FLAG_CONTROL ) )
          {
            v = point->v;
            if( v < min_on_coord )
              min_on_coord = v;
            if( v > max_on_coord )
              max_on_coord = v;
          }

          if( point->out_dir != segment_dir || point == last )
          {
            /* check whether the new segment's start point is identical to */
            /* the previous segment's end point; for example, this might   */
            /* happen for spikes                                           */

            if( !prev_segment || segment->first != prev_segment->last )
            {
              /* points are different: we are just leaving an edge, thus */
              /* record a new segment                                    */

              segment->last  = point;
              segment->pos   = (FT_Short)( ( min_pos + max_pos ) >> 1 );
              segment->delta = (FT_Short)( ( max_pos - min_pos ) >> 1 );

              /* a segment is round if either its first or last point */
              /* is a control point, and the length of the on points  */
              /* inbetween doesn't exceed a heuristic limit           */
              if( ( min_flags | max_flags ) & POINT_FLAG_CONTROL      &&
                  ( max_on_coord - min_on_coord ) < flat_threshold )
                segment->flags |= EDGE_ROUND;

              segment->min_coord = (FT_Short)min_coord;
              segment->max_coord = (FT_Short)max_coord;
              segment->height    = segment->max_coord - segment->min_coord;

              prev_segment      = segment;
              prev_min_pos      = min_pos;
              prev_max_pos      = max_pos;
              prev_min_coord    = min_coord;
              prev_max_coord    = max_coord;
              prev_min_flags    = min_flags;
              prev_max_flags    = max_flags;
              prev_min_on_coord = min_on_coord;
              prev_max_on_coord = max_on_coord;
            }
            else
            {
              /* points are the same: we don't create a new segment but */
              /* merge the current segment with the previous one        */

              if( prev_segment->last->in_dir == point->in_dir )
              {
                /* we have identical directions (this can happen for       */
                /* degenerate outlines that move zig-zag along the main    */
                /* axis without changing the coordinate value of the other */
                /* axis, and where the segments have just been merged):    */
                /* unify segments                                          */

                /* update constraints */

                if( prev_min_pos < min_pos )
                  min_pos = prev_min_pos;
                if( prev_max_pos > max_pos )
                  max_pos = prev_max_pos;

                if( prev_min_coord < min_coord )
                {
                  min_coord = prev_min_coord;
                  min_flags = prev_min_flags;
                }
                if( prev_max_coord > max_coord )
                {
                  max_coord = prev_max_coord;
                  max_flags = prev_max_flags;
                }

                if( prev_min_on_coord < min_on_coord )
                  min_on_coord = prev_min_on_coord;
                if( prev_max_on_coord > max_on_coord )
                  max_on_coord = prev_max_on_coord;

                prev_segment->last = point;
                prev_segment->pos  = (FT_Short)( ( min_pos +
                                                   max_pos ) >> 1 );

                if( ( min_flags | max_flags ) & POINT_FLAG_CONTROL      &&
                    ( max_on_coord - min_on_coord ) < flat_threshold )
                  prev_segment->flags |= EDGE_ROUND;
                else
                  prev_segment->flags &= ~EDGE_ROUND;

                prev_segment->min_coord = (FT_Short)min_coord;
                prev_segment->max_coord = (FT_Short)max_coord;
                prev_segment->height    = prev_segment->max_coord -
                                          prev_segment->min_coord;
              }
              else
              {
                /* we have different directions; use the properties of the */
                /* longer segment and discard the other one                */

                if( FT_ABS( prev_max_coord - prev_min_coord ) >
                    FT_ABS( max_coord - min_coord ) )
                {
                  /* discard current segment */

                  if( min_pos < prev_min_pos )
                    prev_min_pos = min_pos;
                  if( max_pos > prev_max_pos )
                    prev_max_pos = max_pos;

                  prev_segment->last = point;
                  prev_segment->pos  = (FT_Short)( ( prev_min_pos +
                                                     prev_max_pos ) >> 1 );
                }
                else
                {
                  /* discard previous segment */

                  if( prev_min_pos < min_pos )
                    min_pos = prev_min_pos;
                  if( prev_max_pos > max_pos )
                    max_pos = prev_max_pos;

                  segment->last = point;
                  segment->pos  = (FT_Short)( ( min_pos + max_pos ) >> 1 );

                  if( ( min_flags | max_flags ) & POINT_FLAG_CONTROL      &&
                      ( max_on_coord - min_on_coord ) < flat_threshold )
                    segment->flags |= EDGE_ROUND;

                  segment->min_coord = (FT_Short)min_coord;
                  segment->max_coord = (FT_Short)max_coord;
                  segment->height    = segment->max_coord -
                                       segment->min_coord;

                  *prev_segment = *segment;

                  prev_min_pos      = min_pos;
                  prev_max_pos      = max_pos;
                  prev_min_coord    = min_coord;
                  prev_max_coord    = max_coord;
                  prev_min_flags    = min_flags;
                  prev_max_flags    = max_flags;
                  prev_min_on_coord = min_on_coord;
                  prev_max_on_coord = max_on_coord;
                }
              }

              analysis->num_segments--;
            }

            on_edge = 0;
            segment = NULL;

            /* fall through */
          }
        }

        /* now exit if we are at the start/end point */
        if( point == last )
        {
          if( passed )
            break;
          passed = 1;
        }

        /* if we are not on an edge, check whether the major direction */
        /* coincides with the current point's `out' direction, or      */
        /* whether we have a single-point contour                      */
        if( !on_edge                                  &&
            ( FT_ABS( point->out_dir ) == major_dir ||
              point == point->prev                  ) )
        {
          /* this is the start of a new segment! */
          segment_dir = (Direction)point->out_dir;

          error = _analysis_new_segment( analysis, &segment );
          if( error )
            goto Exit;

          /* clear all segment fields */
          segment[0] = seg0;

          segment->dir   = (FT_Char)segment_dir;
          segment->first = point;
          segment->last  = point;

          /* `_analysis_new_segment' reallocates memory,        */
          /* thus we have to refresh the `prev_segment' pointer */
          if( prev_segment )
            prev_segment = segment - 1;

          min_pos   = max_pos   = point->u;
          min_coord = max_coord = point->v;
          min_flags = max_flags = point->flags;

          if( point->flags & POINT_FLAG_CONTROL )
          {
            min_on_coord =  32000;
            max_on_coord = -32000;
          }
          else
            min_on_coord = max_on_coord = point->v;

          on_edge = 1;

          if( point == point->prev )
          {
            /* we have a one-point segment: this is a one-point */
            /* contour with `in' and `out' direction set to     */
            /* DIR_NONE                                         */
            segment->pos = (FT_Short)min_pos;

            if (point->flags & POINT_FLAG_CONTROL)
              segment->flags |= EDGE_ROUND;

            segment->min_coord = (FT_Short)point->v;
            segment->max_coord = (FT_Short)point->v;
            segment->height = 0;

            on_edge = 0;
            segment = NULL;
          }
        }

        point = point->next;
      }

    } /* contours */


    /* now slightly increase the height of segments if this makes */
    /* sense -- this is used to better detect and ignore serifs   */
    {
      Segment  segments     = analysis->segments;
      Segment  segments_end = segments + analysis->num_segments;


      for( segment = segments; segment < segments_end; segment++ )
      {
        Point     first   = segment->first;
        Point     last    = segment->last;
        FT_Pos    first_v = first->v;
        FT_Pos    last_v  = last->v;


        if( first_v < last_v )
        {
          Point  p;


          p = first->prev;
          if( p->v < first_v )
            segment->height = (FT_Short)( segment->height +
                                          ( ( first_v - p->v ) >> 1 ) );

          p = last->next;
          if( p->v > last_v )
            segment->height = (FT_Short)( segment->height +
                                          ( ( p->v - last_v ) >> 1 ) );
        }
        else
        {
          Point  p;


          p = first->prev;
          if ( p->v > first_v )
            segment->height = (FT_Short)( segment->height +
                                          ( ( p->v - first_v ) >> 1 ) );

          p = last->next;
          if ( p->v < last_v )
            segment->height = (FT_Short)( segment->height +
                                          ( ( last_v - p->v ) >> 1 ) );
        }
      }
    }

  Exit:
    return error;
  }


  /* ----------------------------------------------------------------------- */


  #define SCALED_CONSTANT( analysis, c )                      \
  ( ( (c) * (FT_Long)analysis->face->units_per_EM ) / 2048 )


  static void
  _analysis_link_segments( Analysis analysis, FT_Pos linking_limit )
  {
    Segment       segments      = analysis->segments;
    Segment       segment_limit = segments + analysis->num_segments;
    FT_Pos        len_threshold, len_score;
    Segment       seg1, seg2;


    /* a heuristic value to set up a minimum value for overlapping */
    len_threshold = SCALED_CONSTANT( analysis, 8 );
    if( len_threshold == 0 )
      len_threshold = 1;

    /* a heuristic value to weight lengths */
    len_score = SCALED_CONSTANT( analysis, 6000 );

    /* now compare each segment to the others */
    for( seg1 = segments; seg1 < segment_limit; seg1++ )
    {
      if( seg1->dir != analysis->y_major_dir )
        continue;

      /* search for stems having opposite directions, */
      /* with seg1 to the `left' of seg2              */
      for( seg2 = segments; seg2 < segment_limit; seg2++ )
      {
        FT_Pos  pos1 = seg1->pos;
        FT_Pos  pos2 = seg2->pos;


        if( seg1->dir + seg2->dir == 0 && pos2 > pos1 )
        {
          /* compute distance between the two segments */
          FT_Pos  min = seg1->min_coord;
          FT_Pos  max = seg1->max_coord;
          FT_Pos  len;


          if( min < seg2->min_coord )
            min = seg2->min_coord;

          if( max > seg2->max_coord )
            max = seg2->max_coord;

          /* compute maximum coordinate difference of the two segments */
          /* (this is, how much they overlap)                          */
          len = max - min;
          if( len >= len_threshold )
          {
            /*
             *  The score is the sum of two demerits indicating the
             *  `badness' of a fit, measured along the segments' main axis
             *  and orthogonal to it, respectively.
             *
             *  o The less overlapping along the main axis, the worse it
             *    is, causing a larger demerit.
             *
             *  o The nearer the orthogonal distance to a stem width, the
             *    better it is, causing a smaller demerit.  For simplicity,
             *    however, we only increase the demerit for values that
             *    exceed the largest stem width.
             */

            FT_Pos  dist = pos2 - pos1;

            if( linking_limit > 0 && dist > linking_limit )
              continue;

            FT_Pos  dist_demerit, score;

            dist_demerit = dist; /* sticking with the default atm */

            score = dist_demerit + len_score / len;

            /* and we search for the smallest score */
            if( score < seg1->score )
            {
              seg1->score = score;
              seg1->link  = seg2;
            }

            if( score < seg2->score )
            {
              seg2->score = score;
              seg2->link  = seg1;
            }
          }
        }
      }
    }

    /* Compute serif segments */
    for( seg1 = segments; seg1 < segment_limit; seg1++ )
    {
      seg2 = seg1->link;

      if( seg2 )
      {
        if( seg2->link != seg1 )
        {
          seg1->link  = 0;
          seg1->serif = seg2->link;
        }
      }
    }
  }


  /* ----------------------------------------------------------------------- */


  static FT_Error
  _analysis_compute_edges( Analysis analysis )
  {
    FT_Error      error  = FT_Err_Ok;

    Segment       segments      = analysis->segments;
    Segment       segment_limit = segments + analysis->num_segments;
    Segment       seg;

    FT_Fixed      scale;
    FT_Pos        edge_distance_threshold;
    FT_Pos        segment_length_threshold;
    FT_Pos        segment_width_threshold;


    analysis->num_edges = 0;

    scale = analysis->y_scale;

    /* Vertical ignored... */
    segment_length_threshold = 0;

    /*
     *  Similarly, we ignore segments that have a width delta
     *  larger than 0.5px (i.e., a width larger than 1px).
     */
    segment_width_threshold = FT_DivFix( 32, scale );

    /*********************************************************************/
    /*                                                                   */
    /* We begin by generating a sorted table of edges for the current    */
    /* direction.  To do so, we simply scan each segment and try to find */
    /* an edge in our table that corresponds to its position.            */
    /*                                                                   */
    /* If no edge is found, we create and insert a new edge in the       */
    /* sorted table.  Otherwise, we simply add the segment to the edge's */
    /* list which gets processed in the second step to compute the       */
    /* edge's properties.                                                */
    /*                                                                   */
    /* Note that the table of edges is sorted along the segment/edge     */
    /* position.                                                         */
    /*                                                                   */
    /*********************************************************************/

    /* assure that edge distance threshold is at most 0.25px */
    #if 0
    edge_distance_threshold = FT_MulFix( laxis->edge_distance_threshold,
                                         scale );
    if ( edge_distance_threshold > 64 / 4 )
    #endif
      edge_distance_threshold = 64 / 4;

    edge_distance_threshold = FT_DivFix( edge_distance_threshold,
                                         scale );

    for( seg = segments; seg < segment_limit; seg++ )
    {
      Edge  found = NULL;
      FT_Int   ee;


      /* ignore too short segments, too wide ones, and, in this loop, */
      /* one-point segments without a direction                       */
      if( seg->height < segment_length_threshold ||
          seg->delta > segment_width_threshold   ||
          seg->dir == DIR_NONE                   )
        continue;

      /* A special case for serif edges: If they are smaller than */
      /* 1.5 pixels we ignore them.                               */
      if( seg->serif                                     &&
          2 * seg->height < 3 * segment_length_threshold )
        continue;

      /* look for an edge corresponding to the segment */
      for( ee = 0; ee < analysis->num_edges; ee++ )
      {
        Edge     edge = analysis->edges + ee;
        FT_Pos   dist;


        dist = seg->pos - edge->fpos;
        if( dist < 0 )
          dist = -dist;

        if( dist < edge_distance_threshold && edge->dir == seg->dir )
        {
          found = edge;
          break;
        }
      }

      if( !found )
      {
        Edge  edge;


        /* insert a new edge in the list and */
        /* sort according to the position    */
        error = _analysis_new_edge( analysis, seg->pos,
        	                        (Direction)seg->dir, &edge );
        if( error )
          goto Exit;

        /* add the segment to the new edge's list */
        FT_ZERO( edge );

        edge->first    = seg;
        edge->last     = seg;
        edge->dir      = seg->dir;
        edge->fpos     = seg->pos;
        edge->opos     = FT_MulFix( seg->pos, scale );
        edge->pos      = edge->opos;
        seg->edge_next = seg;
      }
      else
      {
        /* if an edge was found, simply add the segment to the edge's */
        /* list                                                       */
        seg->edge_next         = found->first;
        found->last->edge_next = seg;
        found->last            = seg;
      }
    }

    /* we loop again over all segments to catch one-point segments   */
    /* without a direction: if possible, link them to existing edges */
    for( seg = segments; seg < segment_limit; seg++ )
    {
      Edge     found = NULL;
      FT_Int   ee;


      if( seg->dir != DIR_NONE )
        continue;

      /* look for an edge corresponding to the segment */
      for( ee = 0; ee < analysis->num_edges; ee++ )
      {
        Edge  edge = analysis->edges + ee;
        FT_Pos   dist;


        dist = seg->pos - edge->fpos;
        if ( dist < 0 )
          dist = -dist;

        if( dist < edge_distance_threshold )
        {
          found = edge;
          break;
        }
      }

      /* one-point segments without a match are ignored */
      if( found )
      {
        seg->edge_next         = found->first;
        found->last->edge_next = seg;
        found->last            = seg;
      }
    }


    /******************************************************************/
    /*                                                                */
    /* Good, we now compute each edge's properties according to the   */
    /* segments found on its position.  Basically, these are          */
    /*                                                                */
    /*  - the edge's main direction                                   */
    /*  - stem edge, serif edge or both (which defaults to stem then) */
    /*  - rounded edge, straight or both (which defaults to straight) */
    /*  - link for edge                                               */
    /*                                                                */
    /******************************************************************/

    /* first of all, set the `edge' field in each segment -- this is */
    /* required in order to compute edge links                       */

    /*
     * Note that removing this loop and setting the `edge' field of each
     * segment directly in the code above slows down execution speed for
     * some reasons on platforms like the Sun.
     */
    {
      Edge  edges      = analysis->edges;
      Edge  edge_limit = edges + analysis->num_edges;
      Edge  edge;


      for( edge = edges; edge < edge_limit; edge++ )
      {
        seg = edge->first;
        if( seg )
          do
          {
            seg->edge = edge;
            seg       = seg->edge_next;

          } while( seg != edge->first );
      }

      /* now compute each edge properties */
      for( edge = edges; edge < edge_limit; edge++ )
      {
        FT_Int  is_round    = 0;  /* does it contain round segments?    */
        FT_Int  is_straight = 0;  /* does it contain straight segments? */


        seg = edge->first;

        do
        {
          FT_Bool  is_serif;


          /* check for roundness of segment */
          if ( seg->flags & EDGE_ROUND )
            is_round++;
          else
            is_straight++;

          /* check for links -- if seg->serif is set, then seg->link must */
          /* be ignored                                                   */
          is_serif = (FT_Bool)( seg->serif               &&
                                seg->serif->edge         &&
                                seg->serif->edge != edge );

          if( ( seg->link && seg->link->edge != NULL ) || is_serif )
          {
            Edge     edge2;
            Segment  seg2;


            edge2 = edge->link;
            seg2  = seg->link;

            if( is_serif )
            {
              seg2  = seg->serif;
              edge2 = edge->serif;
            }

            if( edge2 )
            {
              FT_Pos  edge_delta;
              FT_Pos  seg_delta;


              edge_delta = edge->fpos - edge2->fpos;
              if( edge_delta < 0 )
                edge_delta = -edge_delta;

              seg_delta = seg->pos - seg2->pos;
              if( seg_delta < 0 )
                seg_delta = -seg_delta;

              if( seg_delta < edge_delta )
                edge2 = seg2->edge;
            }
            else
              edge2 = seg2->edge;

            if( is_serif )
            {
              edge->serif   = edge2;
              edge2->flags |= EDGE_SERIF;
            }
            else
              edge->link  = edge2;
          }

          seg = seg->edge_next;

        } while( seg != edge->first );

        /* set the round/straight flags */
        edge->flags = EDGE_NORMAL;

        if( is_round > 0 && is_round >= is_straight )
          edge->flags |= EDGE_ROUND;

        /* get rid of serifs if link is set                 */
        /* XXX: This gets rid of many unpleasant artefacts! */
        /*      Example: the `c' in cour.pfa at size 13     */

        if( edge->serif && edge->link )
          edge->serif = NULL;
      }
    }

  Exit:
    return error;
  }


  /* ----------------------------------------------------------------------- */


  static FT_Pos
  _calculate_stem_linking_limit( Analysis analysis, FT_Face face )
  {
    FT_CharMap oldmap = face->charmap;
    FT_Pos max_width = 0;

    if ( !FT_Select_Charmap( face, FT_ENCODING_UNICODE ) )
    {
      FT_Error error;
      FT_Char *width_chars = "oO0";

      for( FT_Char *p = width_chars; *p != 0; p++ )
      {
        error = FT_Load_Char( face, *p, FT_LOAD_NO_SCALE );

        if ( error || face->glyph->outline.n_points <= 2 )
          continue;

        _analysis_reload( analysis, face );
        _analysis_compute_segments( analysis );
        _analysis_link_segments( analysis, 0 );

        Segment segments      = analysis->segments;
        Segment segment_limit = segments + analysis->num_segments;

        for( Segment seg = segments; seg < segment_limit; seg++ )
        {
          Segment linked = seg->link;

          if( linked )
          {
            FT_Pos dist = seg->pos - linked->pos;
            dist = ( dist < 0 ) ? -dist : dist;

            if( dist > max_width )
              max_width = dist;
          }
        }
      }
    }

    FT_Set_Charmap( face, oldmap );

    return max_width * 3;
  }


  /* ----------------------------------------------------------------------- */


  static FT_Pos
  _calculate_alignment_height( FT_Face face, FT_Char *height_chars )
  {
    FT_Pos      height  = 0;
    FT_CharMap  oldmap  = face->charmap;

    if ( !FT_Select_Charmap( face, FT_ENCODING_UNICODE ) )
    {
      FT_Error    error;
      FT_Outline  outline;

      FT_Int      count = 0;


      /* Calculate an average max y height for each char. */
      for( FT_Char *p = height_chars; *p != 0; p++ )
      {
        error = FT_Load_Char( face, *p, FT_LOAD_NO_SCALE );

        if ( error || face->glyph->outline.n_points <= 2 )
          continue;

        outline = face->glyph->outline;

        FT_Int c;
        FT_Int first  = 0;
        FT_Int last   = -1;
        FT_Pos best_y = FT_INT_MIN;

        /* Find max y height in glyph contours */
        for( c = 0; c < outline.n_contours; first = last + 1, c++ )
        {
          last = outline.contours[c];

          /* Avoid contours that won't be rendered. */
          if( last <= first + 1 )
            continue;

          FT_Vector *point       = outline.points + first;
          FT_Vector *point_last  = outline.points + last;

          for( ; point <= point_last; point++ )
            if ( point->y > best_y )
              best_y = point->y;
        }

        if( best_y <= 0 )
          continue; /* We'll skip glyphs under the origin atm. */

        /* Recalculate the average. */
        height = ( height * count + best_y ) / ++count;
      }
    }

    FT_Set_Charmap( face, oldmap );
    return height;
  }


  /* ----------------------------------------------------------------------- */


  static void
  _analysis_position_edges( Analysis  analysis,
                            FT_Pos    cap_height_org,
                            FT_Pos    cap_height_fit,
                            FT_Pos    x_height_org,
                            FT_Pos    x_height_fit )
  {
    FT_Pos  cap_height_delta = cap_height_fit - cap_height_org;
    FT_Pos  x_height_delta   = x_height_fit - x_height_org;

    Edge edge_limit = analysis->edges + analysis->num_edges;

    /* Position stems (linked edges) first */
    for( Edge edge = analysis->edges; edge < edge_limit; edge++ )
    {
      if( !edge->link || edge->link > edge )
        continue;

      if( edge->link->opos <= 0 )
      {
        /* Stem contains the origin. If it also contains the cap height or */
        /* x-height then add the respective delta to the upper edge.       */
        /* Otherwise keep the same positions.                              */

        if( edge->opos >= cap_height_org )
          edge->pos += cap_height_delta;

        else if( edge->opos >= x_height_org )
          edge->pos += x_height_delta;

        /* else                                                          */
        /*   the edge is at or below the origin and no change is needed. */
      }

      else if( edge->opos >= cap_height_org )
      {
        /*
         * The top edge is above the cap height meaning that the bottom edge
         * will be either shifted with the top edge in the case of a stem that's
         * above or containing the cap height, or shifted by the x-height if the
         * stem contains the x-height as well.
         */
        edge->link->pos += ( edge->link->opos <= x_height_org )
                           ? x_height_delta
                           : cap_height_delta;

        edge->pos += cap_height_delta;
      }

      else
      {
        /* Otherwise the stem is between the origin and cap height so it is */
        /* interpolated between the relevant positions.                     */

        FT_Pos top_org, top_fit, bottom_org, bottom_fit;
        FT_Pos stem_pos_org, stem_pos_fit, delta;

        stem_pos_org = ( edge->opos + edge->link->opos ) / 2;

        if( stem_pos_org < x_height_org )
        {
          top_org    = x_height_org;
          top_fit    = x_height_fit;
          bottom_org = 0;
          bottom_fit = 0;
        }
        else
        {
          top_org    = cap_height_org;
          top_fit    = cap_height_fit;
          bottom_org = x_height_org;
          bottom_fit = x_height_fit;
        }

        stem_pos_fit = FT_MulDiv( stem_pos_org - bottom_org,
                                  top_fit - bottom_fit,
                                  top_org - bottom_org ) + bottom_fit;

        delta = stem_pos_fit - stem_pos_org;

        edge->pos += delta;
        edge->link->pos += delta;
      }

      edge->flags       |= EDGE_DONE;
      edge->link->flags |= EDGE_DONE;
    }


    /* Now do the rest of the edges */
    for( Edge edge = analysis->edges; edge < edge_limit; edge++ )
    {
      if( edge->flags & EDGE_DONE )
        continue;

      /* Serif edges just get their corresponding opposite edge's offset */
      if( edge->serif )
        edge->pos += edge->serif->pos - edge->serif->opos;

      /* Above the alignment height so it get's the full offset */
      else if( edge->opos >= cap_height_org )
        edge->pos += cap_height_delta;

      /* Inside alignment zone. Align in proportion to other finished edges */
      else if( edge->opos > 0 )
      {
        Edge prev_done = NULL;
        Edge next_done = NULL;

        for( Edge e = edge - 1; e >= analysis->edges && !prev_done; e-- )
          if( e->flags & EDGE_DONE )
            prev_done = e;

        for( Edge e = edge + 1; e < edge_limit && !next_done; e++ )
          if( e->flags & EDGE_DONE )
            next_done = e;

        FT_Pos prev_org, next_org, prev_fit, next_fit;

        if( prev_done )
        {
          prev_org = prev_done->opos;
          prev_fit = prev_done->pos;
        }
        else
          prev_org = prev_fit = 0;

        if( next_done )
        {
          next_org = next_done->opos;
          next_fit = next_done->pos;
        }
        else
        {
          next_org = cap_height_org;
          next_fit = cap_height_fit;
        }

        edge->pos = FT_MulDiv( edge->opos - prev_org,
                               next_fit - prev_fit,
                               next_org - prev_org ) + prev_fit;
      }

      /* else                                                         */
      /*   the edge is at or below the origin and no change is needed */

      edge->flags |= EDGE_DONE;
    }
  }


  /* ----------------------------------------------------------------------- */


  static void
  _analysis_adjust_edge_points( Analysis analysis )
  {
    Segment seg_limit = analysis->segments + analysis->num_segments;

    for( Segment seg = analysis->segments; seg < seg_limit; seg++ )
    {
      if( !seg->edge )
        continue;

      FT_Pos delta = seg->edge->pos - seg->edge->opos;

      Point p = seg->first;

      for( ;; )
      {
        p->y     += delta;
        p->flags |= POINT_FLAG_DONE;

        if( p == seg->last )
          break;
        else
          p = p->next;
      }
    }
  }


  /* ----------------------------------------------------------------------- */


  static void
  _analysis_adjust_strong_points( Analysis analysis,
                                  FT_Pos height_org,
                                  FT_Pos height_fit )
  {
    FT_Pos height_delta = height_fit - height_org;

    Point point_limit = analysis->points + analysis->num_points;

    for( Point point = analysis->points; point < point_limit; point++ )
    {
      if( point->flags & POINT_FLAG_WEAK_INTERPOLATION ||
          point->flags & POINT_FLAG_DONE )
        continue;

      /* Simple case, point is above the alignment zone. */
      if( point->oy >= height_org )
        point->y += height_delta;

      /*
       * Point is inside the alignment zone.
       * Adjust between two reference positions that have been adjusted
       * already. By default the zone top and bottom (origin and alignment
       * height) are used unless a closer edge exists between them and the
       * point.
       */
      else if( point->oy > 0 )
      {
        FT_Pos before_org, after_org, before_fit, after_fit;

        before_org = 0;
        before_fit = 0;
        after_org  = height_org;
        after_fit  = height_fit;

        if( analysis->num_edges > 0 )
        {
          Edge edges = analysis->edges;
          Edge lower = edges;
          Edge upper = edges + analysis->num_edges;
          Edge last  = upper - 1;

          /* Simple case, the point is above all the edges. */
          if( point->oy > last->opos )
            lower = upper;

          /*
           * Point is inside the range of edges. Find the first edge higher
           * than the point. A binary search is used for more than 8 edges.
           */
          else if( point->oy >= lower->opos )
          {
            if( analysis->num_edges > 8 )
            {
              while( lower < upper )
              {
                Edge middle = ( ( upper - lower ) >> 1 ) + lower;

                if( point->oy > middle->opos )
                  lower = middle + 1;
                else
                  upper = middle;
              }
            }

            else
            {
              for( ; lower < upper; lower++ )
                if( point->oy <= lower->opos )
                  break;
            }
          }

          /*
           * Adjust references as needed.
           */

          if( lower == last + 1 )
          {
            if( last->opos > 0 )
            {
              before_org = last->opos;
              before_fit = last->pos;
            }
          }

          else if( lower == edges )
          {
            if( edges->opos < height_org )
            {
              after_org = edges->opos;
              after_fit = edges->pos;
            }
          }

          else
          {
            Edge before = lower - 1;

            if( before->opos > 0 )
            {
              before_org = before->opos;
              before_fit = before->pos;
            }

            if( lower->opos < height_org )
            {
              after_org = lower->opos;
              after_fit = lower->pos;
            }
          }
        }

        /* Now adjust the point to its new position. */
        point->y = FT_MulDiv( point->oy - before_org,
                              after_fit - before_fit,
                              after_org - before_org ) + before_fit;
      }

      /* else                                                          */
      /*   the point is at or below the origin and no change is needed */

      point->flags |= POINT_FLAG_DONE;
    }
  }


  /* ----------------------------------------------------------------------- */


  static void
  _shift_range( Point start, Point end, Point ref )
  {
    FT_Pos delta = ref->y - ref->oy;

    if( delta == 0 )
      return;

    for( Point p = start; p < ref; p++ )
      p->y = p->oy + delta;

    for( Point p = ref + 1; p <= end; p++ )
      p->y = p->oy + delta;
  }


  /* ----------------------------------------------------------------------- */


  static void
  _interpolate_range( Point start, Point end, Point ref1, Point ref2 )
  {
    if( start > end )
      return;

    /* Keep ref1 smaller than ref2 to simplfy */
    if( ref1->oy > ref2->oy )
    {
      Point tmp;
      tmp  = ref1;
      ref1 = ref2;
      ref2 = tmp;
    }

    FT_Pos delta1 = ref1->y - ref1->oy;
    FT_Pos delta2 = ref2->y - ref2->oy;

    /* Avoid division/multiplication by 0; */
    /* second condition likely redundant.  */
    if( ref1->y == ref2->y || ref1->oy == ref2->oy )
    {
      for( Point p = start; p <= end; p++ )
      {
        if( p->oy <= ref1->oy )
          p->y += delta1;
        else if( p->oy >= ref2->oy )
          p->y += delta2;
        else
          p->y = ref1->y;
      }
    }

    /* Interpolate between the references as needed. */
    else
    {
      FT_Fixed  scale = FT_DivFix( ref2->y - ref1->y, ref2->oy - ref1->oy );

      for( Point p = start; p <= end; p++ )
      {
        if( p->oy <= ref1->oy )
          p->y += delta1;
        else if( p->oy >= ref2->oy )
          p->y += delta2;
        else
          p->y = ref1->y + FT_MulFix( p->oy - ref1->oy, scale );
      }
    }
  }


  /* ----------------------------------------------------------------------- */


  static void
  _analysis_adjust_weak_points( Analysis analysis )
  {
    Point   points        = analysis->points;
    Point   point_limit   = points + analysis->num_points;
    Point*  contour       = analysis->contours;
    Point*  contour_limit = contour + analysis->num_contours;

    Point point, end_point, first_point;

    for( ; contour < contour_limit; contour++ )
    {
      Point first_done, last_done;

      point       = *contour;
      end_point   =  point->prev;
      first_point =  point;

      /* find first adjusted point */
      for( ; point <= end_point; point++ )
        if( point->flags & POINT_FLAG_DONE )
          break;

      /* if no adjusted points in contour, skip */
      if( point > end_point )
        continue;

      first_done = point;


      for( ; point <= end_point; point++ )
      {
        /* find the next unadjusted point */
        for( ; point <= end_point; point++ )
          if( !( point->flags & POINT_FLAG_DONE ) )
            break;

        last_done = point - 1;

        /* find the next adjusted point, if any */
        for( ; point <= end_point; point++ )
          if( point->flags & POINT_FLAG_DONE )
            break;

        /* We've reached the end of the contour's points */
        if( point > end_point )
          break;

        /* interpolate between last_done and point */
        _interpolate_range( last_done + 1, point - 1,
                            last_done, point );
      }


      /* If the end point of the contour is adjusted then it might be needed */
      /* to adjust the points at the start of the contour                    */
      if( end_point->flags & POINT_FLAG_DONE )
        last_done = end_point;

      /*
       * Reached the end of the contour. Now finish adjusting it.
       *
       * If the contour contained only one adjusted point, just shift
       * the remaining points.
       *
       * Otherwise interpolate the range between the last and first
       * adjusted points.
       */

      if ( last_done == first_done )
      {
        _shift_range( first_point, end_point, first_done );
        continue;
      }


      if ( last_done < end_point )
        _interpolate_range( last_done + 1, end_point,
                            last_done, first_done );

      if ( first_done > points )
        _interpolate_range( first_point, first_done - 1,
                            last_done, first_done );
    }
  }


  /***************************************************************************/


  /* ----------------------------------------------------------------------- *\
   *
   *                          == Public Interface ==
   *
  \* ----------------------------------------------------------------------- */

  static AnalysisRec _analysisrec;
  static Analysis    _analysis;


  void
  analysis_init()
  {
    memset( &_analysisrec, 0, sizeof(AnalysisRec) );
    _analysis = &_analysisrec;
  }


  void
  analysis_load_hinted()
  {

    FT_Fixed  y_scale;
    FT_Pos    limit;
    FT_Pos    cap_height, cap_height_scaled, cap_height_fitted;
    FT_Pos    x_height, x_height_scaled, x_height_fitted;

    y_scale = globals.face->size->metrics.y_scale;

    cap_height = _calculate_alignment_height( globals.face, "OTHEZD" );
    x_height = _calculate_alignment_height( globals.face, "aeocsz" );
    limit = _calculate_stem_linking_limit( _analysis, globals.face );

    /* Round up to next px if in top 2/3 otherwise round down. */
    cap_height_scaled = FT_MulFix( cap_height, y_scale );
    cap_height_fitted = ( cap_height_scaled + 43 ) & ~63;

    x_height_scaled = FT_MulFix( x_height, y_scale );
    x_height_fitted = ( x_height_scaled + 43 ) & ~63;

    if( FT_Load_Glyph( globals.face, globals.glyph_index, FT_LOAD_NO_SCALE ) )
      panic( "analysis_detect_face_edges: Couldn't load glyph index: %d",
             globals.glyph_index );

    _analysis_reload( _analysis, globals.face );
    _analysis_compute_segments( _analysis );
    _analysis_link_segments( _analysis, limit );
    _analysis_compute_edges( _analysis );
    _analysis_position_edges( _analysis, cap_height_scaled, cap_height_fitted,
                              x_height_scaled, x_height_fitted );
    _analysis_adjust_edge_points( _analysis );
    _analysis_adjust_strong_points( _analysis, cap_height_scaled, cap_height_fitted );
    _analysis_adjust_weak_points( _analysis );
    _analysis_write_glyph( _analysis );
  }


  void
  analysis_detect_face_edges()
  {
    FT_Pos limit = _calculate_stem_linking_limit( _analysis, globals.face );

    if( FT_Load_Glyph( globals.face, globals.glyph_index, FT_LOAD_NO_SCALE ) )
      panic( "analysis_detect_face_edges: Couldn't load glyph index: %d",
             globals.glyph_index );

    _analysis_reload( _analysis, globals.face );
    _analysis_compute_segments( _analysis );
    _analysis_link_segments( _analysis, limit );
    _analysis_compute_edges( _analysis );
  }


  void
  analysis_draw_face_edges( cairo_t *cr )
  {
    cairo_matrix_t coord_matrix;

    ViewerColor cu = globals.edge_unlinked_color;
    ViewerColor cl = globals.edge_linked_color;
    ViewerColor cs = globals.edge_serif_color;

    /* Store the set transformation for later */
    cairo_get_matrix( cr, &coord_matrix );

    Edge edge_limit = _analysis->edges + _analysis->num_edges;

    for( Edge edge = _analysis->edges; edge < edge_limit; edge++ )
    {
      /* Figure out the edge min and max coords */
      Segment   seg       = edge->first;
      FT_Short  min_coord = seg->min_coord;
      FT_Short  max_coord = seg->max_coord;

      do
      {
        seg = seg->edge_next;

        if( seg->max_coord > max_coord )
          max_coord = seg->max_coord;

        if( seg->min_coord < min_coord )
          min_coord = seg->min_coord;
      }
      while( seg != edge->last );

      /* Draw the edge line */
      {
        double min_x = FT_MulFix( min_coord, _analysis->x_scale );
        double max_x = FT_MulFix( max_coord, _analysis->x_scale );

        if( edge->link )
          cairo_set_source_rgba( cr, cl.red, cl.green, cl.blue, cl.alpha );
        else if( edge->serif )
          cairo_set_source_rgba( cr, cs.red, cs.green, cs.blue, cs.alpha );
        else
          cairo_set_source_rgba( cr, cu.red, cu.green, cu.blue, cu.alpha );

        cairo_move_to( cr, min_x, edge->opos );
        cairo_line_to( cr, max_x, edge->opos );

        /* Avoid Scaling the Line Width */
        cairo_identity_matrix( cr );
        cairo_stroke( cr );
        cairo_set_matrix( cr, &coord_matrix );
      }
    }
  }