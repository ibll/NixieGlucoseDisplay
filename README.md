# NixieGlucoseDisplay

![Project Image](media/readme.jpeg)

A DIY glucose monitoring display using Nixie tubes that fetches data from a Dexcom continuous glucose monitor (CGM) and displays it on beautiful vintage Nixie tubes.

## Hardware Components

This project uses the following hardware:

- [Nixie Tube Driver Board for 4 Nixie tubes – Omnixie](https://omnixie.com/products/nixie-tube-driver-board-ntdb-kit-for-4-nixie-tubes)
- [Arduino UNO R4 WiFi](https://store-usa.arduino.cc/products/uno-r4-wifi)
- [DGM01 Tubes – INIXIE](https://inixielab.com/products/inixie-dgm-tubes)

## Setup Instructions

### 1. Hardware Assembly

Connect the NTDB to Arduino Uno:

```
    NTDB        Arduino Pins
    --------------------------------
    GND         GND
    DC5V        5V
    DATA        11
    OE          10
    STCP        8
    SHCP        12
    COLON       5 (Not currently used)
    ON/OFF      6 (HVEnable)
    --------------------------------
```

Connect the 12V DC power to the NTDB board.

### 2. Arduino Software Setup

1. Install the Arduino IDE
2. Install required libraries:
   - ArduinoGraphics
   - Arduino_LED_Matrix
   - R4HttpClient (for Arduino UNO R4 WiFi)

3. Create `arduino/arduino_secrets.h` file with your configuration:
```cpp
#define SECRET_SSID "your_wifi_ssid"
#define SECRET_PASS "your_wifi_password"
#define SECRET_ADDRESS "your_server_address"  // IP or hostname of your server
#define SECRET_PORT 5000                       // Port where Python server runs
```

**Tip**: You can copy and modify `arduino/arduino_secrets.h.template` as a starting point.

4. Upload `arduino/glucose.ino` to your Arduino UNO R4 WiFi

### 3. Python Server Setup

1. Install Python 3.7+ and pip
2. Install required Python packages:
```bash
pip install -r requirements.txt
```

Or manually install:
```bash
pip install flask pydexcom
```

3. Set up environment variables for your Dexcom credentials:
```bash
export DEXCOM_USERNAME="your_dexcom_username"
export DEXCOM_PASSWORD="your_dexcom_password"
```

Alternatively, you can modify the `dexcom_data_server.py` file directly (not recommended for security).

4. Run the server:
```bash
python3 dexcom_data_server.py
```

The server will start on port 5000 and provide glucose data in the format:
```
142
steady
2025-09-12 10:34:46.807000-05:00
```

## Features

- **Real-time glucose monitoring**: Fetches data from Dexcom CGM every minute
- **Beautiful Nixie tube display**: Shows glucose values on vintage-style tubes
- **Slot machine animation**: Smooth transitions between glucose readings
- **WiFi connectivity**: Wireless data fetching from your server
- **Cathode poisoning prevention**: Regular tube cycling to maintain longevity
- **Error handling**: Displays error codes for network/server issues
- **LED matrix status display**: Shows current glucose reading and connection status

## API Endpoints

- `GET /` - Returns current glucose data
- `GET /health` - Health check endpoint

## Troubleshooting

### Arduino Issues

- **WiFi connection fails**: Check your WiFi credentials in `arduino_secrets.h`
- **No data displayed**: Verify the Python server is running and accessible
- **Error codes on display**:
  - `F[number]`: Network/HTTP library error
  - `E[number]`: HTTP status error code
  - `Empty`: Server returned empty response
  - `No Data`: Server couldn't fetch glucose reading

### Python Server Issues

- **Authentication errors**: Verify Dexcom credentials are correct
- **Import errors**: Ensure all required packages are installed with pip
- **Server not accessible**: Check firewall settings and network configuration

### Hardware Issues

- **Tubes not lighting**: Check 12V power supply and connections
- **Dim display**: Adjust brightness in Arduino code (`nixieClock.setBrightness()`)
- **Incorrect numbers**: Verify wiring connections to NTDB board

## Configuration

### Timing Settings

You can adjust various timing parameters in the Arduino code:

- `REQUEST_INTERVAL_MS`: How often to fetch new glucose data (default: 60 seconds)
- `STROBE_INTERVAL_MS`: Cathode poisoning prevention interval (default: 6 minutes)
- `HTTP_TIMEOUT_MS`: HTTP request timeout (default: 3 seconds)

### Display Settings

- Adjust tube brightness: `nixieClock.setBrightness(0xff)`
- Modify animation speed by changing `FRAME_DISPLAY_MS`

## Safety Notes

- This project involves high voltage (170V) for the Nixie tubes
- Use appropriate safety precautions when working with the NTDB board
- This display is for informational purposes only and should not replace medical monitoring devices

## License

This project is provided as-is for educational and personal use.
