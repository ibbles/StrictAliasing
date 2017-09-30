.PHONY: all

flags=-std=c++14 -O3 -fno-strict-aliasing -fno-omit-frame-pointer -g

all: memcpy.exe naive.exe


memcpy.exe: blocked_reorder.o memcpy_main.o Makefile
	clang++ $(flags)  blocked_reorder.o memcpy_main.o -o memcpy.exe

naive.exe: blocked_reorder.o naive_main.o Makefile
	clang++ $(flags) blocked_reorder.o naive_main.o -o naive.exe

memcpy_main.o: blocked_reorder_main.cpp blocked_reorder.hpp Makefile
	clang++ $(flags) -c blocked_reorder_main.cpp -DMEMCPY -o memcpy_main.o

naive_main.o: blocked_reorder_main.cpp blocked_reorder.hpp Makefile
	clang++ $(flags) -c blocked_reorder_main.cpp -DNAIVE -o naive_main.o

blocked_reorder.o: blocked_reorder.cpp blocked_reorder.hpp Makefile
	clang++ $(flags) -c blocked_reorder.cpp -o blocked_reorder.o

perf: naive.exe memcpy.exe Makefile
	perf record -g ./naive.exe
	mv perf.data naive.perf
	perf record -g ./memcpy.exe
	mv perf.data memcpy.perf


plot: naive.dat memcpy.dat Makefile
	gnuplot -persist -e "set key top left ; set grid ; plot 'naive.dat' w p t 'naive', 'memcpy.dat' w p t 'memcpy'"

naive.dat: naive.exe Makefile
	./naive.exe > naive.dat

memcpy.dat: memcpy.exe Makefile
	./memcpy.exe > memcpy.dat




matrix_vector.exe: matrix_vector.o matrix_vector_main.o Makefile
	clang++ $(flags) matrix_vector.o matrix_vector_main.o -o matrix_vector.exe

matrix_vector.o: matrix_vector.hpp matrix_vector.cpp Makefile
	clang++ $(flags) -c matrix_vector.cpp -o matrix_vector.o

matrix_vector_main.o: matrix_vector.hpp matrix_vector_main.cpp Makefile
	clang++ $(flags) -c matrix_vector_main.cpp -o matrix_vector_main.o


clean:
	rm -f *.o *.exe *.dat *.perf
