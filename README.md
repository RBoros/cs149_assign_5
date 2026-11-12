# CS 149 - Assignment 5

## Students

- Ebsan Iqbal
- Raymond Okolo

## Description

This assignment extends our Assignment 4 parallel name-counting program. In Assignment 4 the parent `shell` kept the combined totals in a fixed-size summation region of the shared-memory object. In this assignment that fixed-size aggregation array is replaced by a **dynamically allocated hash table** that the parent builds with `malloc()`, `realloc()` and `strdup()` and frees with `free()` after every command line. The parent can now count any number of distinct names.

The `shell` program is an interactive shell. It creates one child process for each input file using `fork()`, and each child runs `countnames` using `execl()`. (If no filename is given, one child reads from stdin.) For every command line, the parent creates a new shared-memory object with `shm_open()` and `ftruncate()` and maps it with `mmap()`. The mapping is divided into one region for each child. Each region holds up to `MAXNAMES + 1` (1001) `childEntry` records (a name of up to 255 characters plus an `int` count); the extra slot guarantees room for the terminator.

As in Assignment 4, an `exec` destroys the memory mappings a child inherited from `fork`, so the shell gives each child the **name** of the shared-memory object and the **index** of the region it owns. Each `countnames` child calls `shm_open()` and `mmap()` itself, counts the names in its assigned input file, and writes an array of `childEntry` records into its own region only. A record whose name has length 0 marks the end of valid data in a region. The children still use fixed-size character arrays in shared memory on purpose: a `char *` written by a child would point into the child's own address space and would be meaningless to the parent.

After all children have been created, the parent waits for every child to finish. It then calls `initializeTable()`, reads each child region up to its terminator, adds every record to the dynamic hash table with `insertCount()`, prints the table with `printTable()`, and releases it with `freeTable()`. The shared-memory object is unmapped, closed and unlinked at the end of every command line, so a later command never sees stale data.

For compatibility with the previous assignments, each child also creates a `<PID>.out` file containing its own counts and a `<PID>.err` file containing empty-line warnings. The shell prints each child's exit status to stderr. `countnames` can still be run directly without the shell (`./countnames names.txt` or with names on stdin); in that case it uses no shared memory and only writes `<PID>.out` and `<PID>.err`.

### How `countnames` is called by the shell

```text
./countnames <PID> <filename> <shared_memory_name> <region_index>
```

A filename of `1` tells `countnames` to read from stdin.

### The dynamic hash table (`dyHashTable.h`)

Each unique name is stored in one heap-allocated node:

```c
typedef struct NameCountData {
    char *name;                         // strdup'd copy of the name
    int count;                          // total occurrences
    struct NameCountData *next;         // next entry in the same hash chain
    struct NameCountData *order_next;   // next entry in first-seen order
} NameCountData;
```

