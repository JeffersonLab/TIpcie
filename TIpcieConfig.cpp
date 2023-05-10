#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include "TIpcieConfig.h"
#include "INIReader.h"

#ifdef __cplusplus
extern "C" {
#include "TIpcieLib.h"
}
#endif


// place to store the ini INIReader instance
INIReader *ir;

typedef std::map<std::string, int32_t> ti_param_map;

const ti_param_map ti_general_def
  {
    { "CRATE_ID", -1 },
    { "BLOCK_LEVEL", -1 },
    { "BLOCK_BUFFER_LEVEL", -1 },

    { "INSTANT_BLOCKLEVEL_ENABLE", -1 },
    { "BROADCAST_BUFFER_LEVEL_ENABLE", -1 },

    { "BLOCK_LIMIT", -1 },

    { "TRIGGER_SOURCE", -1 },

    { "SYNC_SOURCE", -1 },
    { "SYNC_RESET_TYPE", -1 },

    { "BUSY_SOURCE_SWA", -1 },
    { "BUSY_SOURCE_SWB", -1 },
    { "BUSY_SOURCE_P2", -1 },
    { "BUSY_SOURCE_FP_TDC", -1 },
    { "BUSY_SOURCE_FP_ADC", -1 },
    { "BUSY_SOURCE_FP", -1 },
    { "BUSY_SOURCE_LOOPBACK", -1 },
    { "BUSY_SOURCE_FIBER1", -1 },
    { "BUSY_SOURCE_FIBER2", -1 },
    { "BUSY_SOURCE_FIBER3", -1 },
    { "BUSY_SOURCE_FIBER4", -1 },
    { "BUSY_SOURCE_FIBER5", -1 },
    { "BUSY_SOURCE_FIBER6", -1 },
    { "BUSY_SOURCE_FIBER7", -1 },
    { "BUSY_SOURCE_FIBER8", -1 },

    { "CLOCK_SOURCE", -1 },

    { "PRESCALE", -1 },
    { "EVENT_FORMAT", -1 },
    { "FP_INPUT_READOUT_ENABLE", -1 },

#ifdef DNE
    { "GO_OUTPUT_ENABLE", -1 },
#endif

    { "TRIGGER_WINDOW", -1 },
    { "TRIGGER_INHIBIT_WINDOW", -1 },

    { "TRIGGER_LATCH_ON_LEVEL_ENABLE", -1 },

    { "TRIGGER_OUTPUT_DELAY", -1 },
    { "TRIGGER_OUTPUT_DELAYSTEP", -1 },
    { "TRIGGER_OUTPUT_WIDTH", -1 },

    { "PROMPT_TRIGGER_WIDTH", -1 },

    { "SYNCRESET_DELAY", -1 },
    { "SYNCRESET_WIDTH", -1 },
    { "SYNCRESET_WIDTHSTEP", -1 },

#ifdef DNE
    { "EVENTTYPE_SCALERS_ENABLE", -1 },
    { "SCALER_MODE", -1 },
#endif
    { "SYNCEVENT_INTERVAL", -1 },

    { "TRIGGER_TABLE", -1 },

    { "FIXED_PULSER_EVENTTYPE", -1 },
    { "RANDOM_PULSER_EVENTTYPE", -1 },

#ifdef DNE
    { "FIBER_SYNC_DELAY", -1 }
#endif
  };
static ti_param_map ti_general_ini = ti_general_def, ti_general_readback = ti_general_def;

const ti_param_map ti_slaves_def
  {
    { "ENABLE_FIBER_1", -1},
  };
static ti_param_map ti_slaves_ini = ti_slaves_def, ti_slaves_readback = ti_slaves_def;

const ti_param_map ti_tsinputs_def
  {
    { "ENABLE_TS1", -1 },
    { "ENABLE_TS2", -1 },
    { "ENABLE_TS3", -1 },
    { "ENABLE_TS4", -1 },
    { "ENABLE_TS5", -1 },
    { "ENABLE_TS6", -1 },

    { "PRESCALE_TS1", -1 },
    { "PRESCALE_TS2", -1 },
    { "PRESCALE_TS3", -1 },
    { "PRESCALE_TS4", -1 },
    { "PRESCALE_TS5", -1 },
    { "PRESCALE_TS6", -1 },

    { "DELAY_TS1", -1 },
    { "DELAY_TS2", -1 },
    { "DELAY_TS3", -1 },
    { "DELAY_TS4", -1 },
    { "DELAY_TS5", -1 },
    { "DELAY_TS6", -1 }
  };
