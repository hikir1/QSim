all:
	gcc -o qsim src/*.c -lm

clean:
	rm qsim
