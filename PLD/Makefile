%.jed: %.pld
	@echo -n Compiling $< into $@...
	@galette $< 
	@echo done.

all: Type103PLD.jed



install: Type103PLD.jed
	minipro -p ATF16V8C --write Type103PLD.jed 

