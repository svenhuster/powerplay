#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "powerplay.h"

/*
 *
 * Powerplay
 *
 */

int config_from_env(struct config *config)
{
     const char *config_debug = getenv("SPARKSHIFT_DEBUG");
     if (config_debug == NULL) {
	  fprintf(stderr, "Error: %s environment variable not set\n", "SPARKSHIFT_DEBUG");
	  goto error;
     }

     if (!strcmp("1", config_debug)) {
	  config->debug = 1;
     }

     const char *config_dryrun = getenv("SPARKSHIFT_DRYRUN");
     if (config_dryrun == NULL) {
	  fprintf(stderr, "Error: %s environment variable not set\n", "SPARKSHIFT_DRYRUN");
	  goto error;
     }

     if (!strcmp("1", config_dryrun)) {
	  config->dryrun = 1;
     }

     const char *config_averaging_secs = getenv("AVERAGING_SECS");
     if (config_averaging_secs == NULL) {
	  fprintf(stderr, "Error: %s environment variable not set\n", "AVERAGING_SECS");
	  goto error;
     }

     config->averaging_secs = atoi(config_averaging_secs);
     if (config->averaging_secs == 0) {
	  fprintf(stderr, "Error: %s environment variable not an integer\n", "AVERAGING_SECS");
	  goto error;
     }

     const char *config_sleep_secs = getenv("SLEEP_SECS");
     if (config_sleep_secs == NULL) {
	  fprintf(stderr, "Error: %s environment variable not set\n", "SLEEP_SECS");
	  goto error;
     }

     config->sleep_secs = (uint32_t)atoi(config_sleep_secs);
     if (config->sleep_secs == 0) {
	  fprintf(stderr, "Error: %s environment variable not an integer\n", "SLEEP_SECS");
	  goto error;
     }

     const char *config_power_excess_min = getenv("POWER_EXCESS_MIN");
     if (config_power_excess_min == NULL) {
	  fprintf(stderr, "Error: %s environment variable not set\n", "POWER_EXCESS_MIN");
	  goto error;
     }

     config->power_excess_min = atoi(config_power_excess_min);
     if (config->power_excess_min == 0) {
	  fprintf(stderr, "Error: %s environment variable not an integer\n", "POWER_EXCESS_MIN");
	  goto error;
     }

     config->gx.host = getenv("GX_HOST");
     if (config->gx.host == NULL) {
	  fprintf(stderr, "Error: %s environment variable not set\n", "GX_HOST");
	  goto error;
     }

     const char *gx_port_str = getenv("GX_PORT");
     if (gx_port_str == NULL) {
	  fprintf(stderr, "Error: %s environment variable not set\n", "GX_PORT");
	  goto error;
     }
     config->gx.port = atoi(gx_port_str);
     if (config->gx.port == 0) {
	  fprintf(stderr, "Error: %s environment variable not an integer\n", "GX_PORT");
	  goto error;
     }

     config->evcs.host = getenv("EVCS_HOST");
     if (config->evcs.host == NULL) {
	  fprintf(stderr, "Error: %s environment variable not set\n", "EVCS_HOST");
	  goto error;
     }

     const char *evcs_port_str = getenv("EVCS_PORT");
     if (evcs_port_str == NULL) {
	  fprintf(stderr, "Error: %s environment variable not set\n", "EVCS_PORT");
	  goto error;
     }
     config->evcs.port = atoi(evcs_port_str);
     if (config->evcs.port == 0) {
	  fprintf(stderr, "Error: %s environment variable not an integer\n", "EVCS_PORT");
	  goto error;
     }

     return 0;

error:
     fflush(stderr);
     return -1;

}

int modbus_device_connect(struct modbus_device device, modbus_t **ctx)
{
     *ctx = modbus_new_tcp(device.host, device.port);
     if (*ctx == NULL) {
	  fprintf(stderr, "Error: Failed to create modbus context: %s\n", modbus_strerror(errno));
	  goto error;
     }

     if (modbus_set_error_recovery(*ctx, MODBUS_ERROR_RECOVERY_LINK|MODBUS_ERROR_RECOVERY_PROTOCOL) == -1) {
	  fprintf(stderr, "Error: failed to set error recovery: %s\n", modbus_strerror(errno));
	  goto error;
     }

     if (modbus_set_response_timeout(*ctx, 5, 0) == -1) {
	  fprintf(stderr, "Error: failed to set timeout: %s\n", modbus_strerror(errno));
	  goto error;
     }

     if (modbus_connect(*ctx) == -1) {
	  fprintf(stderr, "Error: connection failed to %s:%d: %s\n",
		  device.host, device.port, modbus_strerror(errno));
	  goto error;
     }

     return 0;

error:
     fflush(stderr);
     return -1;
}

