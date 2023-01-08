%.o: %.cc
	@echo -n Compiling $< into $@...
	@g++ -std=c++17 -c $< -o $@
	@echo done.

generate: romgen0
	@echo -n Generating rom images u3 and u10...
	@./romgen0
	@echo done

romgen0: romgen0.o
	@echo -n Linking $@...
	@g++ -o $@ $<
	@echo done

help:
	@echo "Available Targets"
	@echo "- romgen0: generates a program to build flash roms u3 and u10"
	@echo "- generate: Build romgen0 and run it!"
	@echo "- clean: Clean temporary files"

clean:
	@echo -n Cleaning...
	@rm -f *.o u*.bin romgen0
	@echo done

.PHONY: help clean
