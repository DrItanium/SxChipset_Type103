%.o: %.cc
	@echo -n Compiling $< into $@...
	@g++ -std=c++17 -c $< -o $@
	@echo done.

all: romgen0

romgen0: romgen0.o
	@echo -n Linking $@...
	@g++ -o $@ $<
	@echo done


help:
	@echo "Available Targets"
	@echo "- romgen0: generates a program to build flash roms u3 and u10"
	@echo "- all: Build romgen"


.PHONY: help