int system_status_update(struct system_status *status)
{
     uint16_t pv[3];
     int16_t consumption[3];
     int16_t grid[3];

     int16_t power_battery;
     uint16_t soc_battery;

     uint16_t power_evcs;
     uint16_t charge_start, charger_status, charging_mode;

     if ((modbus_set_slave(status->gx_ctx, 100) == -1)
	 || (modbus_read_registers(status->gx_ctx, GX_REGISTER_PV_AC_IN_L1, 3, (uint16_t *)&pv) == -1)
	 || (modbus_read_registers(status->gx_ctx, GX_REGISTER_AC_CONSUMPTION_L1, 3, (uint16_t *)&consumption) == -1)
	 || (modbus_read_registers(status->gx_ctx, GX_REGISTER_GRID_L1, 3, (uint16_t *)&grid) == -1)
	 || (modbus_read_registers(status->gx_ctx, GX_REGISTER_BATTERY_POWER, 1, (uint16_t *)&power_battery) == -1)
	 || (modbus_read_registers(status->gx_ctx, GX_REGISTER_BATTERY_SOC, 1, (uint16_t *)&soc_battery) == -1)) {
	  fprintf(stderr, "Error: could not read GX value: %s\n", modbus_strerror(errno));
	  goto error;
     }

     if ((modbus_read_registers(status->evcs_ctx, EVCS_REGISTER_TOTAL_POWER, 1, &power_evcs) == -1)
	 || (modbus_read_registers(status->evcs_ctx, EVCS_REGISTER_CHARGE_START, 1, &charge_start) == -1)
	 || (modbus_read_registers(status->evcs_ctx, EVCS_REGISTER_CHARGER_STATUS, 1, &charger_status) == -1)
	 || (modbus_read_registers(status->evcs_ctx, EVCS_REGISTER_CHARGE_MODE, 1, &charging_mode) == -1)) {
	  fprintf(stderr, "Error: could not read EVCS value: %s\n", modbus_strerror(errno));
	  goto error;
     }

     status->power_grid = (int32_t)grid[0] + (int32_t)grid[1] + (int32_t)grid[2];
     status->power_pv = (int32_t)pv[0] + (int32_t)pv[1] + (int32_t)pv[2];
     status->power_consumption = (int32_t)consumption[0] + (int32_t)consumption[1] + (int32_t)consumption[2];
     status->power_battery = (int32_t)power_battery;
     status->power_evcs = (int32_t)power_evcs;
     status->power_excess = status->power_battery - status->power_grid + status->power_evcs;

     status->soc_battery = soc_battery;
     status->soc_ev = 999;

     status->evcs_charge_start = charge_start;
     status->evcs_charger_status = charger_status;
     status->evcs_charging_mode = charging_mode;

     return 0;

error:
     fflush(stderr);
     return -1;
}

int evcs_charging_start_set(const struct system_status *status, evcs_charging_start_t start)
{
     if (modbus_write_register(status->evcs_ctx, EVCS_REGISTER_CHARGE_START, start) == -1) {
	  fprintf(stderr, "Error: could not set EVCS charge start value to %u: %s\n", start, modbus_strerror(errno));
	  goto error;
     }

     return 0;

error:
     fflush(stderr);
     return -1;
}

char get_charger_status_char(evcs_charger_status_t status)
{
     switch (status) {
     case EVCS_CHARGER_STATUS_DISCONNECTED:            return 'X';

     case EVCS_CHARGER_STATUS_CONNECTED:               return 'D';

     case EVCS_CHARGER_STATUS_START_CHARGING:
     case EVCS_CHARGER_STATUS_STOP_CHARGING:
     case EVCS_CHARGER_STATUS_CHARGING:                return 'C';

     case EVCS_CHARGER_STATUS_CHARGED:                 return 'F';

     case EVCS_CHARGER_STATUS_WAITING_FOR_SUN:
     case EVCS_CHARGER_STATUS_WAITING_FOR_START:
     case EVCS_CHARGER_STATUS_LOW_SOC:                 return 'W';

     case EVCS_CHARGER_STATUS_WAITING_FOR_RFID:
     case EVCS_CHARGER_STATUS_GROUND_TEST_ERROR:
     case EVCS_CHARGER_STATUS_WELDED_CONTACTS_ERROR:
     case EVCS_CHARGER_STATUS_CP_INPUT_ERROR_SHORTED:
     case EVCS_CHARGER_STATUS_RESIDUAL_CURRENT_DETECTED:
     case EVCS_CHARGER_STATUS_UNDERVOLTAGE_DETECTED:
     case EVCS_CHARGER_STATUS_OVERVOLTAGE_DETECTED:
     case EVCS_CHARGER_STATUS_OVERHEATING_DETECTED:
     case EVCS_CHARGER_STATUS_RESERVED_15:
     case EVCS_CHARGER_STATUS_RESERVED_16:
     case EVCS_CHARGER_STATUS_RESERVED_17:
     case EVCS_CHARGER_STATUS_RESERVED_18:
     case EVCS_CHARGER_STATUS_RESERVED_19:             return 'E';

     case EVCS_CHARGER_STATUS_CHARGING_LIMIT:          return 'L';

     case EVCS_CHARGER_STATUS_SWITCHING_TO_3_PHASE:    return '3';

     case EVCS_CHARGER_STATUS_SWITCHING_TO_1_PHASE:    return '1';

     default:                                          return '?';
     }
}

