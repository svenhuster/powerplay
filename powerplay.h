#ifndef INCLUDE_POWERPLAY_H
#define INCLUDE_POWERPLAY_H

#include <stdint.h>
#include <modbus/modbus.h>

/*
 *
 * PowerPlay
 *
 */

struct modbus_device {
     const char *host;
     int port;
};


struct config {
     int32_t power_excess_min;
     time_t averaging_secs;
     uint32_t sleep_secs;
     int debug;
     int dryrun;
     struct modbus_device gx;
     struct modbus_device evcs;
};

struct system_status {
     struct config config;

     modbus_t *gx_ctx, *evcs_ctx;

     int32_t power_grid;
     int32_t power_pv;
     int32_t power_consumption;
     int32_t power_battery;
     int32_t power_evcs;
     int32_t power_excess;

     uint16_t soc_battery;
     uint16_t soc_ev;

     uint16_t evcs_charge_start;
     uint16_t evcs_charger_status;
     uint16_t evcs_charging_mode;
};

int config_from_env(struct config *config);
int modbus_device_connect(struct modbus_device device, modbus_t **ctx);
int system_status_update(struct system_status *status);
void system_status_debug_print(const struct system_status *status);

/*
 *
 * GX
 *
 */

/* CCGX Modbus TCP register list 3.50 */
typedef enum {
     GX_REGISTER_SWITCH_POSITION			= 33,  /* uint16, com.victronenergy.vebus */
     GX_REGISTER_PV_AC_IN_L1				= 811, /* uint16, com.victronenergy.system */
     GX_REGISTER_PV_AC_IN_L2				= 812, /* uint16, com.victronenergy.system */
     GX_REGISTER_PV_AC_IN_L3				= 813, /* uint16, com.victronenergy.system */
     GX_REGISTER_AC_CONSUMPTION_L1			= 817, /* uint16, com.victronenergy.system */
     GX_REGISTER_AC_CONSUMPTION_L2			= 818, /* uint16, com.victronenergy.system */
     GX_REGISTER_AC_CONSUMPTION_L3			= 819, /* uint16, com.victronenergy.system */
     GX_REGISTER_GRID_L1				= 820, /* int16,  com.victronenergy.system */
     GX_REGISTER_GRID_L2				= 821, /* int16,  com.victronenergy.system */
     GX_REGISTER_GRID_L3				= 822, /* int16,  com.victronenergy.system */
     GX_REGISTER_BATTERY_POWER				= 842, /* int16,  com.victronenergy.system */
     GX_REGISTER_BATTERY_SOC				= 843, /* uint16, com.victronenergy.system */
} gx_register_t;

typedef enum {
     GX_SWITCH_CHARGER_ONLY				= 1,
     GX_SWITCH_INVERTER_ONLY				= 2,
     GX_SWITCH_ON					= 3,
     GX_SWITCH_OFF					= 4,
} gx_switch_t;

/*
 *
 * EVCS
 *
 */

/* EVCS Modbus TCP register list 3.5 */
typedef enum {
     EVCS_REGISTER_CHARGE_MODE				= 5009, /* uint16_t */
     EVCS_REGISTER_CHARGE_START				= 5010, /* uint16_t */
     EVCS_REGISTER_TOTAL_POWER				= 5014, /* uint16_t */
     EVCS_REGISTER_CHARGER_STATUS			= 5015, /* uint16_t */
     EVCS_REGISTER_CHARGING_CURRENT			= 5016, /* uint16_t */
     EVCS_REGISTER_MAX_CURRENT				= 5017, /* uint16_t */
} evcs_register_t;

typedef enum {
     EVCS_CHARGE_MODE_MANUAL				= 0,
     EVCS_CHARGE_MODE_AUTO				= 1,
     EVCS_CHARGE_MODE_SCHED				= 2,
} evcs_charge_mode_t;

typedef enum {
     EVCS_CHARGING_STOP                                 = 0,
     EVCS_CHARGING_START                                = 1,
} evcs_charging_start_t;

typedef enum {
    EVCS_CHARGER_STATUS_DISCONNECTED			= 0,
    EVCS_CHARGER_STATUS_CONNECTED			= 1,
    EVCS_CHARGER_STATUS_CHARGING			= 2,
    EVCS_CHARGER_STATUS_CHARGED				= 3,
    EVCS_CHARGER_STATUS_WAITING_FOR_SUN			= 4,
    EVCS_CHARGER_STATUS_WAITING_FOR_RFID		= 5,
    EVCS_CHARGER_STATUS_WAITING_FOR_START		= 6,
    EVCS_CHARGER_STATUS_LOW_SOC				= 7,
    EVCS_CHARGER_STATUS_GROUND_TEST_ERROR		= 8,
    EVCS_CHARGER_STATUS_WELDED_CONTACTS_ERROR		= 9,
    EVCS_CHARGER_STATUS_CP_INPUT_ERROR_SHORTED		= 10,
    EVCS_CHARGER_STATUS_RESIDUAL_CURRENT_DETECTED	= 11,
    EVCS_CHARGER_STATUS_UNDERVOLTAGE_DETECTED		= 12,
    EVCS_CHARGER_STATUS_OVERVOLTAGE_DETECTED		= 13,
    EVCS_CHARGER_STATUS_OVERHEATING_DETECTED		= 14,
    EVCS_CHARGER_STATUS_RESERVED_15			= 15,
    EVCS_CHARGER_STATUS_RESERVED_16			= 16,
    EVCS_CHARGER_STATUS_RESERVED_17			= 17,
    EVCS_CHARGER_STATUS_RESERVED_18			= 18,
    EVCS_CHARGER_STATUS_RESERVED_19			= 19,
    EVCS_CHARGER_STATUS_CHARGING_LIMIT			= 20,
    EVCS_CHARGER_STATUS_START_CHARGING			= 21,
    EVCS_CHARGER_STATUS_SWITCHING_TO_3_PHASE		= 22,
    EVCS_CHARGER_STATUS_SWITCHING_TO_1_PHASE		= 23,
    EVCS_CHARGER_STATUS_STOP_CHARGING			= 24,
} evcs_charger_status_t;

int evcs_charging_start_set(const struct system_status *status, evcs_charging_start_t start);
int evcs_charge_mode_set(const struct system_status *status, evcs_charge_mode_t start);
char get_charger_status_char(evcs_charger_status_t status);
const char *get_charger_status_str(evcs_charger_status_t status);
char get_charging_mode_char(evcs_charge_mode_t mode);
const char *get_charging_mode_str(evcs_charge_mode_t mode);
const char *get_watchdog_reason_str(uint16_t code);
const char *get_reset_reason_str(uint16_t code);

#endif
