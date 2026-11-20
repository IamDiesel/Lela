#pragma once
// PC-Stub: Der Test laeuft in einem einzigen Thread, die Sperre zaehlt nur mit,
// damit der Test "ausgeglichen betreten/verlassen" pruefen kann.
typedef struct { int depth; } portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED  { 0 }
extern int g_stubCriticalDepth;
#define portENTER_CRITICAL(mux)  do { (mux)->depth++; g_stubCriticalDepth++; } while (0)
#define portEXIT_CRITICAL(mux)   do { (mux)->depth--; g_stubCriticalDepth--; } while (0)
