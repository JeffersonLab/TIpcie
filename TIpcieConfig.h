#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
  /* routine prototypes */
  int32_t tiConfigInitGlobals();
  int32_t tiConfig(const char *filename);
  int32_t tiConfigFree();
  void    tiConfigPrintParameters();
  int32_t tipConfigGetIntParameter(const char* param, int32_t *value);

  int32_t tipConfigEnablePulser();
  int32_t tipConfigDisablePulser();

  int32_t writeIni(const char *filename);
#ifdef __cplusplus
}
#endif
