clean:
	rm -r ./os161-base-2.0.3/kern/compile/SUCHVM

configure:
	cd ./os161-base-2.0.3/kern/conf && \
	./config SUCHVM

compile: configure
	cd ./os161-base-2.0.3/kern/compile/SUCHVM && \
	bmake depend && \
	bmake && \
	bmake install

debug: compile
	cd root && \
	gnome-terminal -- sys161 -w kernel-SUCHVM && \
	sleep 0.5 && \
	ddd --debugger mips-harvard-os161-gdb kernel-SUCHVM