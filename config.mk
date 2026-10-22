VERSION = 0.10
PREFIX = /usr/local

# NetBSD stuff
#CPPFLAGS += -I/usr/pkg/include
#LDFLAGS += -L/usr/pkg/lib -Wl,-R/usr/pkg/lib

# libusb-1.0 flags are detected by pkg-config, set them manually without it
# (e.g. macOS with Homebrew libusb on Apple Silicon)
#LIBUSB_CFLAGS = -I/opt/homebrew/opt/libusb/include/libusb-1.0
#LIBUSB_LIBS = -L/opt/homebrew/opt/libusb/lib -lusb-1.0
