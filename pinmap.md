## Onboard pins
| FUNC         | PIN | ------ | PIN | FUNC         |
|--------------|-----|--------|-----|--------------|
|              | 3V3 |        | GND |              |
|              | 3V3 |        | 44  | TX           |
|              | RST |        | 43  | RX           |
| M1-ENC-A     | 4   |        | 1   | TX RP3       |
| M1-ENC-B     | 5   |        | 2   | RX RP3       |
| M2-ENC-A     | 6   |        | 42  | M1-PWM       |
| M2-ENC-B     | 7   |        | 41  | M2-PWM       |
| M3-ENC-A     | 15  |        | 40  | M3-PWM       |
| M3-ENC-B     | 16  |        | 39  | M4-PWM       |
| M4-ENC-A     | 17  |        | 38  | BRAKE 1      |
| M4-ENC-B     | 18  |        | 37  | PSRAM        |
| SDA          | 8   |        | 36  | PSRAM        |
| BNO085 INT   | 3   |        | 35  | PSRAM        |
| BNO085 RST   | 46  |        | 0   | BOOT STRAP   |
| SCL          | 9   |        | 45  | BRAKE 2      |
| BNO085 CS    | 10  |        | 48  | BUILTIN LED  |
| CLK          | 11  |        | 47  | DIR 1        |
| MOSI         | 12  |        | 21  | DIR 2        |
| MISO         | 13  |        | 20  | USB D+       |
|              | 14  |        | 19  | USB D-       |
|              | 5V  |        | GND |              |
|              | GND |        | GND |              |

## Other pins
| 34  | PSRAM        |
| 33  | PSRAM        |
| 32  | PSRAM        |

Note: GPIO26–37 are reserved by the ESP32-S3 octal PSRAM and must not be used for application I/O. The motor 3 and motor 4 encoder signals were moved from GPIO38 and GPIO35–37 to GPIO21–24.

Motor
|Red|Black|Blue|Green|White|Yellow|Grey|
|-|-|-|-|-|-|-|
|VCC|GND|DIR|Encoder A|PWM speed control|BRAKE|Encoder B|