EXE = wordwar
OBJECTS = wordwar.o
CFLAGS = -lncurses
$(EXE) : $(OBJECTS) 
	g++ -o $(EXE)  $(OBJECTS) $(CFLAGS) && rm wordwar.o

$(OBJECTS) : wordwar.cpp
	g++ $(CFLAGS) -c wordwar.cpp -o $(OBJECTS)

.PHONY : clean
clean :
	rm $(EXE) $(OBJECTS)
