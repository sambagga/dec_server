CXXFLAGS =	-O2 -g -Wall -fmessage-length=0

OBJS =		dec_server.o

LIBS =

TARGET =	dec_server

$(TARGET):	$(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
