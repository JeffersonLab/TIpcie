#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

  typedef struct
  {
    int32_t enable;
    int32_t prescale;
    int32_t delay;
  } ts_inputs_t;

  typedef struct
  {
    int32_t window;
    int32_t timestep;
    int32_t minimum;
  } trigger_rules_t;

  typedef struct
  {
    int32_t swa;
    int32_t swb;
    int32_t p2;
    int32_t fp_tdc;
    int32_t fp_fadc;
    int32_t fp;
    int32_t loopback;
    int32_t fiber[8];
  } busy_source_t;

  typedef struct
  {
    int32_t crate_id;
    int32_t block_level;
    int32_t block_buffer_level;
    int32_t instant_block_level_enable;
    int32_t broadcast_buffer_level_enable;
    int32_t block_limit;
    int32_t trigger_source;
    int32_t sync_source;
    busy_source_t busy_source;
    int32_t clock_source;

    int32_t prescale;
    int32_t event_format;
    int32_t fp_input_readout_enable;
    int32_t go_output_enable;

    int32_t trigger_window;
    int32_t trigger_inhibit_window;
    int32_t trigger_latch_on_level_enable;

    int32_t trigger_output_delay;
    int32_t trigger_output_delaystep;
    int32_t trigger_output_width;

    int32_t prompt_trigger_width;

    int32_t syncreset_delay;
    int32_t syncreset_width;
    int32_t syncreset_widthstep;

    int32_t eventtype_scalers_enable;
    int32_t scaler_mode;
    int32_t syncevent_interval;

    int32_t trigger_table;

    int32_t fixed_pulser_eventtype;
    int32_t random_pulser_eventtype;

    int32_t fiber_enable[8];

    ts_inputs_t ts_inputs[6];

    trigger_rules_t trigger_rules[4];

  } ti_param_t;

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
