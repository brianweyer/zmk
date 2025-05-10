
# [Wireless Kinesis Advantage BLE](https://github.com/mikewudev/KinesisBLE)

Turns the Kinesis Advantage Keyboard keyboard into a wireless BLE keyboard with a few extra features. Built with Adafruit's nRF52 Feather Arduino boards.

For instructions, see the **[wiki](https://github.com/sysdevmike/KinesisBLE/wiki)**. For more details on the build check out the **[project](https://hackaday.io/project/161578-wireless-ble-kinesis-advantage-custom-controller)**.

This firmware was used with the hardware fork from:
https://github.com/ergodone/KinesisBLE


To build firmware
From app directory do:
CONFIG_ZMK_BLE_EXPERIMENTAL_SEC=y CONFIG_ZMK_BLE_EXPERIMENTAL_CONN=y west build -p -b adafruit_feather_nrf52840 -- -DSHIELD=kinesisble


To restore original Adafruit firmware download from:
https://github.com/adafruit/Adafruit_nRF52_Bootloader/releases
then install with Jlink with following command:
nrfjprog --program feather_nrf52840_express_bootloader-0.4.1_s140_6.1.1.hex --chiperase -f nrf52 --reset