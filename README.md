# HeartPCB - PCB with LEDs forming an illuminated and animated heart
Firmware and PCB layout for a heart shaped PCB with LEDs.

## About
I designed this PCB and the corresponding firmware as a wedding present for all guests attending. The PCB is the size and shape of a standard coaster, so that it could be used during the party by all guests to put their glass on top of it. When they go home they get a small plastic back containing the components to complete the kit.

The PCB has 18 LEDs, divided into 10 groups and is driven by a) an Arduino Pro Mini of b) statically lit by soldering some pads together.

## Option A
The simple build involves 8 x 47 Ohm and 2 x 150 Ohm resistors, a USB mini connector and 18 red 3mm LEDs.

Solder in place and solder the pads in the middle of the heart together. When plugged in, all LEDs will turn on.

## Option B
When using with an Arduino Pro Mini, there are a number of animations which are played. There are 2 buttons: the brightness button changes the brightness of all LEDs, the fast-forward button switches to the next animation. It supports a demo mode in which animations automatically cycle: hold down brightness until the animation is interrupted and the top-middle LED is on, keep pushing as more and more LEDs come on blinking. The more LEDs blink, the longer the animation is displayed before switching to the next. Set to 0 blinking LEDs to stop automatically forwarding.

The firmware saves the settings and state in the EEPROM (with write wear-levelling) every time the animation changes or the settings are changed.

The component list is 8 x 47 Ohm, 2 x 150 Ohm, 12 x 10k Ohm, USB mini connector, 18 red 3mm LEDs, 2 x 5x5mm push buttons, 1 x 470uF or 1000uF capacitor, 1 x Arduino Pro Mini (ATMEGA328 5V 16MHz)

### Troubleshooting
I had one board getting corrupted after the USB power bank feeding it got empty - my guess is EEPROM corruption. It got stuck loading some invalid stuff and the error LEDs kept turning on.
I fixed this by making error reporting optional and disabled (for production).

If the board does not start correctly due to EEPROM corruption, hold down any button while booting it.
It will now ignore the EEPROM and start the first animation. Press the fast-forward button to skip to the next animation and saving new settings to EEPROM.

## When using this project
Feel free to base your own gift off this design; drop me a note if you do as its nice to hear if this stuff is used again.
