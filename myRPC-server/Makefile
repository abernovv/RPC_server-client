# Makefile for myRPC-server

CC        := gcc
CFLAGS    := -Wall -g -I../libmysyslog
LDFLAGS   := -L../libmysyslog -lmysyslog
SRC_DIR   := src
OBJ_DIR   := obj
BIN_DIR   := bin
TARGET    := $(BIN_DIR)/myrpc-server

SRC       := $(wildcard $(SRC_DIR)/*.c)
OBJ       := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))

.PHONY: all clean deb

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) *.deb

deb: clean all
	# Create temporary directory structure
	mkdir -p deb_pkg/usr/bin
	mkdir -p deb_pkg/DEBIAN
	# Copy the executable to usr/bin
	install -m 755 $(TARGET) deb_pkg/usr/bin/
	# Create control file
	echo "Package: myrpc-server" > deb_pkg/DEBIAN/control
	echo "Version: 1.0-1" >> deb_pkg/DEBIAN/control
	echo "Section: net" >> deb_pkg/DEBIAN/control
	echo "Priority: optional" >> deb_pkg/DEBIAN/control
	echo "Architecture: amd64" >> deb_pkg/DEBIAN/control
	echo "Maintainer: Your Name <you@example.com>" >> deb_pkg/DEBIAN/control
	echo "Description: myRPC Server Application" >> deb_pkg/DEBIAN/control
	# Build the package
	fakeroot dpkg-deb --build deb_pkg ../deb/myrpc-server_1.0-1_amd64.deb
	# Clean up temporary files
	rm -rf deb_pkg
