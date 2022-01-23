clean:
	rm -r ./os161-base-2.0.3/kern/compile/PAGING

configure:
	cd ./os161-base-2.0.3/kern/conf && \
	./config PAGING

compile: configure
	cd ./os161-base-2.0.3/kern/compile/PAGING && \
	bmake depend && \
	bmake && \
	bmake install

debug: compile
	cd root && \
	gnome-terminal -- sys161 -w kernel-PAGING && \
	sleep 0.5 && \
	ddd --debugger mips-harvard-os161-gdb kernel-PAGING

justdebug:
	cd root && \
	gnome-terminal -- sys161 -w kernel-PAGING && \
	sleep 0.5 && \
	ddd --debugger mips-harvard-os161-gdb kernel-PAGING

run:
	cd root && \
	sys161 kernel-PAGING

pdf:
	pandoc README.md \
		-V linkcolor:blue \
		-V geometry:a4paper \
		-V geometry:margin=3cm \
		--toc \
		-o ../report.pdf
	evince ../report.pdf

dumbclean:
	rm -r ./os161-base-2.0.3/kern/compile/DUMBVM

dumbconfigure:
	cd ./os161-base-2.0.3/kern/conf && \
	./config DUMBVM

dumbcompile: dumbconfigure
	cd ./os161-base-2.0.3/kern/compile/DUMBVM && \
	bmake depend && \
	bmake && \
	bmake install

dumbrun:
	cd root && \
	sys161 kernel-DUMBVM