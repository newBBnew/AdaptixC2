PLUGIN_NAME=listener_beacon_doh.so
GO_FILES=pl_doh.go pl_listener_impl.go pl_main.go

all: clean
	mkdir -p dist
	GOEXPERIMENT=jsonv2,greenteagc go build -buildmode=plugin -o dist/$(PLUGIN_NAME) $(GO_FILES)
	cp ax_config.axs dist/
	cp config.json dist/

clean:
	rm -rf dist