static ti_param_map ti_tsinputs_ini = ti_tsinputs_def, ti_tsinputs_readback = ti_tsinputs_def;

const ti_param_map ti_rules_def =
  {
    { "RULE_1", -1 },
    { "RULE_TIMESTEP_1", -1 },
    { "RULE_MIN_1", -1 },
    { "RULE_2", -1 },
    { "RULE_TIMESTEP_2", -1 },
    { "RULE_MIN_2", -1 },
    { "RULE_3", -1 },
    { "RULE_TIMESTEP_3", -1 },
    { "RULE_MIN_3", -1 },
    { "RULE_4", -1 },
    { "RULE_TIMESTEP_4", -1 },
    { "RULE_MIN_4", -1 },
  };
static ti_param_map ti_rules_ini = ti_rules_def, ti_rules_readback = ti_rules_def;

const ti_param_map ti_pulser_def =
  {
    { "FIXED_ENABLE", -1 },
    { "FIXED_NUMBER", -1 },
    { "FIXED_PERIOD", -1 },
    { "FIXED_RANGE", -1 },
    { "RANDOM_ENABLE", -1 },
    { "RANDOM_PRESCALE", -1 },
  };
static ti_param_map ti_pulser_ini = ti_pulser_def;



int32_t
tiConfigInitGlobals()
{
#ifdef DEBUG
  std::cout << __func__ << ": INFO: here" << std::endl;
#endif

  return 0;
}


/**
 * @brief Write the Ini values to the local ti_param_map's
 */
void
parseIni()
{
#ifdef DEBUG
  std::cout << __func__ << ": INFO: here" << std::endl;
#endif

  if(ir == NULL)
    return;

  ti_param_map::const_iterator pos = ti_general_def.begin();

  while(pos != ti_general_def.end())
    {
      ti_general_ini[pos->first] = ir->GetInteger("general", pos->first, pos->second);
      ++pos;
    }

  pos = ti_slaves_def.begin();
  while(pos != ti_slaves_def.end())
    {
      ti_slaves_ini[pos->first] = ir->GetInteger("slaves", pos->first, pos->second);
      ++pos;
    }

  pos = ti_tsinputs_def.begin();
  while(pos != ti_tsinputs_def.end())
    {
      ti_tsinputs_ini[pos->first] = ir->GetInteger("tsinputs", pos->first, pos->second);
      ++pos;
    }

  pos = ti_rules_def.begin();
  while(pos != ti_rules_def.end())
    {
      ti_rules_ini[pos->first] = ir->GetInteger("trigger_rules", pos->first, pos->second);
      ++pos;
    }

  pos = ti_pulser_def.begin();
  while(pos != ti_pulser_def.end())
    {
      ti_pulser_ini[pos->first] = ir->GetInteger("pulser", pos->first, pos->second);
      ++pos;
    }


}

/**
 * @brief Print the values stored in the local structure
 */
void
tiConfigPrintParameters()
{
#ifdef DEBUG
  std::cout << __func__ << ": INFO: HERE" << std::endl;
#endif

  ti_param_map::const_iterator pos = ti_general_ini.begin();

  printf("[general]\n");
  while(pos != ti_general_ini.end())
    {
      printf("  %28.24s = 0x%08x (%d)\n", pos->first.c_str(), pos->second, pos->second);
      ++pos;
    }

  pos = ti_slaves_ini.begin();

  printf("[slaves]\n");
  while(pos != ti_slaves_ini.end())
    {
      printf("  %28.24s = 0x%08x (%d)\n", pos->first.c_str(), pos->second, pos->second);
      ++pos;
    }

  pos = ti_tsinputs_ini.begin();

  printf("[tsinputs]\n");
  while(pos != ti_tsinputs_ini.end())
    {
      printf("  %28.24s = 0x%08x (%d)\n", pos->first.c_str(), pos->second, pos->second);
      ++pos;
    }

  pos = ti_rules_ini.begin();

  printf("[trigger rules]\n");
  while(pos != ti_rules_ini.end())
    {
      printf("  %28.24s = 0x%08x (%d)\n", pos->first.c_str(), pos->second, pos->second);
      ++pos;
    }

  pos = ti_pulser_ini.begin();

  printf("[pulser]\n");
  while(pos != ti_pulser_ini.end())
    {
      printf("  %28.24s = 0x%08x (%d)\n", pos->first.c_str(), pos->second, pos->second);
      ++pos;
    }


}

