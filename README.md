# 7-Segment
Custom large display for clock or timer

This is on MakerWorld at: https://makerworld.com/en/models/1581853-yuge-7-segment-display-1290mm-50-8-wide-clock#profileId-1664897


Make a massive 50.8" wide clock on your H2D, or 40.6" on your standard-size printer.

Use 4 segments for HH:MM or 6 for HH:MM:SS. Prints with 2-colors in place with no support. 

Recommended parts:

Power Supply: https://www.amazon.com/dp/B0C1ZVW7B9?ref=ppx_yo2ov_dt_b_fed_asin_title
 
ESP32 with Breakout Board (just need one of them): https://www.amazon.com/dp/B0DPS44HCQ

Buttons (need two): https://www.amazon.com/dp/B01E38OS7K

5v WS2811 LED strings (21 per segment): https://tinyurl.com/5y28k6ad

Optional magnets to help hold them in alignment (or you can glue them): https://www.amazon.com/dp/B09WRF51KY

Some extra wire, and possibly 3-pin connectors made for LED strings.



Note: I have also included a model with a decimal in case you want it for another purpose. 

One button cycles through 5 brightness levels (ESP32 PIN 14), the other one cycles color modes (ESP32 PIN 27). There are 33 color modes, plus three auto-cycle modes. The first auto-cycle mode (when you see a white flash) changes the mode once per second. The second auto mode does it once per minute. The third does it once per hour. Mode is saved in EEPROM in case of power loss.

Build the code in Arduino IDE. Use ESP32 Dev Module as the board type, need to hold down the boot button when loading the code. Need libraries: Adafruit NeoPixel 1.15.1, and WiFi Manager 2.0.17 by tzapu.

Adjust for your time-zone.

Has WiFi to sync every hour (use phone to connect to the clock to provide your WiFi password).

You can slide the breakout board into the slot at the bottom, or use M3 heat-set inserts if you want to mount it on the mount area that only the larger model has.

You can design and print clamps to hold the digital together if you don't want to glue them, or use cheap Harbor-Freight plastic clamps.

Connect LED string to PIN 13.

Plug in LEDs as per my photo, and wire from the end of one string to the beginning of the next.






 

