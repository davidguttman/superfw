
COV_FLAGS=-ftest-coverage -fprofile-arcs
MEMCHK_FLAGS=-fsanitize=address -lasan
CFLAGS = -O0 -ggdb -I../src/ -Wall $(COV_FLAGS)

all:
	$(CC) $(CFLAGS) $(MEMCHK_FLAGS) -o cimpl_test.bin cimpl_test.c ../src/cimpl.c -DBUILTIN_PREFIX=superfw_
	./cimpl_test.bin
	lcov -c -d . -o cimpl_test.info
	$(CC) $(CFLAGS) $(MEMCHK_FLAGS) -o util_test.bin util_test.c ../src/util.c ../src/nanoprintf.c ../src/fileutil.c -I../
	./util_test.bin
	lcov -c -d . -o util_test.info
	$(CC) $(CFLAGS) $(MEMCHK_FLAGS) -o utf_util_test.bin utf_util_test.c ../src/utf_util.c -I../
	./utf_util_test.bin
	lcov -c -d . -o utf_util_test.info
	$(CC) $(CFLAGS) $(MEMCHK_FLAGS) -o sha256_test.bin sha256_test.c ../src/sha256.c -lcrypto
	./sha256_test.bin
	lcov -c -d . -o sha256_test.info
	$(CC) $(CFLAGS) -o cheats_test.bin cheats_test.c ../src/cheats.c ../src/util.c ../src/nanoprintf.c -I../
	./cheats_test.bin
	lcov -c -d . -o cheats_test.info
	$(CC) $(CFLAGS) $(MEMCHK_FLAGS) -o crc_test.bin crc_test.c ../src/crc.c
	./crc_test.bin
	lcov -c -d . -o crc_test.info

	lcov -a cimpl_test.info -a util_test.info -a utf_util_test.info -a crc_test.info -a sha256_test.info -a cheats_test.info -o total.info
	rm -rf coverage/
	genhtml -o coverage/ total.info

clean:
	rm -f *.bin *.gcda *.gcno *.info
	rm -rf coverage/

cli_tests:
	$(CC) -flto -O0 -ggdb -I../src/ -Wall $(MEMCHK_FLAGS) -o cli_patchengine.bin cli_patchengine.c ../src/patchengine.c  ../src/util.c  -I../  -ffunction-sections -fdata-sections  -Wl,--gc-sections

# Focused production-parser gate, no lcov dependency or changes to upstream all.
.PHONY: kid_tests
kid_tests:
	$(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -I../src -I.. -o kid_library_test.bin kid_library_test.c ../src/kid_library.c
	./kid_library_test.bin
	uv run ../tools/kid-art.py --self-test ./kid_library_test.bin

	$(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -I../src -I.. -o kid_mode_test.bin kid_mode_test.c ../src/kid_mode.c
	./kid_mode_test.bin

	# Actual menu launch path; drop unrelated UI sections before linking mocks.
	# ASan global roots would retain every popup, and upstream nested callbacks
	# require the real executable stack (not ASan's non-executable fake stack).
	$(CC) -std=gnu11 -O1 -g -fno-builtin -ffunction-sections -fdata-sections -fsanitize=address,undefined --param asan-globals=0 -fno-pie -no-pie -I../src -I.. -DFW_MAX_SIZE_KB=512 -DVERSION_WORD=21 -DVERSION_SLUG_WORD=0 -DFW_FLAVOUR='"SD"' -Wl,--gc-sections -o kid_launch_test.bin kid_launch_test.c ../src/kid_mode.c
	ASAN_OPTIONS=detect_stack_use_after_return=0 ./kid_launch_test.bin

# The production SDRAM loader, including its fast and slow hardware-copy paths.
.PHONY: loader_tests
loader_tests:
	# Upstream fix_gba_header shifts a negative checksum; outside this I/O gate.
	$(CC) -std=gnu11 -O1 -g -fno-builtin -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-sanitize=shift --param asan-globals=0 -fno-pie -no-pie -I../src -I.. -DFW_MAX_SIZE_KB=512 -DVERSION_WORD=21 -DVERSION_SLUG_WORD=0 -DFW_FLAVOUR='"SD"' -Wl,--gc-sections -o loader_read_test.bin loader_read_test.c
	./loader_read_test.bin