/**
 * @brief Write the ini parameters to the module
 * @return 0
 */
int32_t
param2ti()
{
#ifdef DEBUG
  std::cout << __func__ << ": INFO: here" << std::endl;
#endif

  int32_t param_val = 0, rval = OK;
  ti_param_map::const_iterator pos;

#define CHECK_PARAM(__ini, __key)					\
  pos = __ini.find(__key);						\
  if(pos == __ini.end()) {						\
    std::cerr << __func__ << ": ERROR finding " << __key << std::endl;	\
    return ERROR;							\
  }									\
  param_val = pos->second;

  /* Write the parameters to the device */
  CHECK_PARAM(ti_general_ini, "CRATE_ID");
  if(param_val > 0)
    {
#ifdef DEBUG
      printf("%s: Set Crate ID to %d\n", __func__, param_val);
#endif
      rval = tipSetCrateID(param_val);
      if(rval != OK)
	return ERROR;
    }

  CHECK_PARAM(ti_general_ini, "BLOCK_LEVEL");
  if(param_val > 0)
    {
      rval = tipSetBlockLevel(param_val);
      if(rval != OK)
	return ERROR;
    }

  CHECK_PARAM(ti_general_ini, "BLOCK_BUFFER_LEVEL");
  if(param_val > 0)
    {
      rval = tipSetBlockBufferLevel(param_val);
      if(rval != OK)
	return ERROR;
    }

  CHECK_PARAM(ti_general_ini, "INSTANT_BLOCKLEVEL_ENABLE");
  if(param_val > 0)
    {
      rval = tipSetInstantBlockLevelChange(param_val);
      if(rval != OK)
	return ERROR;
    }

  CHECK_PARAM(ti_general_ini, "BROADCAST_BUFFER_LEVEL_ENABLE");
  if(param_val > 0)
    {
      rval = tipUseBroadcastBufferLevel(param_val);
      if(rval != OK)
	return ERROR;
    }

  CHECK_PARAM(ti_general_ini, "BLOCK_LIMIT");
  if(param_val > 0)
    {
      rval = tipSetBlockLimit(param_val);
      if(rval != OK)
	return ERROR;
    }

  CHECK_PARAM(ti_general_ini, "TRIGGER_SOURCE");
  if(param_val > 0)
    {
      rval = tipSetTriggerSource(param_val);
      if(rval != OK)
	return ERROR;
    }

  CHECK_PARAM(ti_general_ini, "SYNC_SOURCE");
  if(param_val > 0)
    {
      rval = tipSetSyncSource(param_val);
      if(rval != OK)
	return ERROR;
    }

  CHECK_PARAM(ti_general_ini, "SYNC_RESET_TYPE");
  if(param_val > 0)
    {
      rval = tipSetSyncResetType(param_val);
      if(rval != OK)
	return ERROR;
    }

  /* Busy Source, build a busy source mask */
  uint32_t busy_source_mask = 0;

  CHECK_PARAM(ti_general_ini, "BUSY_SOURCE_SWA");
  if(param_val > 0)
    busy_source_mask |= TIP_BUSY_SWA;

  CHECK_PARAM(ti_general_ini, "BUSY_SOURCE_SWB");
  if(param_val > 0)
    busy_source_mask |= TIP_BUSY_SWB;

  CHECK_PARAM(ti_general_ini, "BUSY_SOURCE_FP_TDC");
  if(param_val > 0)
    busy_source_mask |= TIP_BUSY_FP_FTDC;

  CHECK_PARAM(ti_general_ini, "BUSY_SOURCE_FP_ADC");
  if(param_val > 0)
    busy_source_mask |= TIP_BUSY_FP_FADC;

  CHECK_PARAM(ti_general_ini, "BUSY_SOURCE_FP");
  if(param_val > 0)
    busy_source_mask |= TIP_BUSY_FP;

  CHECK_PARAM(ti_general_ini, "BUSY_SOURCE_LOOPBACK");
  if(param_val > 0)
    busy_source_mask |= TIP_BUSY_LOOPBACK;

  CHECK_PARAM(ti_general_ini, "BUSY_SOURCE_FIBER1");
  if(param_val > 0)
    busy_source_mask |= (TIP_BUSY_HFBR1 << 0);

  CHECK_PARAM(ti_general_ini, "BUSY_SOURCE_FIBER2");
  if(param_val > 0)
    busy_source_mask |= (TIP_BUSY_HFBR1 << 1);

  CHECK_PARAM(ti_general_ini, "BUSY_SOURCE_FIBER3");
  if(param_val > 0)
    busy_source_mask |= (TIP_BUSY_HFBR1 << 2);

  CHECK_PARAM(ti_general_ini, "BUSY_SOURCE_FIBER4");
  if(param_val > 0)
    busy_source_mask |= (TIP_BUSY_HFBR1 << 3);

  CHECK_PARAM(ti_general_ini, "BUSY_SOURCE_FIBER5");
  if(param_val > 0)
    busy_source_mask |= (TIP_BUSY_HFBR1 << 4);

  CHECK_PARAM(ti_general_ini, "BUSY_SOURCE_FIBER6");
  if(param_val > 0)
    busy_source_mask |= (TIP_BUSY_HFBR1 << 5);

  CHECK_PARAM(ti_general_ini, "BUSY_SOURCE_FIBER7");
  if(param_val > 0)
    busy_source_mask |= (TIP_BUSY_HFBR1 << 6);

  CHECK_PARAM(ti_general_ini, "BUSY_SOURCE_FIBER8");
  if(param_val > 0)
    busy_source_mask |= (TIP_BUSY_HFBR1 << 7);

  if(busy_source_mask != 0)
    {
      rval = tipSetBusySource(busy_source_mask, 1);
      if(rval != OK)
	return ERROR;
    }


  CHECK_PARAM(ti_general_ini, "CLOCK_SOURCE");
  if(param_val > 0)
    {
      rval = tipSetClockSource(param_val);
      if(rval != OK)
	return ERROR;
    }

  CHECK_PARAM(ti_general_ini, "PRESCALE");
  if(param_val > 0)
    {
      rval = tipSetPrescale(param_val);
      if(rval != OK)
	return ERROR;
    }

  CHECK_PARAM(ti_general_ini, "EVENT_FORMAT");
  if(param_val > 0)
    {
      rval = tipSetEventFormat(param_val);
      if(rval != OK)
	return ERROR;
    }

  CHECK_PARAM(ti_general_ini, "FP_INPUT_READOUT_ENABLE");
  if(param_val > 0)
    {
      rval = tipSetFPInputReadout(param_val);
      if(rval != OK)
	return ERROR;
    }

#ifdef DNE
  CHECK_PARAM(ti_general_ini, "GO_OUTPUT_ENABLE");
  if(param_val > 0)
    {
      rval = tipSetGoOutput(param_val);
      if(rval != OK)
	return ERROR;
    }
#endif

  CHECK_PARAM(ti_general_ini, "TRIGGER_WINDOW");
  if(param_val > 0)
    {
      rval = tipSetTriggerWindow(param_val);
      if(rval != OK)
	return ERROR;
    }

  CHECK_PARAM(ti_general_ini, "TRIGGER_INHIBIT_WINDOW");
  if(param_val > 0)
    {
      rval = tipSetTriggerInhibitWindow(param_val);
      if(rval != OK)
	return ERROR;
    }

  CHECK_PARAM(ti_general_ini, "TRIGGER_LATCH_ON_LEVEL_ENABLE");
  if(param_val > 0)
    {
      rval = tipSetTriggerLatchOnLevel(param_val);
      if(rval != OK)
	return ERROR;
    }

  CHECK_PARAM(ti_general_ini, "TRIGGER_OUTPUT_DELAY");
  if(param_val > 0)
    {
      int32_t delay = param_val, width = 0, delaystep = 0;

      CHECK_PARAM(ti_general_ini, "TRIGGER_OUTPUT_WIDTH");
      width = param_val;

      CHECK_PARAM(ti_general_ini, "TRIGGER_OUTPUT_DELAYSTEP");
      delaystep = param_val;

      rval = tipSetTriggerPulse(1, delay, width, delaystep);
      if(rval != OK)
	return ERROR;
    }

  CHECK_PARAM(ti_general_ini, "PROMPT_TRIGGER_WIDTH");
  if(param_val > 0)
    {
      int32_t width = param_val;

      rval = tipSetPromptTriggerWidth(width);
      if(rval != OK)
	return ERROR;
    }

  CHECK_PARAM(ti_general_ini, "SYNCRESET_DELAY");
  if(param_val > 0)
    {
      int32_t delay = param_val, width = 0, widthstep = 0;

      CHECK_PARAM(ti_general_ini, "SYNCRESET_WIDTH");
      width = param_val;

      CHECK_PARAM(ti_general_ini, "SYNCRESET_WIDTHSTEP");
      widthstep = param_val;

      tipSetSyncDelayWidth(delay, width, widthstep);
    }

#ifdef DNE
  CHECK_PARAM(ti_general_ini, "EVENTTYPE_SCALERS_ENABLE");
  if(param_val > 0)
    {
      rval = tipSetEvTypeScalers(param_val);
      if(rval != OK)
	return ERROR;
    }


  CHECK_PARAM(ti_general_ini, "SCALER_MODE");
  if(param_val > 0)
    {
      rval = tipSetScalerMode(param_val, 0);
      if(rval != OK)
	return ERROR;

    }
#endif

  CHECK_PARAM(ti_general_ini, "SYNCEVENT_INTERVAL");
  if(param_val > 0)
    {
      rval = tipSetSyncEventInterval(param_val);
      if(rval != OK)
	return ERROR;

    }

  CHECK_PARAM(ti_general_ini, "TRIGGER_TABLE");
  if(param_val > 0)
    {
      rval = tipTriggerTablePredefinedConfig(param_val);
      if(rval != OK)
	return ERROR;

    }

  CHECK_PARAM(ti_general_ini, "FIXED_PULSER_EVENTTYPE");
  if(param_val > 0)
    {
      int32_t fixed = param_val, random = 0;

      CHECK_PARAM(ti_general_ini, "RANDOM_PULSER_EVENTTYPE");
      random = param_val;

      rval = tipDefinePulserEventType(fixed, random);
      if(rval != OK)
	return ERROR;

    }

#ifdef DNE
  CHECK_PARAM(ti_general_ini, "FIBER_SYNC_DELAY");
  if(param_val > 0)
    {
      tipSetFiberSyncDelay(param_val);
    }
#endif

  // slaves
  CHECK_PARAM(ti_slaves_ini, "ENABLE_FIBER_1");
  if(param_val > 0)
    {
      rval = tipAddSlave(1);
      if(rval != OK)
	return ERROR;
    }

  // ts inputs
  uint32_t tsinput_mask = 0;
  CHECK_PARAM(ti_tsinputs_ini, "ENABLE_TS1");
  if(param_val > 0)
    {
      tsinput_mask |= TIP_TSINPUT_1;
    }

  CHECK_PARAM(ti_tsinputs_ini, "PRESCALE_TS1");
  if(param_val > 0)
    {
      tipSetTSInputDelay(1, param_val);
    }

  CHECK_PARAM(ti_tsinputs_ini, "PRESCALE_TS1");
  if(param_val > 0)
    {
      tipSetInputPrescale(1, param_val);
    }

  CHECK_PARAM(ti_tsinputs_ini, "ENABLE_TS2");
  if(param_val > 0)
    {
      tsinput_mask |= TIP_TSINPUT_2;
    }

  CHECK_PARAM(ti_tsinputs_ini, "PRESCALE_TS2");
  if(param_val > 0)
    {
      tipSetTSInputDelay(2, param_val);
    }

  CHECK_PARAM(ti_tsinputs_ini, "PRESCALE_TS2");
  if(param_val > 0)
    {
      tipSetInputPrescale(2, param_val);
    }

  CHECK_PARAM(ti_tsinputs_ini, "ENABLE_TS3");
  if(param_val > 0)
    {
      tsinput_mask |= TIP_TSINPUT_3;
    }

  CHECK_PARAM(ti_tsinputs_ini, "PRESCALE_TS3");
  if(param_val > 0)
    {
      tipSetTSInputDelay(3, param_val);
    }

  CHECK_PARAM(ti_tsinputs_ini, "PRESCALE_TS3");
  if(param_val > 0)
    {
      tipSetInputPrescale(3, param_val);
    }

  CHECK_PARAM(ti_tsinputs_ini, "ENABLE_TS4");
  if(param_val > 0)
    {
      tsinput_mask |= TIP_TSINPUT_4;
    }

  CHECK_PARAM(ti_tsinputs_ini, "PRESCALE_TS4");
  if(param_val > 0)
    {
      tipSetTSInputDelay(4, param_val);
    }

  CHECK_PARAM(ti_tsinputs_ini, "PRESCALE_TS4");
  if(param_val > 0)
    {
      tipSetInputPrescale(4, param_val);
    }

  CHECK_PARAM(ti_tsinputs_ini, "ENABLE_TS5");
  if(param_val > 0)
    {
      tsinput_mask |= TIP_TSINPUT_5;
    }

  CHECK_PARAM(ti_tsinputs_ini, "PRESCALE_TS5");
  if(param_val > 0)
    {
      tipSetTSInputDelay(5, param_val);
    }

  CHECK_PARAM(ti_tsinputs_ini, "PRESCALE_TS5");
  if(param_val > 0)
    {
      tipSetInputPrescale(5, param_val);
    }

  CHECK_PARAM(ti_tsinputs_ini, "ENABLE_TS6");
  if(param_val > 0)
    {
      tsinput_mask |= TIP_TSINPUT_6;
    }

  CHECK_PARAM(ti_tsinputs_ini, "PRESCALE_TS6");
  if(param_val > 0)
    {
      tipSetTSInputDelay(6, param_val);
    }

  CHECK_PARAM(ti_tsinputs_ini, "PRESCALE_TS6");
  if(param_val > 0)
    {
      tipSetInputPrescale(6, param_val);
    }

  if(tsinput_mask)
    {
      tipEnableTSInput(tsinput_mask);
    }

  // Trigger Rules
  int32_t time = 0, timestep = 0;
  CHECK_PARAM(ti_rules_ini, "RULE_1");
  if(param_val > 0)
    time = param_val;

  CHECK_PARAM(ti_rules_ini, "RULE_TIMESTEP_1");
  if(param_val > 0)
    timestep = param_val;

  if(time > 0)
    tipSetTriggerHoldoff(1, time, timestep);

  CHECK_PARAM(ti_rules_ini, "RULE_MIN_1");
  if(param_val > 0)
    tipSetTriggerHoldoffMin(1, param_val);

  time = 0; timestep = 0;

  CHECK_PARAM(ti_rules_ini, "RULE_2");
  if(param_val > 0)
    time = param_val;

  CHECK_PARAM(ti_rules_ini, "RULE_TIMESTEP_2");
  if(param_val > 0)
    timestep = param_val;

  if(time > 0)
    tipSetTriggerHoldoff(2, time, timestep);

  CHECK_PARAM(ti_rules_ini, "RULE_MIN_2");
  if(param_val > 0)
    tipSetTriggerHoldoffMin(2, param_val);

  time = 0; timestep = 0;

  CHECK_PARAM(ti_rules_ini, "RULE_3");
  if(param_val > 0)
    time = param_val;

  CHECK_PARAM(ti_rules_ini, "RULE_TIMESTEP_3");
  if(param_val > 0)
    timestep = param_val;

  if(time > 0)
    tipSetTriggerHoldoff(3, time, timestep);

  CHECK_PARAM(ti_rules_ini, "RULE_MIN_3");
  if(param_val > 0)
    tipSetTriggerHoldoffMin(3, param_val);

  time = 0; timestep = 0;

  CHECK_PARAM(ti_rules_ini, "RULE_4");
  if(param_val > 0)
    time = param_val;

  CHECK_PARAM(ti_rules_ini, "RULE_TIMESTEP_4");
  if(param_val > 0)
    timestep = param_val;

  if(time > 0)
    tipSetTriggerHoldoff(4, time, timestep);

  CHECK_PARAM(ti_rules_ini, "RULE_MIN_4");
  if(param_val > 0)
    tipSetTriggerHoldoffMin(4, param_val);


  return 0;
}

