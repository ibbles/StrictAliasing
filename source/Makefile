.PHONY: all

flags=-std=c++14 -O3 -fno-omit-frame-pointer -g
strict=-fstrict-aliasing
no_strict=-fno-strict-aliasing

all: naive_no_strict.exe naive_strict.exe memcpy_strict.exe memcpy_no_strict.exe



memcpy_no_strict.exe: blocked_reorder_no_strict.o memcpy_main_no_strict.o Makefile
	clang++ $(flags) $(no_strict)  blocked_reorder_no_strict.o memcpy_main_no_strict.o -o memcpy_no_strict.exe

memcpy_strict.exe: blocked_reorder_strict.o memcpy_main_strict.o Makefile
	clang++ $(flags)  $(strict) blocked_reorder_strict.o memcpy_main_strict.o -o memcpy_strict.exe



naive_no_strict.exe: blocked_reorder_no_strict.o naive_main_no_strict.o Makefile
	clang++ $(flags) $(no_strict) blocked_reorder_no_strict.o naive_main_no_strict.o -o naive_no_strict.exe

naive_strict.exe: blocked_reorder_strict.o naive_main_strict.o Makefile
	clang++ $(flags) $(strict) blocked_reorder_strict.o naive_main_strict.o -o naive_strict.exe



memcpy_main_no_strict.o: blocked_reorder_main_no_strict.cpp blocked_reorder_no_strict.hpp Makefile
	clang++ $(flags) -c $(no_strict)blocked_reorder_main_no_strict.cpp -DMEMCPY -o memcpy_main_no_strict.o

memcpy_main_strict.o: blocked_reorder_main_strict.cpp blocked_reorder_strict.hpp Makefile
	clang++ $(flags) -c $(strict)blocked_reorder_main_strict.cpp -DMEMCPY -o memcpy_main_strict.o



naive_main_no_strict.o: blocked_reorder_main.cpp blocked_reorder.hpp Makefile
	clang++ $(flags) -c $(no_strict) blocked_reorder_main.cpp -DNAIVE -o naive_main.o

naive_main_strict.o: blocked_reorder_main.cpp blocked_reorder.hpp Makefile
	clang++ $(flags) -c $(strict) blocked_reorder_main.cpp -DNAIVE -o naive_main.o



blocked_reorder_no_strict.o: blocked_reorder.cpp blocked_reorder.hpp Makefile
	clang++ $(flags) -c $(no_strict) blocked_reorder.cpp -o blocked_reorder_no_strict.o

blocked_reorder_strict.o: blocked_reorder.cpp blocked_reorder.hpp Makefile
	clang++ $(flags) -c $(strict) blocked_reorder.cpp -o blocked_reorder_strict.o



perf: naive_no_strict.exe naive_strict.exe memcpy_no_strict.exe memcpy_strict.exe Makefile
	perf record -g ./naive_no_strict.exe
	mv perf.data naive_no_strict.perf
	perf record -g ./naive_strict.exe
	mv perf.data naive_strict.perf
	perf record -g ./memcpy_no_strict.exe
	mv perf.data memcpy_no_strict.perf
	perf record -g ./memcpy_strict.exe
	mv perf.data memcpy_strict.perf


naive_no_strict.dat: naive_no_strict.exe Makefile
	./naive_no_strict.exe > naive_no_strict.dat

naive_strict.dat: naive_strict.exe Makefile
	./naive_strict.exe > naive_strict.dat

memcpy_no_strict.dat: memcpy_no_strict.exe Makefile
	./memcpy_no_strict.exe > memcpy_no_strict.dat

memcpy_strict.dat: memcpy_strict.exe Makefile
	./memcpy_strict.exe > memcpy_strict.dat


plot: naive_no_strict.dat naive_strict.dat memcpy_no_strict.dat memcpy_strict.dat Makefile plot_only

