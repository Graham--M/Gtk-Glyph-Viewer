#ifndef UTILS_H_
#define UTILS_H_


/*
 * Wrap an operation in a pair of save/restore calls. Would be nice if there
 * was a general reset function that could be called instead of having to save
 * the default state.
 */
#define RESTORE_AFTER( cr, operation ) \
  do {                                 \
  	cairo_save( cr );                  \
    operation;                         \
    cairo_restore( cr );               \
  } while( 0 )


  void
  panic( const char*  fmt, ... );


#endif /* UTILS_H_ */

/* END */