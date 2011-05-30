all: tapencoder.dll tapdecoder.dll

ifdef WITH_SINE
  libtapdecoder.so:ADD_CFLAGS+=-DHAVE_SINE_WAVE
  libtapdecoder.so:ADD_LDFLAGS=-lm
endif

%.dll:ADD_CFLAGS+=-Wl,--out-implib=$*.lib

clean:
	rm -f *.dll *.lib *~ *.so

lib%.so: %.c
	$(CC) $(CFLAGS) $(ADD_CFLAGS) -shared -o $@ $^ $(LDFLAGS) $(ADD_LDFLAGS)
	
%.dll: %.c %.def
	$(CC) $(CFLAGS) $(ADD_CFLAGS) -shared -o $@ $^ $(LDFLAGS) $(ADD_LDFLAGS)
