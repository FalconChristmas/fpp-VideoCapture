SRCDIR ?= /opt/fpp/src
include $(SRCDIR)/makefiles/common/setup.mk
include $(SRCDIR)/makefiles/platform/*.mk

all: libfpp-VideoCapture.$(SHLIB_EXT)
debug: all

CFLAGS+=-I.


ifeq "1" "1"
OBJECTS_fpp_VideoCapture_so += src/FPP-VideoCapture.o src/V4LVideoCaptureEffect.o src/IPVideoCaptureEffect.o
LIBS_fpp_VideoCapture_so += -L$(SRCDIR) -lfpp -ljsoncpp -lv4l2
else
OBJECTS_fpp_VideoCapture_so += src/FPP-VideoCapture.o src/LibCameraVideoCaptureEffect.o  src/IPVideoCaptureEffect.o
LIBS_fpp_VideoCapture_so += -L$(SRCDIR) -lfpp -lcamera -ljsoncpp -lswscale -lavformat -lavutil
CXXFLAGS_src/LibCameraVideoCaptureEffect.o += -I$(SRCDIR) -I/usr/include/libcamera
endif

%.o: %.cpp Makefile
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

libfpp-VideoCapture.$(SHLIB_EXT): $(OBJECTS_fpp_VideoCapture_so) $(SRCDIR)/libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_VideoCapture_so) $(LIBS_fpp_VideoCapture_so) $(LDFLAGS) -o $@

clean:
	rm -f libfpp-VideoCapture.$(SHLIB_EXT) $(OBJECTS_fpp_VideoCapture_so)

