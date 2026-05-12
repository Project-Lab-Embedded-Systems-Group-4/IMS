# Impedance Measurement System (IMS)

An ESP32-based embedded system designed for high-precision impedance spectroscopy and resistance measurement. This project integrates the **AD5933** Impedance Analyzer and the **AD7680** 16-bit ADC into a service-oriented architecture using the ESP-IDF framework.

## Key Features

- **Automated Frequency Sweeps**: Supports programmable frequency sweeps for impedance spectroscopy.
- **Multi-Channel Multiplexing**: 
    - 10 subject channels for measurements.
    - 3 dedicated calibration channels.
    - 4 selectable feedback resistors for the AD5933 transimpedance amplifier.
- **Advanced Calibration**: Per-frequency gain factor calculation to ensure accuracy across the entire sweep range.
- **Interactive CLI**: Comprehensive console interface for real-time control, hardware debugging, and data dumping.

## Hardware Architecture

- **Microcontroller**: ESP32
- **Impedance Analyzer**: Analog Devices AD5933 (I2C)
- **ADC**: Analog Devices AD7680 (SPI)
- **Analog Front-End**:
    - **ZM_FB MUX**: Selects feedback resistors (2kΩ, 10kΩ, 100kΩ, 330kΩ).
    - **SUBJ MUX**: Switches between 10 measurement channels and 3 calibration resistors (4.7kΩ, 49.9kΩ, 330kΩ).
    - **RM_RANGE MUX**: Selects reference resistors for AD7680 resistance measurements.

## Software Structure

```text
├── components/
│   └── espidf-drivers/      # Low-level hardware drivers (I2C, SPI, GPIO)
├── main/
│   ├── services/            # Background services for AD5933 and AD7680
│   ├── cmd/                 # Console command implementations
│   ├── board.c              # Pin definitions and hardware initialization
│   └── board_utils.c        # Multiplexer and resource locking logic
└── CMakeLists.txt           # Build configuration
```

## Getting Started

### Prerequisites
- ESP-IDF v6.0.1 or later. Follow the [ESP-IDF Get Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) for installation instructions.
- A compatible ESP32 development board.

### Building and Flashing

1. **Set up the environment**:
   Ensure the ESP-IDF environment is initialized in your terminal (e.g., by running `. $HOME/export.sh`).

2. **Clone and Navigate**:
   ```bash
   git clone https://github.com/Project-Lab-Embedded-Systems-Group-4/IMS.git
   cd IMS
   ```

3. **Build and Flash**:
   ```bash
   idf.py build
   idf.py flash monitor
   ```

## Console Commands

### AD5933 (Impedance Analyzer)
The `ad5933` command suite controls the impedance analyzer:

| Subcommand | Description |
| :--- | :--- |
| `info` | Displays current hardware status, temperature, and configuration. |
| `cal -f <0-3>`| Starts an auto-calibration sweep using the specified feedback resistor index. |
| `sweep` | Performs a full frequency sweep on the currently selected channel. |
| `dump` | Displays the results (Real, Imag, Magnitude, Impedance) of the last sweep. |
| `set` | Manually configure start frequency (`-s`), increments (`-i`), or PGA gain (`-p`). |
| `reset` | Performs a hardware reset of the AD5933. |

### AD7680 (Resistance ADC)
The `ad7680` command suite handles high-precision ADC readings:

| Subcommand | Description |
| :--- | :--- |
| `read [-i <n>]`| Requests an ADC reading with optional averaging over `n` iterations. |

### Board Utilities
Low-level control of multiplexers and measurement enable lines:

| Command | Description |
| :--- | :--- |
| `board_info` | Shows current MUX selections (Subject, ZM Feedback, RM Range) and Enable status. |
| `board_set` | Configures the board hardware: Subject channel (`-s`), ZM Feedback (`-f`), RM Range (`-r`), and Enable (`-e`). |

### Example Usage
1. **Calibrate and Sweep Impedance**:
   ```bash
   ims> ad5933 cal -f 1
   ims> ad5933 sweep
   ims> ad5933 dump
   ```

2. **Manual Hardware Configuration**:
   ```bash
   ims> board_set --subj 1 --fb 1 --enable 1
   ims> ad7680 read -i 10
   ```