const char *get_charger_status_str(evcs_charger_status_t status)
{
     switch (status) {
     case EVCS_CHARGER_STATUS_DISCONNECTED:
          return "Disconnected";
     case EVCS_CHARGER_STATUS_CONNECTED:
          return "Connected";
     case EVCS_CHARGER_STATUS_CHARGING:
          return "Charging";
     case EVCS_CHARGER_STATUS_CHARGED:
          return "Charged";
     case EVCS_CHARGER_STATUS_WAITING_FOR_SUN:
          return "Waiting for sun";
     case EVCS_CHARGER_STATUS_WAITING_FOR_RFID:
          return "Waiting for RFID";
     case EVCS_CHARGER_STATUS_WAITING_FOR_START:
          return "Waiting for start";
     case EVCS_CHARGER_STATUS_LOW_SOC:
          return "Low SOC";
     case EVCS_CHARGER_STATUS_GROUND_TEST_ERROR:
          return "Ground test error";
     case EVCS_CHARGER_STATUS_WELDED_CONTACTS_ERROR:
          return "Welded contacts test error";
     case EVCS_CHARGER_STATUS_CP_INPUT_ERROR_SHORTED:
          return "CP input test error (shorted)";
     case EVCS_CHARGER_STATUS_RESIDUAL_CURRENT_DETECTED:
          return "Residual current detected";
     case EVCS_CHARGER_STATUS_UNDERVOLTAGE_DETECTED:
          return "Undervoltage detected";
     case EVCS_CHARGER_STATUS_OVERVOLTAGE_DETECTED:
          return "Overvoltage detected";
     case EVCS_CHARGER_STATUS_OVERHEATING_DETECTED:
          return "Overheating detected";
     case EVCS_CHARGER_STATUS_RESERVED_15:
     case EVCS_CHARGER_STATUS_RESERVED_16:
     case EVCS_CHARGER_STATUS_RESERVED_17:
     case EVCS_CHARGER_STATUS_RESERVED_18:
     case EVCS_CHARGER_STATUS_RESERVED_19:
          return "Reserved";
     case EVCS_CHARGER_STATUS_CHARGING_LIMIT:
          return "Charging limit";
     case EVCS_CHARGER_STATUS_START_CHARGING:
          return "Start charging";
     case EVCS_CHARGER_STATUS_SWITCHING_TO_3_PHASE:
          return "Switching to 3 phase";
     case EVCS_CHARGER_STATUS_SWITCHING_TO_1_PHASE:
          return "Switching to 1 phase";
     case EVCS_CHARGER_STATUS_STOP_CHARGING:
          return "Stop charging";
     default:
          return "Unknown state";
     }
}

char get_charging_mode_char(evcs_charge_mode_t mode)
{
     switch (mode) {
     case EVCS_CHARGE_MODE_MANUAL:   return 'M';
     case EVCS_CHARGE_MODE_AUTO:     return 'A';
     case EVCS_CHARGE_MODE_SCHED:    return 'S';
     default:                        return '?';
     }
}

const char *get_charging_mode_str(evcs_charge_mode_t mode)
{
     switch (mode) {
     case EVCS_CHARGE_MODE_MANUAL:
          return "Manual";
     case EVCS_CHARGE_MODE_AUTO:
          return "Auto";
     case EVCS_CHARGE_MODE_SCHED:
          return "Scheduled";
     default:
          return "Unknown mode";
     }
}

const char *get_reset_reason_str(uint16_t code)
{
     switch (code) {
     case 0:
	  return "Reset reason can not be determined";
     case 1:
	  return "Reset due to power-on event";
     case 2:
	  return "Reset by external pin";
     case 3:
	  return "Software reset via reboot function";
     case 4:
	  return "Software reset due to exception/panic";
     case 5:
	  return "Reset (software or hardware) due to interrupt watchdog";
     case 6:
	  return "Reset due to task watchdog";
     case 7:
	  return "Reset due to other watchdogs";
     case 8:
	  return "Reset after exiting deep sleep mode";
     case 9:
	  return "Brownout reset (software or hardware)";
     case 10:
	  return "Reset over SDIO";
     default:
	  return "Unknown reset reason";
     }
}

const char *get_watchdog_reason_str(uint16_t code)
{
     switch (code) {
     case 0:
	  return "Reset reason can not be determined";
     case 1:
	  return "Can't connect to desired WiFi access point";
     case 2:
	  return "Critically low memory amount";
     case 3:
	  return "Critical memory fragmentation";
     case 4:
	  return "Memory allocation failed";
     case 5:
	  return "Ping failed";
     default:
	  return "Unknown watchdog reason";
     }
}
