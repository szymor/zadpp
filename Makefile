.PHONY: all clean

PROJECTNAME=zadpp

all: $(PROJECTNAME)

$(PROJECTNAME):
	g++ main.cpp -mwindows -o $(PROJECTNAME)

clean:
	-rm -rf $(PROJECTNAME)
