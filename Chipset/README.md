This is the firmware to control an i960Sx series CPU using a Mega2560. I took
design inspiration from the Cyclone Microsystems SDK board from the 1990s where
they use a CPU card style design to allow different i960 CPUs to be plugged
into the development board. I just adapted it to make DDR4 DIMMs the form
factor instead of something different. 

I did this for a few reasons:

# DIMMS are standardized in shape and it is _really_ easy to get sockets for cheap
# I only have to solder the memory sockets, the DIMMs themselves do not require complex or expensive connectors to be purchased installed
# I get access to 288 pins that I can layout however I want
# The DIMM is keyed so you _cannot_ plug it in backwards
# It is literally modular

As of March 24, 2023, my porting effort to go from the old school arduino stack to the DDR4 DIMM design is complete. All that I need to do now is optimize it down

What's really interesting about the Cyclone board is that it forced the CPU cards to present common 32-bit address and data bus regardless of the internals.

The i960Sx CPU card just operated on the upper or lower 16-bits of a 32-bit word at a time. 
So internally, nothing has changed. Externally, you just operate on a generalized interface. 
My plan is to eventually support other CPUs in the i960 family such as the C, H, and J processors. 
Previously, that included the i960Kx series but it does some very goofy things with instruction timing
that make it very difficult to support.

In the process of designing this board I also found out about the WCH351 which is a triple threat. It is a PCI Bus interface chip that can also be turned into a 32-bit IO expander with an 8-bit parallel bus.
I use several of these to interface the i960 with the Mega2560. The DIMM which houses these chips is relatively simple.

This project contains several sub folders:

# Chipset - Firmware for the Mega2560
# IBUSTest - My initial test program for trying out the CH351
# GPU - A failed experiment from the original Type103 days where I wanted to try and use a Dazzler3X with the i960
# MemoryController - An experiment of abstracting the memory interface over i2c. Very slow but interesting to try
# PLD - All of the separate GAL equation files for all of the various designs I've made over this process
# ROM - Back when I used a flash chip for decoding purposes, the code in this folder was relevant, it generated the decode logic as data to be uploaded to the flash chip (SST39SF040 were the chips being used at the time)

Of these folders, the Chipset and PLD directories are still relevant