int32_t
tiConfigLoadParameters()
{
  if(ir == NULL)
    return 1;

  parseIni();

  param2ti();


  return 0;
}

// load in parameters to structure from filename
int32_t
tiConfig(const char *filename)
{
#ifdef DEBUG
  std::cout << __func__ << ": INFO: here" << std::endl;
#endif

  ir = new INIReader(filename);
  if(ir->ParseError() < 0)
    {
      std::cout << "Can't load: " << filename << std::endl;
      return ERROR;
    }
  std::cout << __func__ << ": INFO: Loaded file:" << filename << std::endl;


  tiConfigLoadParameters();
  return 0;
}

int32_t
tipConfigGetIntParameter(const char* param, int32_t *value)
{
  int32_t param_val = 0;
  ti_param_map::const_iterator pos;

  if(ir == NULL)
    return ERROR;

  pos = ti_general_ini.find(param);
  if(pos == ti_general_ini.end()) {
    std::cerr << __func__ << ": ERROR finding " << param << std::endl;
    return ERROR;
  }

  return param_val;
}

int32_t
tipConfigEnablePulser()
{
  int32_t param_val = 0, rval = OK;
  ti_param_map::const_iterator pos;

  int32_t fixed_enable = 0, fixed_number = 0, fixed_period = 0, fixed_range = 0;
  int32_t random_enable = 0, random_prescale = 0;

  CHECK_PARAM(ti_pulser_ini, "FIXED_ENABLE");
  fixed_enable = param_val;

  CHECK_PARAM(ti_pulser_ini, "FIXED_NUMBER");
  fixed_number = param_val;

  CHECK_PARAM(ti_pulser_ini, "FIXED_PERIOD");
  fixed_period = param_val;

  CHECK_PARAM(ti_pulser_ini, "FIXED_RANGE");
  fixed_range = param_val;

  CHECK_PARAM(ti_pulser_ini, "RANDOM_ENABLE");
  random_enable = param_val;

  CHECK_PARAM(ti_pulser_ini, "RANDOM_PRESCALE");
  random_prescale = param_val;

  if(fixed_enable)
    {
      tipSoftTrig(1, fixed_number, fixed_number, fixed_range);
    }

  if(random_enable)
    {
      tipSetRandomTrigger(1, random_prescale);
    }

  return OK;
}

