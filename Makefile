ifndef RISCV
$(error RISCV is unset)
else
$(info Running with RISCV=$(RISCV))
endif

PREFIX ?= $RISCV/
SRC_DIR := src
SRCS= $(SRC_DIR)/sifive_uart.cc $(SRC_DIR)/iceblk.cc
UTIL_OBJS := $(SRC_DIR)/cutils.o $(SRC_DIR)/fs.o $(SRC_DIR)/fs_disk.o
UTIL_OBJS +=$(addprefix $(SRC_DIR)/slirp/, slirp.o bootp.o ip_icmp.o mbuf.o tcp_output.o cksum.o ip_input.o misc.o socket.o tcp_subr.o udp.o if.o ip_output.o sbuf.o tcp_input.o tcp_timer.o)

DEVICE_DLIBS := libspikedevices.so  libvirtio9pdiskdevice.so libvirtioblockdevice.so libvirtionetdevice.so 

VIRTIO_CFLAGS=-O2 -Wall -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -MMD
VIRTIO_CFLAGS+=-D_GNU_SOURCE -fPIC -DCONFIG_SLIRP

default: all

all: $(DEVICE_DLIBS)

$(SRC_DIR)/fs_disk.o : $(SRC_DIR)/fs_disk.c $(SRC_DIR)/list.h
	gcc $(VIRTIO_CFLAGS) -c -o $@ $<

$(UTIL_OBJS: %.o) : %.c %.h
	gcc $(VIRTIO_CFLAGS) -c -o $@ $^

$(SRC_DIR)/slirp/%.o: $(SRC_DIR)/slirp/%.c
	$(CC) $(VIRTIO_CFLAGS) -c $< -o $@

virtio_base.o : $(SRC_DIR)/virtio.cc $(SRC_DIR)/virtio.h 
	g++ -L $(RISCV)/lib -c -o $@ -std=c++17 -I $(RISCV)/include -isystem $(RISCV)/include/fdt -isystem $(RISCV)/include/riscv -isystem $(RISCV)/include/softfloat -isystem $(RISCV)/include/fesvr -isystem $(RISCV)/include/adele  -fPIC $< 

libvirtio9pdiskdevice.so : $(SRC_DIR)/virtio-9p-disk.cc $(SRC_DIR)/virtio-9p-disk.h virtio_base.o $(UTIL_OBJS)
	g++ -L $(RISCV)/lib -Wl,-rpath,$(RISCV)/lib -shared -o $@ -std=c++17 -I $(RISCV)/include -isystem $(RISCV)/include/fdt -isystem $(RISCV)/include/riscv  -isystem $(RISCV)/include/softfloat -isystem $(RISCV)/include/fesvr -isystem $(RISCV)/include/adele  -fPIC $< virtio_base.o $(UTIL_OBJS)

libvirtionetdevice.so : $(SRC_DIR)/virtio-net.cc $(SRC_DIR)/virtio-net.h virtio_base.o $(UTIL_OBJS)
	g++  -DCONFIG_SLIRP -L $(RISCV)/lib -Wl,-rpath,$(RISCV)/lib -shared -o $@ -std=c++17 -I $(RISCV)/include -isystem $(RISCV)/include/fdt -isystem $(RISCV)/include/riscv  -isystem $(RISCV)/include/softfloat -isystem $(RISCV)/include/fesvr -isystem $(RISCV)/include/adele  -fPIC $< virtio_base.o $(UTIL_OBJS)


libvirtioblockdevice.so : $(SRC_DIR)/virtio-block.cc $(SRC_DIR)/virtio-block.h virtio_base.o $(UTIL_OBJS)
	g++ -L $(RISCV)/lib -Wl,-rpath,$(RISCV)/lib -shared -o $@ -std=c++17 -I $(RISCV)/include -isystem $(RISCV)/include/fdt -fPIC $< virtio_base.o $(UTIL_OBJS)

libspikedevices.so: $(SRCS)
	g++ -L $(RISCV)/lib -Wl,-rpath,$(RISCV)/lib -shared -o $@ -std=c++17 -I $(RISCV)/include -isystem $(RISCV)/include/fdt -fPIC $^

.PHONY: install
install: $(DEVICE_DLIBS)
	cp $^ $(RISCV)/lib

clean:
	rm -rf *.o *.so src/*.o src/*.d
