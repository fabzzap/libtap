tap.dll: tap.c tap.def
	$(CC) $(CFLAGS) -shared -Wl,--out-implib=tap.lib -o $@ $^ $(LDFLAGS)

clean:
	rm -f tap.dll tap.lib *~
