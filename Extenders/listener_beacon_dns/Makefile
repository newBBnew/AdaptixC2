all: clean
	@ echo "    * Building listener_beacon_dns plugin"
	@ mkdir dist
	@ cp config.json ax_config.axs ./dist/
	@ go build -buildmode=plugin -ldflags="-s -w" -o ./dist/listener_beacon_dns.so pl_main.go pl_dns.go pl_listener_impl.go
	@ echo "      done..."

clean:
	@ rm -rf dist
