#ifndef CYBERBOAT_TYPES_H
#define CYBERBOAT_TYPES_H

/** @file types.h
 *  @brief Transport- and hardware-independent data types.
 */

#include <stdbool.h>
#include <stdint.h>

/** Minimum normalized command value. */
#define CB_AXIS_MIN (-1000)
/** Maximum normalized command value. */
#define CB_AXIS_MAX 1000
/** Maximum number of propulsion outputs supported by the control model. */
#define CB_MAX_MOTORS 2U
/** Maximum number of independent PWM receiver channels represented in a frame. */
#define CB_MAX_RECEIVER_CHANNELS 8U

/** Normalized control axis in the inclusive range CB_AXIS_MIN..CB_AXIS_MAX. */
typedef int16_t cb_axis_t;

/** Pilot input independent of its physical receiver. */
typedef struct
{
    cb_axis_t throttle; /**< Propulsion request. */
    cb_axis_t steering; /**< Steering request. */
} cb_pilot_command_t;

/** Normalized propulsion commands after motor mixing. */
typedef struct
{
    cb_axis_t motor[CB_MAX_MOTORS]; /**< Output per motor, normalized to the axis range. */
} cb_motor_outputs_t;

/** Latest decoded GPS position and motion data. */
typedef struct
{
    bool valid;             /**< True only when the receiver reports a valid fix. */
    int32_t latitude_e7;    /**< Latitude in degrees multiplied by 10^7. */
    int32_t longitude_e7;   /**< Longitude in degrees multiplied by 10^7. */
    uint32_t speed_cm_s;    /**< Ground speed in centimeters per second. */
    uint16_t course_cdeg;   /**< Course over ground in centidegrees. */
} cb_gps_fix_t;

/** Raw PWM receiver sample before semantic channel mapping. */
typedef struct
{
    uint8_t channel_count; /**< Number of configured receiver channels. */
    uint16_t valid_channels_mask; /**< Bit N is set when channel N has a fresh valid pulse. */
    uint16_t pulse_width_us[CB_MAX_RECEIVER_CHANNELS]; /**< Captured pulse width per channel. */
    uint32_t channel_age_ms[CB_MAX_RECEIVER_CHANNELS]; /**< Age of the last valid pulse per channel. */
} cb_receiver_raw_frame_t;

/** Mapped receiver command with arming request and remaining freshness interval. */
typedef struct
{
    cb_pilot_command_t command; /**< Normalized pilot inputs. */
    cb_axis_t auxiliary_servo;  /**< Normalized command for a configured auxiliary servo role. */
    bool arm_requested;         /**< Requested arming state; not proof that arming is safe. */
    uint32_t valid_for_ms;       /**< Maximum age accepted for this frame. */
} cb_receiver_frame_t;

#endif