- **Buckets:** `hashtab` is a `NameCountData **` allocated with `malloc()` to an initial size of 10 (`INIT_HASHSIZE`). Each slot is the head of a linked list (chaining), so names with the same hash value share a chain.
- **Hash function:** `hashval = c + 31 * hashval` over the characters of the name, modulo the current table size (the prime-31 radix hash suggested in the assignment notes).
- **Insert:** `insertCount(name, n)` looks the name up in its chain. If it is found, `n` is added to its count. If not, a node is created with `malloc()`, the name is copied with `strdup()`, and the node is linked at the head of its chain.
- **Growing with `realloc()`:** when the number of stored names becomes larger than the number of buckets, `growTable()` doubles the bucket array with `realloc()`, clears the new half, and rehashes every chain. Because the size doubles, a node in old slot `i` either stays in slot `i` or moves to slot `i + old size`.
- **Print order:** the nodes are also linked through `order_next` in the order they were first seen, so the totals are printed in the same order as in Assignment 4 (the first file's region first). `order_next` is one extra pointer beyond the three fields in the assignment's struct.
- **Free:** `freeTable()` walks the first-seen list once and frees every name string and every node, then frees the bucket array and resets the globals so the next command line starts from an empty table.

### Assumptions and limits

- The parent has **no limit** on the number of distinct names or on their length (`malloc`/`realloc`/`strdup`). The Assignment 1 limits (100 names of 30 characters) no longer apply to the parent.
- The children still have fixed limits because their shared-memory regions are fixed-size arrays (as the assignment says, the children keep the Assignment 4 mmap arrays). Each child region holds `MAXNAMES` (1000) unique names, and each name field is `NAME_LEN` (256) bytes, so a stored name has at most 255 characters. `countnames` reads each line with `fgets()` into a 256-byte buffer, so we support names of up to 253 characters (which also works for files with Windows `\r\n` line endings). Longer lines would be split into several names; we did not test that. We do not check for overflow beyond 1000 unique names in one input file.
- `NAME_LEN` must have the same value in `shell.c` and `countnames.c`, because it sets the layout of the shared memory. Both use 256.
- The shared-memory object is sized from the number of input files on the command line. The assignment says to assume up to three input files, and our tests use at most three.
- A trailing newline or carriage return is removed from each line. Everything else, including spaces, is part of the name, so `Tom Wu` and `Tom Wu ` (with a trailing space) are different names.
- Empty lines are not counted; they produce a warning in `<PID>.err`.

### Files

- `shell.c` - parent shell; creates and manages shared memory, forks children, waits, aggregates the children's results into the dynamic hash table, prints them, and frees everything
- `countnames.c` - counts names for one input and writes the results into its assigned shared-memory region; frees its own table before it exits
- `dyHashTable.h` - **new:** header-only dynamic chained hash table (`malloc`/`realloc`/`strdup`/`free`) used by `shell.c`
- `hashtable.h` - header-only hash table from Assignment 4, still used by `countnames.c` to count the names of one file
- `test/` - provided and student-created test files

Both headers are needed to compile: `shell.c` includes `dyHashTable.h`, and `countnames.c` includes `hashtable.h`. Neither program includes the other's header, so the names defined in the two headers never collide. The record stored in shared memory is called `childEntry` in both `.c` files, because the name `NameCountData` now belongs to the parent's hash-table node.

---

## Compilation

Compile from the project directory:

```bash
gcc -o shell shell.c -Wall -Werror
gcc -o countnames countnames.c -Wall -Werror
```

Both commands compile without errors or warnings (`output.pdf`, section 1).

To check for memory problems with valgrind, compile with debug information and run the shell under valgrind (see "Valgrind" below):

```bash
gcc -g -o shell shell.c
gcc -g -o countnames countnames.c
valgrind --leak-check=full ./shell
```

---

## Running the Program

Start the shell:

```bash
./shell
```

At the `%` prompt, enter commands such as:

```text
% ./countnames ./test/names1.txt ./test/names2.txt
```

The exact child PIDs and the order of the child-termination messages can change between runs because the child processes run independently. Combined totals are printed in the order names are first seen, reading the children's regions in order (the first file's region first).

Type:

```text
exit
```

to leave the shell.

---

# Tests

All tests are typed at the `%` prompt of `./shell`. The screenshots for every test are in `output.pdf`; the section numbers below refer to it.

## Tests carried over from Assignment 4

These tests produce the same results as in Assignment 4. They confirm that replacing the summation region with the dynamic hash table did not change the output.

### Test 1 - `./test/names1.txt` (section 2)

```text
% ./countnames ./test/names1.txt
```

Expected combined output:

```text
Tom Wu: 3
```

The child terminates normally with exit code `0`. Expected `<PID>.out`: `Tom Wu: 3`. Expected `<PID>.err`:

```text
Warning - file ./test/names1.txt line 3 is empty.
```

Purpose: Tests repeated names and empty-line warning handling.

### Test 2 - `./test/names2.txt` (section 2)

```text
% ./countnames ./test/names2.txt
```

Expected combined output:

```text
Jenn Xu: 2
Tom Wu: 1
```

Expected `<PID>.err`: empty.

Purpose: Tests multiple different names and repeated-name counting.

### Test 3 - `./test/names.txt` (section 3)

```text
% ./countnames ./test/names.txt
```

Expected combined output:

```text
Nicky: 1
Dave Joe: 2
Yuan Cheng Chang: 3
John Smith: 1
```

Expected `<PID>.err`:

```text
Warning - file ./test/names.txt line 2 is empty.
Warning - file ./test/names.txt line 5 is empty.
```

Purpose: Tests names containing spaces, duplicate names, and multiple empty lines.

### Test 4 - `./test/namesB.txt` (section 3)

