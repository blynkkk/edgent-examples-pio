.PHONY: all fw fs clean erase upload uploadfs monitor

PIOENV ?= "esp32"

BUILDDIR ?= ./build/$(PIOENV)
FIRMWARE ?= $(BUILDDIR)/firmware.bin

all: fw #fs

fw:
	@mkdir -p $(BUILDDIR)
	@pio run -e $(PIOENV)
	@cp .pio/build/$(PIOENV)/firmware.bin $(BUILDDIR)
	@cp .pio/build/$(PIOENV)/firmware.elf $(BUILDDIR)

fs:
	@mkdir -p $(BUILDDIR)
	@pio run --target buildfs
	@cp .pio/build/$(PIOENV)/spiffs.bin $(BUILDDIR)

clean:
	-@rm -rf ./build ./.pio

todo:
	@grep -A3 -R TODO ./src

############################
### UPLOAD
############################

erase:
	@pio run -e $(PIOENV) --target erase

upload: fw
	@pio run -e $(PIOENV) --target upload

uploadfs: fs
	@pio run -e $(PIOENV) --target uploadfs

monitor:
	@pio device monitor --quiet
