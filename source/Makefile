.PHONY: all

flags=-std=c++14 -O3 -fstrict-aliasing -fno-omit-frame-pointer -g

all: memcpy.exe naive.exe


memcpy.exe: blocked_reorder.o memcpy_main.o
	clang++ $(flags)  blocked_reorder.o memcpy_main.o -o memcpy.exe

naive.exe: blocked_reorder.o naive_main.o
	clang++ $(flags) blocked_reorder.o naive_main.o -o naive.exe

memcpy_main.o: blocked_reorder_main.cpp blocked_reorder.hpp
	clang++ $(flags) -c blocked_reorder_main.cpp -DMEMCPY -o memcpy_main.o

naive_main.o: blocked_reorder_main.cpp blocked_reorder.hpp
	clang++ $(flags) -c blocked_reorder_main.cpp -DNAIVE -o naive_main.o

blocked_reorder.o: blocked_reorder.cpp blocked_reorder.hpp
	clang++ $(flags) -c blocked_reorder.cpp -o blocked_reorder.o

perf: naive.exe memcpy.exe
	perf record -g ./naive.exe
	mv perf.data naive.perf
	perf record -g ./memcpy.exe
	mv perf.data memcpy.perf


plot: naive.dat memcpy.dat
	gnuplot -persist -e "set key top left ; set grid ; plot 'naive.dat' w p t 'naive', 'memcpy.dat' w p t 'memcpy'"

naive.dat: naive.exe
	./naive.exe > naive.dat

memcpy.dat: memcpy.exe
	./memcpy.exe > memcpy.dat

clean:
	rm -f *.o *.exe *.dat *.perf
