CXX = g++
CXXFLAGS = -std=c++14
# DEBUG ?= 1
# ifeq ($(DEBUG), 1)
#     CXXFLAGS += -g
# else
#     CXXFLAGS += -O2

# endif

server: main.cpp  utils/utils.cpp logger/logger.cpp timer/lst_timer.cpp http/http_conn.cpp server.cpp 
	$(CXX) -o server  $^ $(CXXFLAGS) -pthread

clean:
	rm  -r server
