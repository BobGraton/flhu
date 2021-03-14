# flhu
FLHU stands for Fast Light Head Unit to play music in a car.
As FLHU uses MPD to seek tracks, create and read playlists, it is really fast and highly responsive.
FLHU is written in C and C++. It controls a DAB / FM card based on the Keystone T2B or T4B DAB / FM / RDS module.
A basic Bluetooth A2DP / HSP phone support has been implemented, with the ability to play / pause / previous / next medias from the phone. As bluealsa is used here, Pulse Audio is not required in this project. Sound is directly send to an external high quality USB DAC. As MPD is at the heart of this project, FLACS are mostly used.
When using a Raspberry pi, DAB and FM programs go through its I2S interface. Also a 5 ways navigation knob enables Pause / Play / next song or station / previous song or station / Volume up / Volume down / toggle between Navit and FLHU for complete integration, using Device-Tree bindings for input/keyboard/gpio_keys.c keyboard driver.

