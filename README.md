# MIC24045 Zephyr Driver

A Zephyr RTOS driver for the Microchip MIC24045 I2C Buck Regulator.

## Overview

The MIC24045 is a high-efficiency, synchronous step-down (buck) switching regulator that operates from input voltages up to 5.5V and provides adjustable output voltages from 0.64V to 5.25V. This driver provides full integration with Zephyr's regulator subsystem.

## Features

- ✅ Voltage control (0.64V to 5.25V)
- ✅ Current limit control (2A to 5A)
- ✅ I2C interface support
- ✅ Device tree configuration
- ✅ Zephyr regulator API integration
- ✅ Multi-instance support

## Hardware Requirements

- Microchip MIC24045 buck regulator
- I2C bus connection
- Compatible microcontroller (tested with nRF54L15)

## Installation

### Method 1: As a Zephyr Module (Recommended)

1. **Clone or copy the driver to your project:**
   ```bash
   # In your project root
   mkdir -p modules/mic24045
   # Copy the entire mic24045 driver directory here
   ```

2. **Add to your `west.yml` (if using West):**
   ```yaml
   manifest:
     projects:
       - name: mic24045
         path: modules/mic24045
         url: <your-repo-url>
   ```

3. **Or add to your `CMakeLists.txt`:**
   ```cmake
   # Add the module path
   list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/modules/mic24045)
   ```

### Method 2: Direct Integration

1. **Copy driver files to your project:**
   ```
   your_project/
   ├── drivers/
   │   └── regulator/
   │       └── mic24045/
   │           ├── CMakeLists.txt
   │           ├── Kconfig
   │           ├── mic24045.c
   │           └── mic24045.h
   ├── dts/
   │   └── bindings/
   │       └── regulator/
   │           └── microchip,mic24045.yaml
   ```

2. **Add to your main `CMakeLists.txt`:**
   ```cmake
   add_subdirectory(drivers/regulator/mic24045)
   ```

## Configuration

### Kconfig Options

Enable the driver in your `prj.conf`:

```conf
# Enable regulator subsystem
CONFIG_REGULATOR=y

# Enable MIC24045 driver
CONFIG_REGULATOR_MIC24045=y

# Enable I2C (required)
CONFIG_I2C=y

# Optional: Enable logging
CONFIG_LOG=y
CONFIG_REGULATOR_LOG_LEVEL_DBG=y
```

### Device Tree Configuration

Add the MIC24045 to your device tree overlay (`.overlay` file):

```dts
&i2c0 {
    status = "okay";
    
    mic24045: regulator@69 {
        compatible = "microchip,mic24045";
        reg = <0x69>;  // I2C address
        
        // Voltage limits
        regulator-min-microvolt = <640000>;   // 0.64V
        regulator-max-microvolt = <5250000>;  // 5.25V
        regulator-init-microvolt = <3300000>; // 3.3V initial
        
        // Current limits
        regulator-min-microamp = <2000000>;   // 2A
        regulator-max-microamp = <5000000>;   // 5A
        
        // Optional power management
        regulator-always-on;
        regulator-boot-on;
        
        status = "okay";
    };
};
```

## Usage Examples

### Basic Voltage Control

```c
#include <zephyr/device.h>
#include <zephyr/drivers/regulator.h>

void regulator_example(void)
{
    static const struct device *mic24045_dev = DEVICE_DT_GET(DT_NODELABEL(mic24045));
    
    if (!device_is_ready(mic24045_dev)) {
        printk("Regulator not ready\n");
        return;
    }
    
    // Set voltage to 3.3V
    int ret = regulator_set_voltage(mic24045_dev, 3300000, 3300000);
    if (ret < 0) {
        printk("Failed to set voltage: %d\n", ret);
        return;
    }
    
    // Read back current voltage
    int voltage_uv;
    ret = regulator_get_voltage(mic24045_dev, &voltage_uv);
    if (ret == 0) {
        printk("Current voltage: %d.%03d V\n", 
               voltage_uv / 1000000, 
               (voltage_uv % 1000000) / 1000);
    }
}
```

### Current Limit Control

