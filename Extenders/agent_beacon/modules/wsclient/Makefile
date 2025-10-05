CC      := x86_64-w64-mingw32-g++
CFLAGS  := -std=c++17 -Os -Wall -Wextra -fno-exceptions -fno-rtti -static -static-libgcc -static-libstdc++
LDFLAGS := -shared -Wl,--subsystem,windows
TARGET  := build/wsclient_x64.dll
SRC     := wsclient.cpp
OBJ     := $(SRC:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(OBJ) $(LDFLAGS) -o $@

%.o: %.cpp wsclient.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build *.o

.PHONY: all clean
