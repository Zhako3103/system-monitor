CXX      = g++
CC       = gcc
IMGUI    = imgui/lib
CXXFLAGS = -std=c++17 -Wall -O2 -I. -I$(IMGUI) -I$(IMGUI)/backend -I$(IMGUI)/gl3w \
           -DIMGUI_IMPL_OPENGL_LOADER_GL3W $(shell sdl2-config --cflags)
CFLAGS   = -O2 -I$(IMGUI)/gl3w
LIBS     = $(shell sdl2-config --libs) -lGL -ldl

SRC_CPP  = main.cpp system.cpp mem.cpp network.cpp \
           $(IMGUI)/imgui.cpp $(IMGUI)/imgui_demo.cpp $(IMGUI)/imgui_draw.cpp \
           $(IMGUI)/imgui_tables.cpp $(IMGUI)/imgui_widgets.cpp \
           $(IMGUI)/backend/imgui_impl_sdl.cpp $(IMGUI)/backend/imgui_impl_opengl3.cpp
SRC_C    = $(IMGUI)/gl3w/GL/gl3w.c
OBJS     = $(SRC_CPP:.cpp=.o) $(SRC_C:.c=.o)

monitor: $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LIBS)

%.o: %.cpp header.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) monitor

.PHONY: clean