```text
% ./countnames ./test/namesB.txt
```

Expected combined output and `<PID>.out`: the same four lines as Test 3. Expected `<PID>.err`: warnings for lines 2 and 5 of `./test/namesB.txt`.

Purpose: Provides another test with duplicate names, names containing spaces, and empty lines.

### Tests 5-9 - the large provided files (section 4)

```text
% ./countnames ./test/names_long.txt
% ./countnames ./test/names_long_redundant.txt
% ./countnames ./test/names_long_redundant1.txt
% ./countnames ./test/names_long_redundant2.txt
% ./countnames ./test/names_long_redundant3.txt
```

| Test | File | Lines in `<PID>.out` | Expected `<PID>.err` | What to look for |
|---|---|---|---|---|
| 5 | `names_long.txt` | 101 | empty | starts with `MARY SMITH: 1` and ` : 99`, ends with `ROBIN HAYES: 1` |
| 6 | `names_long_redundant.txt` | 91 | `line 97 is empty` | `Avery Taylor: 2`, `Andrew Lee: 2`, `Nathan Davis: 2` |
| 7 | `names_long_redundant1.txt` | 40 | lines 2, 4, 6, 8 are empty | empty lines at different positions are skipped |
| 8 | `names_long_redundant2.txt` | 44 | empty | a larger normal input, every name counted once |
| 9 | `names_long_redundant3.txt` | 41 | empty | `MARY SMITH: 2`, `PATRICIA JOHNSON: 2`, `LINDA WILLIAMS: 2`, `BARBARA JONES: 2`, `ELIZABETH BROWN: 2` |

In every case the child exits with code `0`.

Purpose: Tests large inputs, many unique names, repeated names that are summed, and empty lines at different positions.

**Notes on the long-output screenshots in `output.pdf`.** These outputs are longer than one screenshot, so `output.pdf` shows a top part and a bottom part of the terminal output and of `<PID>.out`. Each `<PID>.err` file is shown in full. Each page of `output.pdf` says which lines are shown.

| Test | Terminal screenshots show | `<PID>.out` screenshots show | Not shown in any screenshot |
|---|---|---|---|
| `names_long.txt` | totals 1-38 and 60-100 | lines 1-25 and 81-101 | totals 39-59 |
| `names_long_redundant.txt` | totals 1-40 and 50-91 | lines 1-40 and 54-91 | totals 41-49 |
| `names_long_redundant1.txt` | all 40 | lines 1-39 | nothing |
| `names_long_redundant2.txt` | all 44 | lines 1-40 | nothing |
| `names_long_redundant3.txt` | all 41 | lines 1-40 | nothing |

### Test - `./test/mytestA2.txt` twice (section 5)

Our own test file from Assignment 2 (it was called `mytestA4.txt` in the Assignment 4 README). Contents:

```text
Alice
Bob
Alice
Charlie
Bob
Alice
```

Run the same file twice:

```text
% ./countnames ./test/mytestA2.txt ./test/mytestA2.txt
```

Expected combined output:

```text
Alice: 6
Bob: 4
Charlie: 2
```

Two children terminate normally. Each child's own `<PID>.out` contains `Alice: 3`, `Bob: 2`, `Charlie: 1`.

Purpose: Tests that two children write to different shared-memory regions and that the parent adds both regions into one hash table.

### Test - two different files (section 6)

```text
% ./countnames ./test/names1.txt ./test/names2.txt
```

Expected combined output:

```text
Tom Wu: 4
Jenn Xu: 2
```

Purpose: Verifies aggregation when the same name appears in two different child regions.

### Test - three files (section 7)

```text
% ./countnames ./test/mytestA2.txt ./test/names2.txt ./test/names1.txt
```

Expected combined output:

```text
Alice: 3
Bob: 2
Charlie: 1
Jenn Xu: 2
Tom Wu: 4
```

Purpose: Tests the assignment maximum of three input files and verifies that all three regions are combined.

### Test - invalid filename while another child succeeds (section 8)

```text
% ./countnames ./test/does_not_exist ./test/names2.txt
```

Expected error from the failed child:

```text
error: cannot open file ./test/does_not_exist
```

That child exits with status `1` before it creates any output file. The other child exits with status `0`, and the combined output is:

```text
Jenn Xu: 2
Tom Wu: 1
```