plot_only:
	octave -q --no-gui --eval "\
		naive_no_strict = load('naive_no_strict.dat') ;\
		num_blocks = naive_no_strict(:,1) ;\
		naive_no_strict = naive_no_strict(:, 2) ;\
		naive_no_strict = (naive_no_strict ./ num_blocks) .* 1000 ;\
		naive_strict = load('naive_strict.dat') ;\
		naive_strict = naive_strict(:, 2) ;\
		naive_strict = (naive_strict ./ num_blocks) .* 1000 ;\
		memcpy_no_strict = load('memcpy_no_strict.dat') ;\
		memcpy_no_strict = memcpy_no_strict(:, 2) ;\
		memcpy_no_strict = (memcpy_no_strict ./ num_blocks) .* 1000 ;\
		memcpy_strict = load('memcpy_strict.dat') ;\
		memcpy_strict = memcpy_strict(:, 2) ;\
		memcpy_strict = (memcpy_strict ./ num_blocks) .* 1000 ;\
		all = [num_blocks, naive_no_strict, naive_strict, memcpy_no_strict, memcpy_strict] ;\
		save('-ascii', 'per_block.dat', 'all') ;\
	"
	gnuplot -persist -e "\
		set key top left ;\
		set xlabel 'Number of blocks' ;\
		set ylabel 'Time per block [ms]' ;\
		set grid ;\
		set logscale x 2 ;\
		set yrange [0:] ;\
		plot \
			'per_block.dat' u 1:2 w p t 'naive no strict', \
			'' u 1:3 w p t 'naive strict', \
			'' u 1:4 w p t 'memcpy no strict', \
			'' u 1:5 w p t 'memcpy strict' ;\
	"

	# gnuplot -persist -e "set xlabel 'Number of blocks' ; \
    #                      set ylabel 'Time [s]' ; \
    #                      set logscale x 2 ; \
    #                      set title 'Blocked reorder' ; \
    #                      set key top left ; \
    #                      set grid ; \
    #                      plot 'naive_no-strict.dat' w p t 'naive no strict', \
    #                           'memcpy_no-strict.dat' w p t 'memcpy no strict', \
    #                           'naive_strict.dat' w p t 'naive strict', \
    #                           'memcpy_strict.dat' w p t 'memcpy strict'\
    #                     "

compare: naive_no_strict.dat naive_strict.dat memcpy_no_strict.dat memcpy_strict.dat Makefile compare_only

compare_only:
	octave -q --no-gui --eval "\
		naive_no_strict = load('naive_no_strict.dat'); \
		num_blocks = naive_no_strict(:,1); \
		naive_no_strict = naive_no_strict(:, 2); \
		naive_strict = load('naive_strict.dat'); \
		naive_strict = naive_strict(:, 2); \
		memcpy_no_strict = load('memcpy_no_strict.dat'); \
		memcpy_no_strict = memcpy_no_strict(:, 2); \
		memcpy_strict = load('memcpy_strict.dat'); \
		memcpy_strict = memcpy_strict(:, 2); \
		baseline = naive_strict; \
		naive_no_strict = naive_no_strict ./ baseline; \
		naive_strict = naive_strict ./ baseline; \
		memcpy_no_strict = memcpy_no_strict ./ baseline; \
		memcpy_strict = memcpy_strict ./ baseline; \
		all = [num_blocks, naive_no_strict, naive_strict, memcpy_no_strict, memcpy_strict]; \
		save('-ascii', 'compare.dat', 'all'); \
		"
	gnuplot -persist -e "\
		set grid ; \
		set xlabel 'Number of blocks' ; \
		set ylabel 'Relative execution time' ; \
		set logscale x 2 ; \
		plot 'compare.dat' u 1:2 w p t 'naive no strict', \
			 '' u 1:3 w p t 'naive strict', \
			 '' u 1:4 w p t 'memcpy no strict', \
		     '' u 1:5 w p t 'memcpy strict' \
		"

matrix_vector.exe: matrix_vector.o matrix_vector_main.o Makefile
	clang++ $(flags) matrix_vector.o matrix_vector_main.o -o matrix_vector.exe

matrix_vector.o: matrix_vector.hpp matrix_vector.cpp Makefile
	clang++ $(flags) -c matrix_vector.cpp -o matrix_vector.o

matrix_vector_main.o: matrix_vector.hpp matrix_vector_main.cpp Makefile
	clang++ $(flags) -c matrix_vector_main.cpp -o matrix_vector_main.o


clean:
	rm -f *.o *.exe *.dat *.perf
