.SILENT:

default: gcc run

gcc:
	gcc -o main main.c -lpthread

run:
	./main

clean:
	rm -rf main