Purpose: Verifies that one failed child does not stop another child from contributing its results. The failed child never writes to its region, so the parent sees an empty region (first byte of the name is 0) and skips it.

### Test - the table is reset between commands (section 2)

In the same shell session we ran `./countnames ./test/names2.txt` and then `./countnames ./test/names1.txt`. The second command prints only `Tom Wu: 3`, not `Tom Wu: 4`.

Purpose: Verifies that the hash table is freed at the end of each command line, so the second command starts from an empty table and the totals of the first command do not carry over.

### Test - shared memory cleanup (section 9)

```text
% exit
$ ls /dev/shm
```

Expected output: nothing is listed, so no `shared_memory_*` object was left behind.

---

## New tests for Assignment 5

These tests exercise the dynamic hash table. All are in `test/` and were created by us.

### Test 10 - `./test/mytestA5_100.txt` (section 10)

100 different names (first `James Carter`, last `Alice Rosales`).

```text
% ./countnames ./test/mytestA5_100.txt
```

Expected: 100 lines, each name with count `1`, in the order of the file; `<PID>.err` is empty.

Purpose: Tests that the table grows with `realloc()`. The table starts with 10 buckets and is doubled four times (10, 20, 40, 80, 160) while the names are added, and no name is lost.

### Test 11 - `./test/mytestA5_300.txt` (section 11)

300 different names (first `James Anderson`, last `Selena Horton`).

```text
% ./countnames ./test/mytestA5_300.txt
```

Expected: 300 lines, each name with count `1`, in the order of the file; `<PID>.err` is empty.

Purpose: Tests "a few hundred" names. The table is doubled five times (10, 20, 40, 80, 160, 320), far beyond the fixed sizes used in Assignment 4.

### Test 12 - two files with table growth (section 12)

`mytestA5_100.txt` (100 names) and `mytestA5_150.txt` (150 different names, first `James Anderson`, last `Juniper Logan`).

```text
% ./countnames ./test/mytestA5_100.txt ./test/mytestA5_150.txt
```

Expected: the parent prints 250 lines, each with count `1`: the 100 names of the first file followed by the 150 names of the second file. The two files share no names. `735.out` has 100 lines and `736.out` has 150 lines (PIDs vary); both `.err` files are empty.

Purpose: Tests aggregation of two child regions into one table that grows while the parent inserts (to 320 buckets), and checks that the first-seen order follows the order of the regions. The files have no names in common, so this test checks growth and ordering; Tests "two different files", "three files" and `mytestA2.txt` twice check that counts from different children are added together.

### Test 13 - hash collisions, `./test/mytestA5_collide.txt` (section 13)

Contents:

```text
Aa
BB
Aa
BB
BB
C
```

```text
% ./countnames ./test/mytestA5_collide.txt
```

Expected combined output:

```text
Aa: 2
BB: 3
C: 1
```

Purpose: `Aa` and `BB` have the same hash value (2112) with our hash function, so they always share one chain, at every table size. The test checks that chaining keeps the two names apart and that rehashing during `realloc()` growth keeps them together in the right chain.

### Test 14 - empty file, `./test/mytestA5_empty.txt` (section 14)

```text
% ./countnames ./test/mytestA5_empty.txt
```

Expected: no totals are printed, the child exits with code `0`, and its `.out` and `.err` files are empty.

Purpose: Tests the smallest input. The table is created with its 10 empty buckets, nothing is inserted, and `freeTable()` still releases the bucket array.

### Test 15 - long names, `./test/mytestA5_long_names.txt` (section 15)

Seven names of 27 to 33 characters (several are longer than the old 31-character limit of Assignment 4):

```text
Christopher Montgomery-Smythe-Lee
Maximilian Featherstone-Hawthorne
Alexandriana Montgomery-Smythe
Bartholomew Fitzpatrick-Lancaster
Christopher Wellington-Richards
Konstantinopoulos Blackwell
Anastasius Vandermeer-Hamilton
```

```text
% ./countnames ./test/mytestA5_long_names.txt
```

Expected combined output: the same seven names, each followed by `: 1`, in the order of the file. `<PID>.err` is empty.

