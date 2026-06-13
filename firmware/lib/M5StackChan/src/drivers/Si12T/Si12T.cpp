#include "Si12T.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"

static const char *TAG = "Si12T";

/**
 * @brief Si12T设备结构体
 */
struct si12t_dev_t {
    m5::I2C_Class *i2c;              /*!< I2C bus */
    uint8_t dev_addr;                /*!< 设备地址 */
    uint32_t freq;                   /*!< I2C clock */
};

uint8_t si12t_point_type[3] = {SI12T_OUTPUT_NONE, SI12T_OUTPUT_NONE, SI12T_OUTPUT_NONE};

/**
 * @brief 写寄存器
 */
static esp_err_t si12t_i2c_write_reg(si12t_handle_t handle, uint8_t reg_addr, uint8_t value)
{
    if (handle == NULL || handle->i2c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return handle->i2c->writeRegister8(handle->dev_addr, reg_addr, value, handle->freq) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief 读寄存器
 */
static esp_err_t si12t_i2c_read_reg(si12t_handle_t handle, uint8_t reg_addr, uint8_t *value)
{
    if (handle == NULL || handle->i2c == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = handle->i2c->readRegister(handle->dev_addr, reg_addr, value, 1, handle->freq) ? ESP_OK : ESP_FAIL;
    ESP_LOGD(TAG, "Read reg 0x%02x, value: 0x%02x", reg_addr, *value);
    return ret;
}

/**
 * @brief 设置所有灵敏度寄存器
 */
static esp_err_t si12t_set_sens(si12t_handle_t handle, uint8_t value)
{
    esp_err_t ret = ESP_OK;

    ret |= si12t_i2c_write_reg(handle, SI12T_SENSITIVITY1_ADDR, value);
    ret |= si12t_i2c_write_reg(handle, SI12T_SENSITIVITY2_ADDR, value);
    ret |= si12t_i2c_write_reg(handle, SI12T_SENSITIVITY3_ADDR, value);
    ret |= si12t_i2c_write_reg(handle, SI12T_SENSITIVITY4_ADDR, value);
    ret |= si12t_i2c_write_reg(handle, SI12T_SENSITIVITY5_ADDR, value);

    return ret;
}

esp_err_t si12t_init(const si12t_config_t *config, si12t_handle_t *handle)
{
    if (config == NULL || handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    si12t_handle_t dev = (si12t_handle_t)calloc(1, sizeof(struct si12t_dev_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for Si12T device");
        return ESP_ERR_NO_MEM;
    }

    dev->dev_addr = config->dev_addr ? config->dev_addr : SI12T_GND_ADDRESS;
    dev->i2c      = config->i2c ? config->i2c : &M5.In_I2C;
    dev->freq     = config->freq ? config->freq : 100000;

    if (!dev->i2c->isEnabled()) {
        ESP_LOGE(TAG, "I2C bus is not enabled");
        free(dev);
        return ESP_ERR_INVALID_STATE;
    }

    bool scanResult[120] = {};
    dev->i2c->scanID(scanResult);
    if (dev->dev_addr >= sizeof(scanResult) || !scanResult[dev->dev_addr]) {
        ESP_LOGE(TAG, "Si12T not found at 0x%02x", dev->dev_addr);
        free(dev);
        return ESP_ERR_NOT_FOUND;
    }

    *handle = dev;
    ESP_LOGI(TAG, "Si12T initialized, version: %s", SI12T_VERSION);
    return ESP_OK;
}

esp_err_t si12t_setup(si12t_handle_t handle, si12t_type_t sens_type, si12t_sensitivity_level_t sens_level)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;

    ret |= si12t_enable_channel(handle);
    ret |= si12t_set_ctrl2(handle);
    ret |= si12t_set_ctrl1(handle);
    ret |= si12t_set_sensitivity(handle, sens_type, sens_level);
    ret |= si12t_get_sensitivity(handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Si12T setup done");
    }
    return ret;
}

esp_err_t si12t_delete(si12t_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    free(handle);
    return ESP_OK;
}

esp_err_t si12t_get_sensitivity(si12t_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data  = 0;
    esp_err_t ret = ESP_OK;

    ret |= si12t_i2c_read_reg(handle, SI12T_SENSITIVITY1_ADDR, &data);
    ret |= si12t_i2c_read_reg(handle, SI12T_SENSITIVITY2_ADDR, &data);
    ret |= si12t_i2c_read_reg(handle, SI12T_SENSITIVITY3_ADDR, &data);
    ret |= si12t_i2c_read_reg(handle, SI12T_SENSITIVITY4_ADDR, &data);
    ret |= si12t_i2c_read_reg(handle, SI12T_SENSITIVITY5_ADDR, &data);

    return ret;
}

esp_err_t si12t_set_sensitivity(si12t_handle_t handle, si12t_type_t sens_type, si12t_sensitivity_level_t sens_level)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (sens_type != SI12T_TYPE_LOW && sens_type != SI12T_TYPE_HIGH) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t value = 0x00;

    if (sens_type == SI12T_TYPE_HIGH) {
        switch (sens_level) {
            case SI12T_SENSITIVITY_LEVEL_0:
                value = 0x88;
                break;
            case SI12T_SENSITIVITY_LEVEL_1:
                value = 0x99;
                break;
            case SI12T_SENSITIVITY_LEVEL_2:
                value = 0xAA;
                break;
            case SI12T_SENSITIVITY_LEVEL_3:
                value = 0xBB;
                break;
            case SI12T_SENSITIVITY_LEVEL_4:
                value = 0xCC;
                break;
            case SI12T_SENSITIVITY_LEVEL_5:
                value = 0xDD;
                break;
            case SI12T_SENSITIVITY_LEVEL_6:
                value = 0xEE;
                break;
            case SI12T_SENSITIVITY_LEVEL_7:
                value = 0xFF;
                break;
            default:
                return ESP_ERR_INVALID_ARG;
        }
    } else {
        switch (sens_level) {
            case SI12T_SENSITIVITY_LEVEL_0:
                value = 0x00;
                break;
            case SI12T_SENSITIVITY_LEVEL_1:
                value = 0x11;
                break;
            case SI12T_SENSITIVITY_LEVEL_2:
                value = 0x22;
                break;
            case SI12T_SENSITIVITY_LEVEL_3:
                value = 0x33;
                break;
            case SI12T_SENSITIVITY_LEVEL_4:
                value = 0x44;
                break;
            case SI12T_SENSITIVITY_LEVEL_5:
                value = 0x55;
                break;
            case SI12T_SENSITIVITY_LEVEL_6:
                value = 0x66;
                break;
            case SI12T_SENSITIVITY_LEVEL_7:
                value = 0x77;
                break;
            default:
                return ESP_ERR_INVALID_ARG;
        }
    }

    ESP_LOGD(TAG, "Set sensitivity value: 0x%02x", value);
    return si12t_set_sens(handle, value);
}

esp_err_t si12t_set_ctrl1(si12t_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t test;
    // sends register data, Auto Mode, FTC=01, Interrupt(Middle,High), Response 4 (2+2)
    esp_err_t ret = si12t_i2c_write_reg(handle, SI12T_CTRL1_ADDR, 0x22);
    si12t_i2c_read_reg(handle, SI12T_CTRL1_ADDR, &test);
    return ret;
}

esp_err_t si12t_set_ctrl2(si12t_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t test;
    esp_err_t ret = ESP_OK;
    // S/W Reset Enable, Sleep Mode Enable
    ret |= si12t_i2c_write_reg(handle, SI12T_CTRL2_ADDR, 0x0F);
    ret |= si12t_i2c_write_reg(handle, SI12T_CTRL2_ADDR, 0x07);
    si12t_i2c_read_reg(handle, SI12T_CTRL2_ADDR, &test);
    return ret;
}

esp_err_t si12t_sleep_enable(si12t_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return si12t_i2c_write_reg(handle, SI12T_CTRL2_ADDR, 0x07);  // S/W Reset Enable, Sleep Mode Enable
}

esp_err_t si12t_sleep_disable(si12t_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return si12t_i2c_write_reg(handle, SI12T_CTRL2_ADDR, 0x03);  // S/W Reset Disable, Sleep Mode Disable
}

esp_err_t si12t_enable_channel(si12t_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    uint8_t data  = 1;

    ret |= si12t_i2c_write_reg(handle, SI12T_REF_RST1_ADDR, 0x00);  // channel 1-8 enable reference calibration
    ret |= si12t_i2c_write_reg(handle, SI12T_REF_RST2_ADDR, 0x00);  // channel 9 enable reference calibration

    ret |= si12t_i2c_write_reg(handle, SI12T_CH_HOLD1_ADDR, 0x00);  // channel 1-8 enable
    ret |= si12t_i2c_write_reg(handle, SI12T_CH_HOLD2_ADDR, 0x00);  // channel 9 enable

    ret |= si12t_i2c_write_reg(handle, SI12T_CAL_HOLD1_ADDR, 0x00);  // channel 1-8 enable reference calibration
    ret |= si12t_i2c_write_reg(handle, SI12T_CAL_HOLD2_ADDR, 0x00);  // channel 9 enable reference calibration

    // Read back to verify
    si12t_i2c_read_reg(handle, SI12T_REF_RST1_ADDR, &data);
    si12t_i2c_read_reg(handle, SI12T_REF_RST2_ADDR, &data);
    si12t_i2c_read_reg(handle, SI12T_CH_HOLD1_ADDR, &data);
    si12t_i2c_read_reg(handle, SI12T_CH_HOLD2_ADDR, &data);
    si12t_i2c_read_reg(handle, SI12T_CAL_HOLD1_ADDR, &data);
    si12t_i2c_read_reg(handle, SI12T_CAL_HOLD2_ADDR, &data);

    return ret;
}

esp_err_t si12t_read_touch_result(si12t_handle_t handle, uint8_t *touch_result)
{
    if (handle == NULL || touch_result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return si12t_i2c_read_reg(handle, SI12T_OUTPUT1_ADDR, touch_result);
}

void si12t_parse_touch_result(uint8_t touch_result)
{
    int index = 0;
    for (int j = 0; j < 6; j += 2) {
        si12t_point_type[index] = (touch_result >> j) & 0x03;
        index++;
    }
}

void si12t_parse_touch_result_to(uint8_t touch_result, uint8_t *parsed_result)
{
    int index = 0;
    for (int j = 0; j < 6; j += 2) {
        parsed_result[index] = (touch_result >> j) & 0x03;
        index++;
    }
}
