all: tap.dll

ifdef WITH_SINE
  ADD_CFLAGS=-DHAVE_SINE_WAVE
  ADD_LDFLAGS=-lm
endif

%.dll: %.c %.def
	$(CC) $(CFLAGS) -shared -Wl,--out-implib=%.lib -o $@ $^ $(LDFLAGS)

clean:
	rm -f tap.dll tap.lib *~ *.so

lib%.so: %.c
	$(CC) $(CFLAGS) $(ADD_CFLAGS) -shared -o $@ $^ $(LDFLAGS) $(ADD_LDFLAGS)

