override CFLAGS+=-Wall -Wextra -Werror

.PHONY: all
all: efetch

efetch: $(wildcard src/*.c)
	gcc -o $@ $(CFLAGS) $(CPPFLAGS) $< $(LDLIBS)

.PHONY: install
install: /bin/efetch

/bin/efetch: efetch
	$(shell [ -f /bin/efetch ] && rm -rf /bin/efetch)
# make a new symlink for efetch
	@echo "Making new symlink for /bin/efetch"
	ln $< $@
	@echo installed


