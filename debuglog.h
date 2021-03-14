#ifdef VERBOSE_DEBUG_LOGBUFFER
#define LOGBUFFER_DEBUG(M, V) (Serial << M << V);
#define LOGBUFFER_DEBUGN(M, V) (Serial << M << V << endl);
#else
#define LOGBUFFER_DEBUG(M, V) ;
#define LOGBUFFER_DEBUGN(M, V) ;
#endif
