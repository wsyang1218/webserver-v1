CXX = g++
CXXFLAGS = -std=c++14
# DEBUG ?= 1
# ifeq ($(DEBUG), 1)
#     CXXFLAGS += -g
# else
#     CXXFLAGS += -O2

# endif

server: src/main.cpp  src/logger/logger.cpp src/timer/lst_timer.cpp src/http/http_conn.cpp src/server.cpp 
	$(CXX) -o bin/server  $^ $(CXXFLAGS) -pthread

clean:
	rm  -r bin/server
