#define DT_DRV_COMPAT microchip_mic24045

#include <zephyr/device.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/logging/log.h>
#include "mic24045.h"

LOG_MODULE_REGISTER(mic24045, CONFIG_REGULATOR_LOG_LEVEL);

// ────── Voltage Conversion ──────

static uint8_t float_to_vout_reg(float vout)
{
    if (vout >= 5.250f) return 0xFF;
    else if (vout >= 4.750f) return 0xF5 + (int)((vout - 4.75f) / 0.05f);
    else if (vout >= 3.420f) return 0xF4;
    else if (vout >= 1.980f) return 0xC4 + (int)((vout - 1.98f) / 0.03f);
    else if (vout >= 1.290f) return 0x81 + (int)((vout - 1.29f) / 0.01f);
    else if (vout >= 0.640f) return 0x00 + (int)((vout - 0.64f) / 0.005f);
    else return 0x00;
}

static float vout_reg_to_float(uint8_t reg)
{
    if (reg <= 0x80) return 0.640f + 0.005f * reg;
    else if (reg <= 0xC3) return 1.290f + 0.010f * (reg - 0x81);
    else if (reg <= 0xF4) return 1.980f + 0.030f * (reg - 0xC4);
    else return 4.750f + 0.050f * (reg - 0xF5);
}

// ────── Zephyr Regulator API ──────

