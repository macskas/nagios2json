PREFIX=.
CXX=g++
CXXFLAGS=-mtune=native -Wall -O2 -pipe -I$(PREFIX)/include -I /usr/local/include
LDFLAGS=-L$(PREFIX)/lib -L /usr/lib -L /usr/local/lib

NAGIOSCHECKER=nagios2json
NAGIOSCHECKERSTATIC=nagios2json.static
OBJS=$(patsubst %.cpp,%.o,$(wildcard *.cpp))
all: $(NAGIOSCHECKER) static
$(NAGIOSCHECKER): $(OBJS)
		$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
%.o: %.cpp %.h
		$(CXX) -c $(CXXFLAGS) -o $@ $<
clean:
		rm -f $(NAGIOSCHECKER) $(NAGIOSCHECKERSTATIC) $(OBJS)
static:	$(OBJS)
		$(CXX) $(CXXFLAGS) -o $(NAGIOSCHECKERSTATIC) $^ $(LDFLAGS) -static
