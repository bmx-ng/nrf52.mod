#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>

#if DT_NODE_EXISTS(DT_NODELABEL(bmx_imu))
#define BMX_HAS_IMU 1
static const struct device *const bmx_imu = DEVICE_DT_GET(DT_NODELABEL(bmx_imu));
#else
#define BMX_HAS_IMU 0
#endif

static int bmx_imu_ready(void) {
#if BMX_HAS_IMU
    return device_is_ready(bmx_imu);
#else
    return 0;
#endif
}

int32_t bmx_embedded_motion_sensor_count(void) {
    return bmx_imu_ready() ? 1 : 0;
}

int32_t bmx_embedded_motion_read(int32_t sensor, float *accel_x,
        float *accel_y, float *accel_z, float *gyro_x, float *gyro_y,
        float *gyro_z, int64_t *timestamp_us) {
#if BMX_HAS_IMU
    if (sensor != 0 || !bmx_imu_ready() || !accel_x || !accel_y || !accel_z ||
            !gyro_x || !gyro_y || !gyro_z || !timestamp_us) return 0;
    struct sensor_value acceleration[3], rotation[3];
    if (sensor_sample_fetch(bmx_imu) != 0 ||
            sensor_channel_get(bmx_imu, SENSOR_CHAN_ACCEL_XYZ, acceleration) != 0 ||
            sensor_channel_get(bmx_imu, SENSOR_CHAN_GYRO_XYZ, rotation) != 0) return 0;
    *timestamp_us = k_ticks_to_us_floor64(k_uptime_ticks());
    *accel_x = (float)sensor_value_to_double(&acceleration[0]);
    *accel_y = (float)sensor_value_to_double(&acceleration[1]);
    *accel_z = (float)sensor_value_to_double(&acceleration[2]);
    *gyro_x = (float)sensor_value_to_double(&rotation[0]);
    *gyro_y = (float)sensor_value_to_double(&rotation[1]);
    *gyro_z = (float)sensor_value_to_double(&rotation[2]);
    return 1;
#else
    (void)sensor; (void)accel_x; (void)accel_y; (void)accel_z;
    (void)gyro_x; (void)gyro_y; (void)gyro_z; (void)timestamp_us;
    return 0;
#endif
}

int32_t bmx_embedded_temperature_sensor_count(void) {
    return bmx_imu_ready() ? 1 : 0;
}

int32_t bmx_embedded_temperature_sensor_kind(int32_t sensor) {
    return sensor == 0 && bmx_imu_ready() ? 1 : 0;
}

int32_t bmx_embedded_temperature_read_celsius(int32_t sensor, float *celsius) {
#if BMX_HAS_IMU
    if (sensor != 0 || !bmx_imu_ready() || !celsius) return 0;
    struct sensor_value value;
    if (sensor_sample_fetch_chan(bmx_imu, SENSOR_CHAN_DIE_TEMP) != 0 ||
            sensor_channel_get(bmx_imu, SENSOR_CHAN_DIE_TEMP, &value) != 0) return 0;
    *celsius = (float)sensor_value_to_double(&value);
    return 1;
#else
    (void)sensor; (void)celsius;
    return 0;
#endif
}