Purpose: Tests names longer than the old limit. With the Assignment 4 value of `NAME_LEN` (32) and a 32-byte read buffer, long names were split into pieces and counted as separate names. We changed `NAME_LEN` to 256 in both `shell.c` and `countnames.c` and read each line into a `NAME_LEN`-byte buffer, and the names are now stored whole.

---

## Valgrind

We used valgrind to check for memory leaks and memory errors. The programs were compiled with `gcc -g`, and the shell was run with:

```bash
valgrind --leak-check=full ./shell
```

Each run types one command at the `%` prompt and then `exit`. The screenshots are in `output.pdf`, section 16.

| Run | Heap in use at exit | Total heap usage | Result |
|---|---|---|---|
| `./countnames ./test/mytestA5_300.txt` | 0 bytes in 0 blocks | 608 allocs, 608 frees, 20,902 bytes | All heap blocks were freed; 0 errors |
| `./countnames ./test/mytestA5_100.txt ./test/mytestA5_150.txt` | 0 bytes in 0 blocks | 508 allocs, 508 frees, 18,588 bytes | All heap blocks were freed; 0 errors |
| `./countnames ./test/does_not_exist.txt ./test/names2.txt` | 0 bytes in 0 blocks | 7 allocs, 7 frees, 2,207 bytes | All heap blocks were freed; 0 errors |
| `./countnames ./test/mytestA5_empty.txt` | 0 bytes in 0 blocks | 3 allocs, 3 frees, 2,128 bytes | All heap blocks were freed; 0 errors |

These runs check the `shell` process, which owns the dynamic hash table, so they show that every node, every `strdup`'d name and the bucket array are freed after each command. In `countnames.c` we also free the child's own table (`clearTable()`) before every normal exit.

Purpose of each run: the 300-name run has the most table growth (five `realloc()` calls); the two-file run merges two child regions; the missing-file run covers the error path with an empty region; the empty-file run covers the case where nothing is inserted.

---

# Lessons learned

In this assignment, we learned how to replace a fixed-size array with a data structure that grows as the input grows. We built a chained hash table whose bucket array starts small and is enlarged with `realloc()`. We learned that after the array is enlarged, every name has to be rehashed, because its slot depends on the table size. With a doubling table, a node either stays in its slot or moves to the slot `old size` further on.

We learned to match every allocation with a `free()`. A string copied with `strdup()` is a separate heap allocation that must be freed too, so freeing a node is not enough. We used valgrind to confirm that nothing was left behind, and we learned to read its heap summary (allocations versus frees, and "in use at exit"). We also found that `countnames` had a leak on its successful path, which we fixed by clearing its table before every exit.

We learned why the children still use fixed-size arrays in shared memory while the parent uses pointers: a pointer written by one process is only valid inside that process's address space, so data shared between processes has to be stored by value.

Our long-name test taught us that a fixed buffer limits more than we expected. A line longer than the `fgets()` buffer is returned in pieces, and each piece was counted as its own name, so the output was wrong rather than just truncated. We also learned that test files from Windows contain `\r\n` line endings and trailing spaces, which change the names that are compared. We now strip `\r` and `\n`, and we cleaned the trailing spaces out of our own test files.

Finally, we learned to keep names in headers from colliding (the parent's `NameCountData` and the children's record), and to keep the static functions in a header used so that `-Wall -Werror` stays quiet. This assignment also reinforced `fork()`, `exec()`, `wait()`, shared memory with `mmap()`, and file handling from the earlier assignments.

# References

- SJSU CS 149 Assignment 5 instructions, including the notes on `getline`/`fgets`/`strdup` and on hashing, and the provided hash table template code.
- SJSU CS 149 course slides/examples covering processes, `fork()`, `exec()`, `wait()`, shared memory, `mmap()`, and dynamic memory allocation.
- zyBooks / zyLabs CS 149 environment, provided test files, and the valgrind installation in zyLabs.
- Claude (Anthropic) - used for assistance with this assignment. 
# Acknowledgements

We would like to acknowledge the SJSU CS 149 course materials, lectures, slides, zyBooks exercises, and provided examples for explaining the process, shared-memory and dynamic-memory concepts used in this assignment.

We also used Claude (Anthropic) for assistance with this assignment: planning and creating the test cases and test files, debugging (for example the name-length problem found by the long-names test, and compiler and analyzer warnings), designing the dynamic hash table from the assignment description and the course template, and preparing the README and `output.pdf`.
