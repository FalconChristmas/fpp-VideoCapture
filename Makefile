SRCDIR ?= /opt/fpp/src
include $(SRCDIR)/makefiles/common/setup.mk
include $(SRCDIR)/makefiles/platform/*.mk


all: libfpp-VideoCapture.$(SHLIB_EXT)
debug: all

CFLAGS+=-I.



OBJECTS_fpp_VideoCapture_so = src/FPP-VideoCapture.o src/IPVideoCaptureEffect.o src/V4LVideoCaptureEffect.o
LIBS_fpp_VideoCapture_so = -L$(SRCDIR) -lfpp -ljsoncpp -lswscale -lavformat -lavutil -lavcodec -lv4l2

%.o: %.cpp Makefile
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

libfpp-VideoCapture.$(SHLIB_EXT): $(OBJECTS_fpp_VideoCapture_so) $(SRCDIR)/libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_VideoCapture_so) $(LIBS_fpp_VideoCapture_so) $(LDFLAGS) -o $@

clean:
	rm -f libfpp-VideoCapture.$(SHLIB_EXT) $(OBJECTS_fpp_VideoCapture_so)

