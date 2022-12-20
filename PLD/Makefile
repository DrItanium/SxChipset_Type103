%.jed: %.pld
	@echo -n Compiling $< into $@...
	@galette $< 
	@echo done.

all: Type103PLD.jed Type103aPLD.jed Type103aCounter.pld



install_Type103: Type103PLD.jed
	minipro -p ATF16V8C --write Type103PLD.jed 

install_Type103a: Type103aPLD.jed
	minipro -p ATF16V8C --write Type103aPLD.jed 

install_Type103aCounter: Type103aCounter.jed
	minipro -p ATF22LV10CQZ --write Type103aCounter.jed 



help: 
	@echo "Build Options"
	@echo "- all : compile the gal code"
	@echo "- install_Type103 : Program a GAL16V8 with Type103 GAL implementation"
	@echo "- install_Type103a : Program a GAL16V8 with Type103a GAL implementation"
	@echo "- install_Type103aCounter : Program a GAL22V10 with Type103aCounter GAL implementation"
	@echo "- clean : remove all extra files except .pld files"

clean:
	@echo -n "Cleaning..."
	@rm -rf *.chp *.fus *.jed *.pin
	@echo "done"

.PHONY: help clean
