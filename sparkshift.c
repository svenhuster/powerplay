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

     int64_t power_excess_accum = 0;
     int64_t rounds = 0;

     for(size_t i = 0;; ++i) {
	  int64_t power_excess_mean;

	  if (system_status_update(&current)) goto loop;

	  rounds += 1;
	  power_excess_accum += current.power_excess;
	  power_excess_mean = power_excess_accum / rounds;

	  printf("M/%c S/%c C/%d R/%4ld A/%7ld X/%7d G/%7d B/%7d P/%7d C/%7d E/%7d BS/%3d ES/%3d\n",
		 get_charging_mode_char(current.evcs_charging_mode),
		 get_charger_status_char(current.evcs_charger_status),
		 current.evcs_charge_start,
		 rounds,
		 power_excess_mean,
		 current.power_excess, current.power_grid, current.power_battery,
		 current.power_pv, current.power_consumption, current.power_evcs,
		 current.soc_battery, current.soc_ev);

	  if (current.evcs_charging_mode == EVCS_CHARGE_MODE_MANUAL
	      && current.evcs_charger_status == EVCS_CHARGER_STATUS_DISCONNECTED) {
	       /* TODO: reset back to Auto mode */
	  }

	  if (current.config.averaging_secs <= (rounds * current.config.sleep_secs)) {
	       rounds = 0;
	       power_excess_accum = 0;

	       if (current.config.dryrun) goto loop;

	       if (current.evcs_charging_mode == EVCS_CHARGE_MODE_AUTO) {
		    if (power_excess_mean > current.config.power_excess_min) {
			 if (evcs_charging_start_set(&current, EVCS_CHARGING_START)) goto loop;
		    } else {
			 if (evcs_charging_start_set(&current, EVCS_CHARGING_STOP)) goto loop;
		    }
	       }
	  }

     loop:
	  fflush(stdout);
	  sleep(current.config.sleep_secs);
     }
}
