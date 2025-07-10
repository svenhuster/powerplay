#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "powerplay.h"

/*
  Environment variables required for configuration

  GX_HOST		: IP address of GX device
  GX_PORT		: Modbus TCP port of GX device

  EVCS_HOST		: IP address of EVCS device
  ECVS_PORT		: Modbus TCP port of EVCS device

  POWER_EXCESS_MIN	: Minimum excess power to start charging

  AVERAGING_SECS	: Seconds to average excess power over
  SLEEP_SECS		: Seconds to sleep in control loop

 */


int main(void)
{
     struct system_status current = {0};
     if (config_from_env(&current.config)) return 1;

     if (modbus_device_connect(current.config.evcs, &current.evcs_ctx)) return 1;
     if (modbus_device_connect(current.config.gx, &current.gx_ctx)) return 1;

     if (system_status_update(&current)) return 1;

     if (current.config.dryrun) printf("Dry run configure - ignoring all actions\n");

     /* Initialize desired state with current charging state as we do not yet have a reason to
      * change without collecting stats */
     evcs_charging_start_t charge_start = current.evcs_charge_start;
     int64_t power_excess_accum = 0;
     int64_t rounds = 0;

     for(size_t i = 0;; ++i) {
	  int64_t power_excess_mean;

	  if (system_status_update(&current)) goto next;

	  rounds += 1;
	  power_excess_accum += current.power_excess;
	  power_excess_mean = power_excess_accum / rounds;

	  printf("M/%c S/%c C/%d D/%u R/%4ld A/%7ld X/%7d G/%7d B/%7d P/%7d C/%7d E/%7d BS/%3d ES/%3d\n",
		 get_charging_mode_char(current.evcs_charging_mode),
		 get_charger_status_char(current.evcs_charger_status),
		 current.evcs_charge_start,
		 charge_start,
		 rounds,
		 power_excess_mean,
		 current.power_excess, current.power_grid, current.power_battery,
		 current.power_pv, current.power_consumption, current.power_evcs,
		 current.soc_battery, current.soc_ev);

	  if (current.config.averaging_secs <= (rounds * current.config.sleep_secs)) {
	       rounds = 0;
	       power_excess_accum = 0;

	       if (power_excess_mean > current.config.power_excess_min) {
		    if (current.config.debug) printf("High excess power - want charging\n");
		    charge_start = EVCS_CHARGING_START;
	       } else {
		    if (current.config.debug) printf("Low excess power - refuse charging\n");
		    charge_start = EVCS_CHARGING_STOP;
	       }
	  }

	  if (current.config.dryrun) goto next;

	  if (current.evcs_charging_mode == EVCS_CHARGE_MODE_AUTO
	      && current.evcs_charge_start != charge_start) {
	       if (current.config.debug) printf("Setting charge start to: %u\n", charge_start);
	       if (evcs_charging_start_set(&current, charge_start)) goto next;
	  }

	  if (current.evcs_charging_mode == EVCS_CHARGE_MODE_MANUAL
	      && current.evcs_charger_status == EVCS_CHARGER_STATUS_DISCONNECTED) {
	       if (current.config.debug) printf("Manual and disconnected - change to Auto\n");
	       if (evcs_charge_mode_set(&current, EVCS_CHARGE_MODE_AUTO)) goto next;
	  }

     next:
	  fflush(stdout);
	  sleep(current.config.sleep_secs);
     }
}