```c
void current_limit_example(void)
{
    static const struct device *mic24045_dev = DEVICE_DT_GET(DT_NODELABEL(mic24045));
    
    // Set current limit to 3A
    int ret = regulator_set_current_limit(mic24045_dev, 3000000, 3000000);
    if (ret < 0) {
        printk("Failed to set current limit: %d\n", ret);
        return;
    }
    
    // Read back current limit
    int32_t current_ua;
    ret = regulator_get_current_limit(mic24045_dev, &current_ua);
    if (ret == 0) {
        printk("Current limit: %d.%03d A\n", 
               current_ua / 1000000, 
               (current_ua % 1000000) / 1000);
    }
}
```


### Multiple Regulators

```dts
&i2c0 {
    mic24045_1: regulator@69 {
        compatible = "microchip,mic24045";
        reg = <0x69>;
        regulator-min-microvolt = <640000>;
        regulator-max-microvolt = <5250000>;
        status = "okay";
    };
    
    mic24045_2: regulator@6a {
        compatible = "microchip,mic24045";
        reg = <0x6a>;
        regulator-min-microvolt = <640000>;
        regulator-max-microvolt = <5250000>;
        status = "okay";
    };
};
```

```c
void multiple_regulators_example(void)
{
    const struct device *reg1 = DEVICE_DT_GET(DT_NODELABEL(mic24045_1));
    const struct device *reg2 = DEVICE_DT_GET(DT_NODELABEL(mic24045_2));
    
    // Configure first regulator to 3.3V
    regulator_set_voltage(reg1, 3300000, 3300000);
    
    // Configure second regulator to 1.8V
    regulator_set_voltage(reg2, 1800000, 1800000);
}
```

## API Reference

### Supported Functions

| Function | Description |
|----------|-------------|
| `regulator_set_voltage()` | Set output voltage |
| `regulator_get_voltage()` | Get current output voltage |
| `regulator_set_current_limit()` | Set current limit |
| `regulator_get_current_limit()` | Get current limit |
| `regulator_enable()` | Enable regulator (if GPIO control implemented) |
| `regulator_disable()` | Disable regulator (if GPIO control implemented) |
| `regulator_is_enabled()` | Check if regulator is enabled |

### Device Tree Properties

| Property | Type | Description | Default |
|----------|------|-------------|---------|
| `reg` | int | I2C address | Required |
| `regulator-min-microvolt` | int | Minimum voltage (µV) | 640000 |
| `regulator-max-microvolt` | int | Maximum voltage (µV) | 5250000 |
| `regulator-init-microvolt` | int | Initial voltage (µV) | 0 |
| `regulator-min-microamp` | int | Minimum current (µA) | 2000000 |
| `regulator-max-microamp` | int | Maximum current (µA) | 5000000 |
| `regulator-always-on` | bool | Never disable | false |
| `regulator-boot-on` | bool | Enable at boot | false |

## Voltage Ranges

The MIC24045 supports the following voltage ranges with different step sizes:

| Range | Step Size | Register Values |
|-------|-----------|-----------------|
| 0.640V - 1.290V | 5mV | 0x00 - 0x80 |
| 1.290V - 1.980V | 10mV | 0x81 - 0xC3 |
| 1.980V - 3.420V | 30mV | 0xC4 - 0xF4 |
| 4.750V - 5.250V | 50mV | 0xF5 - 0xFF |

## Current Limits

The MIC24045 supports four current limit settings:

| Setting | Current Limit |
|---------|---------------|
| 0b00 | 2A |
| 0b01 | 3A |
| 0b10 | 4A |
| 0b11 | 5A |

## Troubleshooting

### Common Issues

1. **"Regulator not ready"**
   - Check I2C bus configuration
   - Verify I2C address in device tree
   - Ensure MIC24045 is powered and connected

2. **"Failed to set voltage"**
   - Check voltage is within supported range
   - Verify device tree min/max voltage settings
   - Check I2C communication

3. **Driver not found**
   - Ensure `CONFIG_REGULATOR_MIC24045=y` in prj.conf
   - Check module is properly included in build
   - Verify device tree binding is available

### Debug Tips

Enable debug logging:
```conf
CONFIG_LOG=y
CONFIG_REGULATOR_LOG_LEVEL_DBG=y
```

Check device tree compilation:
```bash
# Build and check generated devicetree
west build -t devicetree
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

This driver is provided under the Apache 2.0 license.

## Support

For issues and questions:
- Create an issue in the repository
- Contact the development team
- Check Zephyr documentation for regulator subsystem

## Changelog

### v1.0.0
- Initial release
- Basic voltage control
- Current limit control
- I2C interface support
- Device tree integration
