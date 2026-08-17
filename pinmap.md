| FUNC         | PIN | ------ | PIN | FUNC         |
|--------------|-----|--------|-----|--------------|
|              | 3V3 |        | GND |              |
|              | 3V3 |        | 44  | TX           |
|              | RST |        | 43  | RX           |
| MOTOR-1A     | 4   |        | 1   | TX RP3       |
| MOTOR-1B     | 5   |        | 2   | RX RP3       |
| MOTOR-2A     | 6   |        | 42  | ENC-1A       |
| MOTOR-2B     | 7   |        | 41  | ENC-1B       |
| MOTOR-3A     | 15  |        | 40  | ENC-2A       |
| MOTOR-3B     | 16  |        | 39  | ENC-2A       |
| MOTOR-4A     | 17  |        | 38  |              |
| MOTOR-4B     | 18  |        | 37  | PSRAM        |
| SDA          | 8   |        | 36  | PSRAM        |
| BNO085 INT   | 3   |        | 35  | PSRAM        |
| BNO085 RST   | 46  |        | 0   |              |
| SCL          | 9   |        | 45  |              |
| BNO085 CS    | 10  |        | 48  | BUILTIN LED  |
| CS           | 11  |        | 47  |              |
| MOSI         | 12  |        | 21  | ENC-3A       |
| MISO         | 13  |        | 20  | USB D+       |
| CLK          | 14  |        | 19  | USB D-       |
| ENC-3B       | 22  |        | 34  | PSRAM        |
| ENC-4A       | 23  |        | 33  | PSRAM        |
| ENC-4B       | 24  |        | 32  | PSRAM        |
|              | 5V  |        | GND |              |
|              | GND |        | GND |              |

Note: GPIO26–37 are reserved by the ESP32-S3 octal PSRAM and must not be used for application I/O. The motor 3 and motor 4 encoder signals were moved from GPIO38 and GPIO35–37 to GPIO21–24.