int32_t
tipConfigDisablePulser()
{
  int32_t param_val = 0, rval = OK;
  ti_param_map::const_iterator pos;

  int32_t fixed_enable = 0, random_enable = 0;

  CHECK_PARAM(ti_pulser_ini, "FIXED_ENABLE");
  fixed_enable = param_val;

  CHECK_PARAM(ti_pulser_ini, "RANDOM_ENABLE");
  random_enable = param_val;

  if(fixed_enable)
    {
      tipSoftTrig(1, 0, 0, 0);
    }

  if(random_enable)
    {
      tipDisableRandomTrigger();
    }

  return OK;
}

/**
 * @brief Read the module parameters input the maps
 * @return 0
 */

int32_t
ti2param()
{
  int32_t rval = OK;

  rval = tipGetCrateID(0);
  if(rval == ERROR)
    return ERROR;
  else
    ti_general_readback["CRATE_ID"] = rval;


  rval = tipGetCurrentBlockLevel();
  if(rval == ERROR)
    return ERROR;
  else
    ti_general_readback["BLOCK_LEVEL"] = rval;

#ifdef DNE
  rval = tipGetBlockBufferLevel();
  if(rval == ERROR)
    return ERROR;
  else
    ti_general_readback["BLOCK_BUFFER_LEVEL"] = rval;
#endif

  rval = tipGetInstantBlockLevelChange();
  if(rval == ERROR)
    return ERROR;
  else
    ti_general_readback["INSTANT_BLOCKLEVEL_ENABLE"] = rval;

#ifdef DNE
  // FIXME: DNE
  rval = tipGetUseBroadcastBlockBufferLevel();
  if(rval == ERROR)
    return ERROR;
  else
    ti_general_readback["BROADCAST_BUFFER_LEVEL_ENABLE"] = rval;
#endif

  rval = tipGetBlockLimit();
  if(rval == ERROR)
    return ERROR;
  else
    ti_general_readback["BLOCK_LIMIT"] = rval;

#ifdef DNE
  // FIXME: DNE
  rval = tipGetTriggerSource();
  if(rval == ERROR)
    return ERROR;
  else
    ti_general_readback["TRIGGER_SOURCE"] = rval;

  rval = tipGetSyncSource();
  if(rval == ERROR)
    return ERROR;
  else
    ti_general_readback["SYNC_SOURCE"] = rval;

  rval = tipGetSyncResetType();
  if(rval == ERROR)
    return ERROR;
  else
    ti_general_readback["SYNC_RESET_TYPE"] = rval;

  rval = tipGetBusySourceMask();
  ti_general_readback["BUSY_SOURCE_SWA"] = rval;
  ti_general_readback["BUSY_SOURCE_SWB"] = rval;
  ti_general_readback["BUSY_SOURCE_FP_TDC"] = rval;
  ti_general_readback["BUSY_SOURCE_FP_ADC"] = rval;
  ti_general_readback["BUSY_SOURCE_FP"] = rval;
  ti_general_readback["BUSY_SOURCE_LOOPBACK"] = rval;
  ti_general_readback["BUSY_SOURCE_FIBER1"] = rval;
  ti_general_readback["BUSY_SOURCE_FIBER2"] = rval;
  ti_general_readback["BUSY_SOURCE_FIBER3"] = rval;
  ti_general_readback["BUSY_SOURCE_FIBER4"] = rval;
  ti_general_readback["BUSY_SOURCE_FIBER5"] = rval;
  ti_general_readback["BUSY_SOURCE_FIBER6"] = rval;
  ti_general_readback["BUSY_SOURCE_FIBER7"] = rval;
  ti_general_readback["BUSY_SOURCE_FIBER8"] = rval;
#endif

  rval = tipGetClockSource();
  if(rval == ERROR)
    return ERROR;
  else
    ti_general_readback["CLOCK_SOURCE"] = rval;

  rval = tipGetPrescale();
  if(rval == ERROR)
    return ERROR;
  else
    ti_general_readback["PRESCALE"] = rval;

#ifdef DNE
  rval = tipGetEventFormat();
  if(rval == ERROR)
    return ERROR;
  else
    ti_general_readback["EVENT_FORMAT"] = rval;

  rval = tipGetFPInputReadout();
  if(rval == ERROR)
    return ERROR;
  else
    ti_general_readback["FP_INPUT_READOUT_ENABLE"] = rval;
#endif

  return OK;
}

