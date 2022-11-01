This is a brand new reimplementation of the SxChipset targeting an atmega1284p.
The difference with the original SxChipset is that this implementation targets
the type103 design which does things differently compared to type1.

In type103 we have the following features:

- READY, SD_EN, GPIOSelect, and psram are connected over the primary SPI bus
using a GAL chip and decoder setup. This part is only indirectly controlled by
the i960 itself. 
- Up to 32 megabytes of PSRAM across four eight megabyte PSRAM chips are
supported.
- PD2,3,4 are being used as a second SPI bus that the i960 directly controls
  via a DMA like design. The chipset does the actual assignment of registers
  but through interrupts.
- All 128 possible GPIOs on GPIOSelect are available. The upper 64 pins are
broken up into two 32-bit ports titled A and B. With each port having a direct
connection to an individual i960Sx interrupt pin (1 and 2). 
- 4 way memory cache with round-robin replacement @ 8k of data
- HOLD and HLDA are directly connected to the 1284p to make DMA operations
easier to support
- INT0 and INT3 are directly connected to the 1284p. They are connected to the
A timer block of timer 3 and 2 respectively. This should allow easy
configuration in the future if desired.
- Port A on the 1284p is multiplexed via a decoder rom to allow up to 16
different input sources to be mapped to the 8-bit port. 

Because this is very different compare to how type 1 works, I decided to start
a new from scratch implementation as it will be simpler overall
