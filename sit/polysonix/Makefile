CC = gcc
CFLAGS = -I. -Wall -Wextra -O2
LIBS = -lm

TESTS = test/test_vm_taming test/test_vm_div_zero test/test_vm_jump_bounds test/test_vm_compliance test/test_lfsr test/test_security_wave_idx test/test_vm_prob test/test_vm_select test/test_vm_smooth_select test/test_vm_markov test/test_adsr

.PHONY: all clean run_tests

all: $(TESTS)

test/test_security_wave_idx: test/test_security_wave_idx.c polysonix.h px_patching.h
	$(CC) $(CFLAGS) $< -o $@ $(LIBS)

test/test_vm_div_zero: test/test_vm_div_zero.c px_vm.h
	$(CC) $(CFLAGS) $< -o $@ $(LIBS)

test/test_vm_jump_bounds: test/test_vm_jump_bounds.c px_vm.h
	$(CC) $(CFLAGS) -DPX_VM_IMPLEMENTATION $< -o $@ $(LIBS)

test/test_vm_compliance: test/test_px_vm_impl.c test/test_px_vm_client.c px_vm.h
	$(CC) $(CFLAGS) -c test/test_px_vm_impl.c -o test/test_px_vm_impl.o
	$(CC) $(CFLAGS) -c test/test_px_vm_client.c -o test/test_px_vm_client.o
	$(CC) test/test_px_vm_impl.o test/test_px_vm_client.o -o $@ $(LIBS)

test/test_lfsr: test/test_lfsr.c px_vm.h
	$(CC) $(CFLAGS) -DPX_VM_IMPLEMENTATION $< -o $@ $(LIBS)

test/test_vm_prob: test/test_vm_prob.c px_vm.h
	$(CC) $(CFLAGS) -DPX_VM_IMPLEMENTATION $< -o $@ $(LIBS)

test/test_vm_select: test/test_vm_select.c px_vm.h
	$(CC) $(CFLAGS) -DPX_VM_IMPLEMENTATION $< -o $@ $(LIBS)

test/test_vm_smooth_select: test/test_vm_smooth_select.c px_vm.h
	$(CC) $(CFLAGS) -DPX_VM_IMPLEMENTATION $< -o $@ $(LIBS)

run_tests: $(TESTS)
	@echo "Running test_vm_div_zero..."
	@./test/test_vm_div_zero > /dev/null
	@echo "Running test_vm_jump_bounds..."
	@./test/test_vm_jump_bounds > /dev/null
	@echo "Running test_vm_compliance..."
	@./test/test_vm_compliance > /dev/null
	@echo "Running test_lfsr..."
	@./test/test_lfsr > /dev/null
	@echo "Running test_security_wave_idx..."
	@./test/test_security_wave_idx > /dev/null
	@echo "Running test_vm_prob..."
	@./test/test_vm_prob > /dev/null
	@echo "Running test_vm_select..."
	@./test/test_vm_select > /dev/null
	@echo "Running test_vm_smooth_select..."
	@./test/test_vm_smooth_select > /dev/null
	@echo "Running test_vm_markov..."
	@./test/test_vm_markov > /dev/null
	@echo "Running test_adsr..."
	@./test/test_adsr > /dev/null
	@echo "All tests passed!"

clean:
	rm -f $(TESTS) test/*.o

test/test_vm_taming: test/test_vm_taming.c polysonix.h px_vm.h dsp_math.h
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lm

test/test_vm_markov: test/test_vm_markov.c px_vm.h
	$(CC) $(CFLAGS) -DPX_VM_IMPLEMENTATION test/test_vm_markov.c -o test/test_vm_markov -lm

test/test_adsr: test/test_adsr.c polysonix.h
	$(CC) $(CFLAGS) test/test_adsr.c -o test/test_adsr -lm