/**
 * @brief Write the Ini values to an output file
 */
int32_t
writeIni(const char* filename)
{
  std::ofstream outFile;
  int32_t error;

  outFile.open(filename);
  if(!outFile)
    {
      std::cerr << __func__ << ": ERROR: Unable to open file for writting: " << filename << std::endl;
      return -1;
    }

  outFile << "[general]" << std::endl;

  ti_param_map::const_iterator pos = ti_general_def.begin();

  while(pos != ti_general_def.end())
    {
      if(ti_general_readback[pos->first] != -1)
	{
	  outFile << pos->first << "= " << ti_general_readback[pos->first] << std::endl;
	}

      ++pos;
    }

  outFile << "[slaves]" << std::endl;

  pos = ti_slaves_def.begin();
  while(pos != ti_slaves_def.end())
    {
      if(ti_slaves_readback[pos->first] != -1)
	{
	  outFile << pos->first << "= " << ti_slaves_readback[pos->first] << std::endl;
	}

      ++pos;
    }

  outFile << "[tsinputs]" << std::endl;

  pos = ti_tsinputs_def.begin();
  while(pos != ti_tsinputs_def.end())
    {
      if(ti_tsinputs_readback[pos->first] != -1)
	{
	  outFile << pos->first << "= " << ti_tsinputs_readback[pos->first] << std::endl;
	}

      ++pos;
    }

  outFile << "[trigger_rules]" << std::endl;

  pos = ti_rules_def.begin();
  while(pos != ti_rules_def.end())
    {
      if(ti_rules_readback[pos->first] != -1)
	{
	  outFile << pos->first << "= " << ti_rules_readback[pos->first] << std::endl;
	}

      ++pos;
    }




  outFile.close();
  return 0;

}

// destroy the ini object
int32_t
tiConfigFree()
{
#ifdef DEBUG
  std::cout << __func__ << ": INFO: here" << std::endl;
#endif

  if(ir == NULL)
    return ERROR;

#ifdef DEBUG
  std::cout << "delete ir" << std::endl;
#endif
  delete ir;

  return 0;
}
