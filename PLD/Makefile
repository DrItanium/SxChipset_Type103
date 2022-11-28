%.jed: %.pld
	@echo -n Compiling $< into $@...
	@galette $< 
	@echo done.

all: Type103PLD.jed Type103aPLD.jed



install_Type103: Type103PLD.jed
	minipro -p ATF16V8C --write Type103PLD.jed 

install_Type103a: Type103aPLD.jed
	minipro -p ATF16V8C --write Type103aPLD.jed 

