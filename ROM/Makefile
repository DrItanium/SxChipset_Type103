%.o: %.cc
	@echo -n Compiling $< into $@...
	@g++ -std=c++17 -c $< -o $@
	@echo done.

all: romgen0 romgen1

romgen0: romgen0.o
	@echo -n Linking $@...
	@g++ -o $@ $<
	@echo done

romgen1: romgen1.o
	@echo -n Linking $@...
	@g++ -o $@ $<
	@echo done

help:
	@echo "Available Targets"
	@echo "- romgen0: first decode rom generator program"
	@echo "- romgen1: first decode rom generator program"
	@echo "- all: Build romgen0 and romgen1"


.PHONY: help
