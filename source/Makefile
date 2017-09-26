.PHONY: all

all: memcpy.exe naive.exe


memcpy.exe: blocked_reorder.o memcpy_main.o
	clang++ -std=c++14 -O3 -fstrict-aliasing blocked_reorder.o memcpy_main.o -o memcpy.exe

naive.exe: blocked_reorder.o naive_main.o
	clang++ -std=c++14 -O3 -fstrict-aliasing blocked_reorder.o naive_main.o -o naive.exe

memcpy_main.o: blocked_reorder_main.cpp blocked_reorder.hpp
	clang++ -std=c++14 -O3 -fstrict-aliasing -c blocked_reorder_main.cpp -DMEMCPY -o memcpy_main.o

naive_main.o: blocked_reorder_main.cpp blocked_reorder.hpp
	clang++ -std=c++14 -O3 -fstrict-aliasing -c blocked_reorder_main.cpp -DNAIVE -o naive_main.o

blocked_reorder.o: blocked_reorder.cpp blocked_reorder.hpp
	clang++ -std=c++14 -O3 -fstrict-aliasing -c blocked_reorder.cpp -o blocked_reorder.o


plot: naive.dat memcpy.dat
	gnuplot -persist -e "set key top left ; set grid ; plot 'naive.dat' w p t 'naive', 'memcpy.dat' w p t 'memcpy'"

naive.dat: naive.exe
	./naive.exe > naive.dat

memcpy.dat: memcpy.exe
	./memcpy.exe > memcpy.dat

clean:
	rm -f *.o *.exe *.dat
