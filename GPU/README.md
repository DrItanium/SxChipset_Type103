This code targets an atmega4809 that the primary chipset processor talks to
over i2c (UART is also possible). Currently, it is i2c because one of the
secondary UART pins on the 1284p is consumed by a psram enable line.

Regardless, this coprocessor talks to a dazzler 3x shield from excamera labs
via a breakout shield. I have to do it this way because the breakout shield
does not tri state the MISO/CIPO line from the dazzler. So it prevents other
devices from being accessible on the SPI bus at the same time.

The protocol is as follows:

byte 0: opcode
bytes [1,31]: data

Up to 31 bytes of data can be sent plus the one byte for the opcode. This is
done because the Wire library only has a 32 byte buffer by default.
