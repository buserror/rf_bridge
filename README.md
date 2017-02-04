
This project implements a RF bridge for 433Mhz devices. The idea is to have a small AVR do the mod/demod and send diggested messasges on it's serial port to a linux box for processing; in this case bridging sensors and switches to a MQTT broker.

This was made to interface my cheap RF433 gizmoz (wall switches, outlets etc) to the home automation, and ultimately the Amazon echo.

The fact the AVR does all the sensitive bits means the linux program doesn't eat a lot of CPU and can concentrate on the applicative parts.

## The AVR Bits
The firmware can run on anything with about 2KB of SRAM, and a clock of 16Mhz. The firmware 'samples' the RF signal from the 433 module at about 64khz and tries to detect edges, then regular pulses, then does some decoding until a 'long low pulse', and forwards it on.

The firmware can also transmit (altho this part isn't finished) using the same message format. The code doesn't try to be too clever and has a 'raw' mode it can be switches into to forward raw pulses straight to the host for analysis. 

The firmware is capable of decoding both frequency based modulation and manchester encoding. It's completely frequency/pattern independent and continuously search for signal in the noise before applying some filtering and hopefully forward the whole diggested message onward.

### Message format
A Typical message will be sent to the host like this, for a dumb switch:

    MA:40553300:19*36!2f

This is a message that already has been decoded as having frequency based encoding, has 0x19 bits, the clock duration was 0x2f timer clock cycle per bit, and the checksum was 0x36.

A more involved message woudl be:

        MM:0028b4206b0b38c0:40*ff!3f

This one has 'M'ancester encoding, 0x40 bits with 0x3f/2 timer ticks per bits.

The reason the timer clock is returned is to be able to reply the message back. More on this later.

