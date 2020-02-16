TARGET = prog

$(TARGET): fib.o mythread.a
	gcc $^ -o $@

fib.o: fib.c
	gcc -c $< -o $@

mythread.a: mythread.o
	ar rcs $@ $^

mythread.o: mythread.c mythread.h
	gcc -c -o $@ $<


clean:
	rm -f *.o *.a $(TARGET)
