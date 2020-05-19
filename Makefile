include /opt/fpp/src/makefiles/common/setup.mk
include /opt/fpp/src/makefiles/platform/*.mk

all: libfpp-VideoCapture.so
debug: all

CFLAGS+=-I.
OBJECTS_fpp_VideoCapture_so += src/FPP-VideoCapture.o src/v4l2Capture.o
LIBS_fpp_VideoCapture_so += -L/opt/fpp/src -lfpp -lv4l2
CXXFLAGS_src/FPP-VideoCapture.o += -I/opt/fpp/src


%.o: %.cpp Makefile
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

libfpp-VideoCapture.so: $(OBJECTS_fpp_VideoCapture_so) /opt/fpp/src/libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_VideoCapture_so) $(LIBS_fpp_VideoCapture_so) $(LDFLAGS) -o $@

clean:
	rm -f libfpp-VideoCapture.so $(OBJECTS_fpp_VideoCapture_so)

