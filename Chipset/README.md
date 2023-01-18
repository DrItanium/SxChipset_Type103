This is a brand new reimplementation of the SxChipset targeting an atmega2560 (originally a 1284p).
The difference with the original SxChipset is that this implementation targets
the type103 design which does things differently compared to type1.

Here are the highlights:

1. Data lines are directly connected to ports H and F (in later designs, I want to move this to PORTJ to free up timer pins)
   - since this is a modification of an older 1284p based design, ioexpander 0 is mapped to the data lines but turned to input exclusively
2. READY, SD_EN, GPIOSelect, and psram are connected over the primary SPI bus using a GAL chip and decoder setup. This part is only indirectly controlled by the i960 itself.
3. Eight megabytes of PSRAM
4. All IO devices are mapped to the upper 256 megabytes by a 28-bit opcode (the upper most 4 bits are all ones). The address is interpreted as an opcode
5. Using the external memory interface, we have access to 512k of sram broken up into 16 32k pages with the upper 8 being exposed directly to the i960 in the address range 0xF1000'0000-0xF1FFF'FFFF. Only 256k is currently accessible but we can go up to 16 megabytes
   - I have updated the firmware to map the stack and several important data structures into this area
   - This area is not cached
6. The lower 256k of the external memory is banked and used to increase the cache from a paltry 4k to roughly 256k or so (it is less than that but the cache is much larger than before)
7. HOLD, HLDA, and RESET are connected to XIO or ioexpander 7. 
8. Timer 2 is mapped to INT0 on the i960 to provide a system timer
9. System has access to an RTC over i2c
10. An External bus known as the 32/8 bus is supposed to be accessible but due to a design screwup this is not possible but in the future it should be possible to access this with a combination of io expanders and free gpios