static int mic24045_set_voltage(const struct device *dev, int min_uv, int max_uv)
{
    const struct mic24045_config *cfg = dev->config;
    int ret;

    float v_target = min_uv / 1e6f;
    uint8_t reg_val = float_to_vout_reg(v_target);

    // Write the voltage setting to VOUT register
    ret = i2c_reg_write_byte_dt(&cfg->i2c, MIC24045_REG_VOUT, reg_val);
    if (ret < 0) {
        return ret;
    }

    // According to MIC24045 datasheet, we may need to send a command to apply the setting
    // The CIFF (Command Interface) bit might need to be set
    ret = i2c_reg_write_byte_dt(&cfg->i2c, MIC24045_REG_COMMAND, MIC24045_CIFF);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

static int mic24045_get_voltage(const struct device *dev, int *voltage_uv)
{
    const struct mic24045_config *cfg = dev->config;
    uint8_t reg_val;
    int ret = i2c_reg_read_byte_dt(&cfg->i2c, MIC24045_REG_VOUT, &reg_val);
    if (ret < 0) {
        return ret;
    }

    *voltage_uv = (int)(vout_reg_to_float(reg_val) * 1e6f);
    return 0;
}

static int mic24045_enable(const struct device *dev)
{
    // Optional: if EN pin is controlled via GPIO, implement here.
    return 0;
}

static int mic24045_disable(const struct device *dev)
{
    // Optional: if EN pin is controlled via GPIO, implement here.
    return 0;
}

static int mic24045_count_current_limits(const struct device *dev)
{
    return ((MIC24045_MAX_CURRENT_UA - MIC24045_MIN_CURRENT_UA) / MIC24045_CURRENT_STEP_UA) + 1;
}

static int mic24045_list_current_limit(const struct device *dev, uint32_t index, int32_t *limit_ma)
{
    int total = mic24045_count_current_limits(dev);
    if (index >= total) {
        return -EINVAL;
    }

    *limit_ma = MIC24045_MIN_CURRENT_UA + index * MIC24045_CURRENT_STEP_UA;
    return 0;
}

static int mic24045_set_current_limit(const struct device *dev, int32_t min_ma, int32_t max_ma)
{
    const struct mic24045_config *cfg = dev->config;
    int desired_ma = min_ma;

    // Clamp to supported range
    if (desired_ma < MIC24045_MIN_CURRENT_UA || desired_ma > MIC24045_MAX_CURRENT_UA) {
        return -EINVAL;
    }

    uint8_t steps = (desired_ma - MIC24045_MIN_CURRENT_UA) / MIC24045_CURRENT_STEP_UA;
    uint8_t reg_val = 0;

    int ret = i2c_reg_read_byte_dt(&cfg->i2c, MIC24045_REG_SETTINGS1, &reg_val);
    if (ret < 0) return ret;

     uint8_t idx = (desired_ma - MIC24045_MIN_CURRENT_UA)
                   / MIC24045_CURRENT_STEP_UA;

    /* 4) clear old ILIM bits, insert new bits into [7:6] */
    reg_val = (reg_val & ~MIC24045_ILIM_MASK)
        | (idx << MIC24045_ILIM_SHIFT);

    /* 5) write it back */
    return i2c_reg_write_byte_dt(&cfg->i2c,
                                 MIC24045_REG_SETTINGS1,
                                 reg_val);


    // // Assuming current limit is in bits [3:0] (placeholder — check datasheet!)
    // reg_val = (reg_val & 0xF0) | (steps & 0x0F);

    // return i2c_reg_write_byte_dt(&cfg->i2c, MIC24045_REG_SETTINGS1, reg_val);
}

static int mic24045_get_current_limit(const struct device *dev,
                                      int32_t *curr_ua)
{
    const struct mic24045_config *cfg = dev->config;
    uint8_t reg_val;
    int ret = i2c_reg_read_byte_dt(&cfg->i2c,
                                   MIC24045_REG_SETTINGS1,
                                   &reg_val);
    if (ret < 0) {
        return ret;
    }

    /* Extract the 2-bit ILIM<1:0> field from bits 7–6 */
    uint8_t idx = (reg_val & MIC24045_ILIM_MASK)
                  >> MIC24045_ILIM_SHIFT;

    /* Map to the 2 A,3 A,4 A,5 A settings */
    const int32_t limits_ma[4] = {2000, 3000, 4000, 5000};
    int32_t current_ma = limits_ma[idx];

    /* Return in μA */
    *curr_ua = current_ma * 1000;
    return 0;
}


// ────── Initialization ──────

static int mic24045_init(const struct device *dev)
{
    const struct mic24045_config *cfg = dev->config;

    if (!device_is_ready(cfg->i2c.bus)) {
        LOG_ERR("I2C bus not ready");
        return -ENODEV;
    }

    // //Read status register to ensure device is responsive
    // uint8_t status;
    // int ret = i2c_reg_read_byte_dt(&cfg->i2c, MIC24045_REG_STATUS, &status);
    // if (ret < 0) {
    //     LOG_ERR("Failed to read MIC24045 status: %d", ret);
    //     return ret;
    // }
    // LOG_INF("MIC24045 initialized, status: 0x%02X", status);

    return 0;
}

// ────── Zephyr Driver Registration ──────

static const struct regulator_driver_api mic24045_api = {
    .set_voltage = mic24045_set_voltage,
    .get_voltage = mic24045_get_voltage,
    .enable = mic24045_enable,
    .disable = mic24045_disable,
    .count_current_limits = mic24045_count_current_limits,
    .list_current_limit = mic24045_list_current_limit,
    .set_current_limit = mic24045_set_current_limit,
    .get_current_limit = mic24045_get_current_limit,
};

#define MIC24045_DEFINE(inst) \
    static const struct mic24045_config mic24045_config_##inst = { \
        .common = { \
            .min_uv = DT_INST_PROP_OR(inst, regulator_min_microvolt, 640000), \
            .max_uv = DT_INST_PROP_OR(inst, regulator_max_microvolt, 5250000), \
            .init_uv = DT_INST_PROP_OR(inst, regulator_init_microvolt, 0), \
            .min_ua = DT_INST_PROP_OR(inst, regulator_min_microamp, 500000), \
            .max_ua = DT_INST_PROP_OR(inst, regulator_max_microamp, 5000000), \
        }, \
        .i2c = I2C_DT_SPEC_INST_GET(inst) \
    }; \
    DEVICE_DT_INST_DEFINE(inst, mic24045_init, NULL, \
                          NULL, &mic24045_config_##inst, \
                          POST_KERNEL, CONFIG_REGULATOR_MIC24045_INIT_PRIORITY, \
                          &mic24045_api);

DT_INST_FOREACH_STATUS_OKAY(MIC24045_DEFINE)
