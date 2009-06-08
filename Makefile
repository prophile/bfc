bfc: bfc-ppc bfc-x86 bfc-x86-64 bfc-ppc64
	lipo -create -output bfc bfc-ppc bfc-x86 bfc-x86-64 bfc-ppc64

bfc-ppc64:	
	gcc-4.2 -O3 -arch ppc64 -o bfc-ppc64 bfc.c

bfc-ppc: bfc.c
	gcc-4.2 -O3 -arch ppc -o bfc-ppc bfc.c

bfc-x86: bfc.c
	clang -O3 -arch i386 -o bfc-x86 bfc.c

bfc-x86-64: bfc.c
	clang -O3 -arch x86_64 -o bfc-x86-64 bfc.c

clean:
	rm -f bfc bfc-x86 bfc-ppc bfc-x86-64 bfc-ppc64
