This repo is a from scratch implementation of the chipset firmware and
PLD source for the i960Sx Type 1.03 (now known as Type 103). The Firmware
directory is for the 1284p based source code. The PLD source code is for a
ATF16V8C chip which provides glue logic.

This design is an evolution of the Type 1 or SxChipset. It views all bus
transactions as instructions being sent from the i960Sx processor. 
