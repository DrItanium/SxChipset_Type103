%.jed: %.pld
	@echo -n Compiling $< into $@...
	@galette $< 
	@echo done.

all: Type103PLD.jed \
	Type103aPLD.jed \
	Type103aCounter.jed \
	SRAMCardDecoder.jed \
	BusDecoder.jed \
	CPUDimmChip1.jed \
	CPUDimmChip2.jed \
	1MEGDecoderCard.jed



install_Type103: Type103PLD.jed
	minipro -p ATF16V8C --write Type103PLD.jed 

install_Type103a: Type103aPLD.jed
	minipro -p ATF16V8C --write Type103aPLD.jed 

install_SRAMCardDecoder: SRAMCardDecoder.jed
	minipro -p ATF16V8C --write SRAMCardDecoder.jed 

install_Type103aCounter: Type103aCounter.jed
	minipro -p "ATF22V10CQZ(UES)" --write Type103aCounter.jed 

install_BusDecoder: BusDecoder.jed
	minipro -p ATF16V8C --write BusDecoder.jed 

install_CPUDimmChip1: CPUDimmChip1.jed
	minipro -p ATF16V8C --write CPUDimmChip1.jed 

install_CPUDimmChip2: CPUDimmChip2.jed
	minipro -p ATF16V8C --write CPUDimmChip2.jed 

install_1MEGDecoderCard: 1MEGDecoderCard.jed
	minipro -p ATF16V8C --write 1MEGDecoderCard.jed 

help: 
	@echo "Build Options"
	@echo "- all : compile the gal code"
	@echo "- install_Type103 : Program a GAL16V8 with Type103 GAL implementation"
	@echo "- install_Type103a : Program a GAL16V8 with Type103a GAL implementation"
	@echo "- install_SRAMCardDecoder: Program a GAL16V8 with SRAMCardDecoder GAL implementation"
	@echo "- install_Type103aCounter : Program a GAL22V10 with Type103aCounter GAL implementation"
	@echo "- clean : remove all extra files except .pld files"

clean:
	@echo -n "Cleaning..."
	@rm -rf *.chp *.fus *.jed *.pin
	@echo "done"

.PHONY: help clean
