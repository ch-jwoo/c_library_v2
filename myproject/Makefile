CXX = g++
SRCS = multiUavServer.cpp
OBJS = $(SRCS:.cpp = .o)
TARGET = multiUavServer
INC = -I./..

all : $(TARGET)
		$(CXX) -o $(TARGET) $(OBJS) $(INC)

$(TARGET) : 
		$(CXX) -c $(SRCS) $(INC) 

clean : 
		rm -f $(TARGET)
		rm -f *.o