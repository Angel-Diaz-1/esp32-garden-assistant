# ESP32 Wi-Fi connectivity test

This standalone sketch joins a local Wi-Fi network and serves a small HTTP test endpoint. It requires no pump or extra hardware. Uploading it replaces the program currently on the ESP32; the garden controller can be uploaded again afterward.

1. Copy `firmware/wifi_connectivity_test/wifi_secrets.example.h` to `wifi_secrets.h` in the same directory. Enter your Wi-Fi name and password in that local copy. Git ignores `wifi_secrets.h`; never put credentials into the example file.
2. Open `firmware/wifi_connectivity_test/wifi_connectivity_test.ino` in Arduino IDE. Use **esp32 by Espressif Systems**, select **NodeMCU-32S**, and select the connected COM port. The WiFi and WebServer libraries are included with this core.
3. Use a 2.4 GHz network with a normal Wi-Fi password. Enterprise authentication and captive portal login are outside this example's scope.
4. Upload, open Serial Monitor at **115200 baud**, then press EN/reset if startup messages were missed.
5. Expect `PASS: Connected to Wi-Fi and received an IP address.` followed by an `http://` address. Connection status and signal strength print every five seconds.
6. Open that exact HTTP address on a phone or computer on the same local network. Expect `PASS: Your browser reached the ESP32 over local Wi-Fi!` and a matching Serial Monitor message. This confirms local HTTP communication, not internet access.
7. To test reconnection, briefly disable the access point (or move the board out of range), then restore it. Expect the disconnection message, reconnection, and the current IP address. Repeat the browser test using the new address if it changed.

If connection never succeeds, check credentials, 2.4 GHz availability, and signal strength. If the ESP32 connects but the browser times out, check guest-network client isolation, VPN routing, and that the URL starts with `http://`. The sample has no connection to the GitHub Pages watering button.

GitHub Actions compiles this sketch for NodeMCU-32S using placeholder credentials. Real network connectivity must be checked on the physical board using the steps above.
