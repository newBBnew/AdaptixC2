all: clean prepare server client extenders

DIST_DIR := release
RUNTIME_EXT_DIR := extenders

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
  NPROC := $(shell nproc)
endif

ifeq ($(UNAME_S),Darwin)
  NPROC := $(shell sysctl -n hw.ncpu)
endif

prepare:
	@mkdir -p "$(DIST_DIR)"
	@mkdir -p "$(DIST_DIR)/extenders"
	@mkdir -p "$(RUNTIME_EXT_DIR)"

clean:
	@ rm -f $(DIST_DIR)/adaptixserver
	@ rm -f $(DIST_DIR)/AdaptixClient
	@ rm -rf $(DIST_DIR)/extenders
	@ rm -rf $(RUNTIME_EXT_DIR)


server: prepare
	@ echo "[*] Building adaptixserver..."
	@ cd AdaptixServer && go build -buildvcs=false -ldflags="-s -w" -o adaptixserver > /dev/null 2>build_error.log || { echo "[ERROR] Failed to build AdaptixServer:"; cat build_error.log >&2; exit 1; }     # for static build use CGO_ENABLED=0
	@ rm -f $(DIST_DIR)/adaptixserver
	@ mv AdaptixServer/adaptixserver $(DIST_DIR)/
	@ cp AdaptixServer/ssl_gen.sh AdaptixServer/profile.json AdaptixServer/404page.html ./$(DIST_DIR)/
	@ echo "[+] done"

client: prepare
	@ echo "[*] Building AdaptixClient..."
	@ cd AdaptixClient && cmake . > /dev/null 2>cmake_error.log || { echo "[ERROR] CMake failed:"; cat cmake_error.log >&2; exit 1; }
	@ cd AdaptixClient && make --no-print-directory
	@ rm -f $(DIST_DIR)/AdaptixClient
	@ mv ./AdaptixClient/AdaptixClient ./$(DIST_DIR)/
	@ echo "[+] done"

client-fast: prepare
	@ echo "[*] Building AdaptixClient in $(NPROC) threads..."
	@ cd AdaptixClient && cmake . > /dev/null 2>cmake_error.log || { echo "[ERROR] CMake failed:"; cat cmake_error.log >&2; exit 1; }
	@ cd AdaptixClient && make --no-print-directory -j$(NPROC)
	@ rm -f $(DIST_DIR)/AdaptixClient
	@ mv ./AdaptixClient/AdaptixClient ./$(DIST_DIR)/
	@ echo "[+] done"

### Extenders here

EXTENDER_DIRS := $(shell find Extenders -maxdepth 1 -type d -not -path "." -exec test -f {}/Makefile \; -print)
WS_MODULE_DIR := Extenders/agent_beacon/modules/wsclient
WS_MODULE_DLL := $(WS_MODULE_DIR)/build/wsclient_x64.dll
WS_MODULE_TARGET := release/extenders/agent_beacon/modules/wsclient_x64.dll

extenders: prepare
	@ echo "[*] Building default extenders"
	@ for dir in $(EXTENDER_DIRS); do \
		(cd $$dir && $(MAKE) --no-print-directory); \
		plugin_name=$$(basename $$dir); \
		if [ -d "$$dir/dist" ]; then \
			mkdir -p $(DIST_DIR)/extenders/$$plugin_name; \
			mkdir -p $(RUNTIME_EXT_DIR)/$$plugin_name; \
			cp -R $$dir/dist/. $(DIST_DIR)/extenders/$$plugin_name/; \
			cp -R $$dir/dist/. $(RUNTIME_EXT_DIR)/$$plugin_name/; \
			rm -rf $$dir/dist; \
		fi; \
	done
	@ echo "[*] Building WebSocket auxiliary module"
	@ if [ -d "$(WS_MODULE_DIR)" ]; then \
		$(MAKE) --no-print-directory -C $(WS_MODULE_DIR); \
		mkdir -p release/extenders/agent_beacon/modules; \
		cp -f $(WS_MODULE_DLL) $(WS_MODULE_TARGET); \
		cp -f $(WS_MODULE_DLL) Extenders/agent_beacon/modules/wsclient_x64.dll; \
		mkdir -p $(DIST_DIR)/extenders/agent_beacon/modules; \
		cp -f $(WS_MODULE_DLL) $(DIST_DIR)/extenders/agent_beacon/modules/wsclient_x64.dll; \
		mkdir -p $(RUNTIME_EXT_DIR)/agent_beacon/modules; \
		cp -f $(WS_MODULE_DLL) $(RUNTIME_EXT_DIR)/agent_beacon/modules/wsclient_x64.dll; \
	else \
		echo "[WARN] WebSocket module directory not found: $(WS_MODULE_DIR)"; \
	fi
	@ echo "[+] done"

clean-all: clean
	@ echo "[*] Cleaning all build artifacts..."
	@ find . -name "*.o" -delete
	@ find . -name "*.so" -delete
	@ find . -name "*.a" -delete
	@ find . -name "build_error.log" -delete
	@ find . -name "cmake_error.log" -delete
	@ echo "[+] All artifacts cleaned"

help:
	@ echo "AdaptixC2 Build System"
	@ echo ""
	@ echo "Available targets:"
	@ echo "  all         - Build everything (server, client, extenders)"
	@ echo "  server      - Build only the server"
	@ echo "  client      - Build only the client in multithread mode (fast build)"
	@ echo "  client-fast - Build only the client"
	@ echo "  extenders   - Build only the extenders"
	@ echo "  clean       - Remove dist directory"
	@ echo "  clean-all   - Remove all build artifacts"
	@ echo "  help        - Show this help message"
	@ echo ""
	@ echo "Platform: $(UNAME_S) [$(NPROC) proc]"

.PHONY: all server client extenders clean clean-all help prepare
