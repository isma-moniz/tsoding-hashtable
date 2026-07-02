#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define HAYSTACK_INIT_CAPACITY 256

void log_elapsed_time(struct timespec begin, struct timespec end) {
	unsigned long a = begin.tv_sec * 1000 * 1000 * 1000 + begin.tv_nsec;
	unsigned long b = end.tv_sec * 1000 * 1000 * 1000 + end.tv_nsec;
	assert(b > a);
	float nice = (b - a) / 1000.0 / 1000.0 / 1000.0;
	printf("Time elapsed:\n%lu nanoseconds\t%.2f seconds\n", b - a, nice);
}

typedef struct {
	const char* key;
	size_t value;
	uint8_t occupied;
} freq_KV;

typedef struct {
	freq_KV* items;
	size_t count;
	size_t capacity;
} freq_KVs;

struct parsed_file {
	char* buf;
	size_t len;
};

uint32_t hash(uint8_t* buf, size_t size)
{
	uint32_t hash = 0;
	for (size_t i = 0; i < size; ++i) {
		hash  = hash*13 + buf[i]; // tsoding casted this to uint32_t, idk why so i won't (yet)
	}
	return hash;
}

freq_KV *find_key(freq_KVs haystack, const char* needle)
{
	for (size_t i = 0; i < haystack.count; ++i) {
		if (strcmp(haystack.items[i].key, needle) == 0) {
			return &haystack.items[i];
		}
	}
	return NULL;
}

int compare_freqkv_count(const void* a, const void* b)
{
	const freq_KV *akv = a;
	const freq_KV *bkv = b;
	return (int)bkv->value - (int)akv->value; // swapped a and b because we want descending order
}

void reserve_KV(freq_KVs* haystack, size_t expected)
{
	int fresh = 0;
	if (expected > haystack->capacity) {
		if (haystack->capacity == 0) {
			fresh = 1;
			haystack->capacity = HAYSTACK_INIT_CAPACITY;
		}

		while (haystack->capacity < expected) {
			haystack->capacity *= 2;
		}
	}

	if (fresh) {
		haystack->items = (freq_KV*)malloc(haystack->capacity * sizeof(freq_KV));
	} else {
		haystack->items = (freq_KV*)realloc(haystack->items, haystack->capacity * sizeof(freq_KV));
	}
}

void append_KV(freq_KVs *haystack, freq_KV item) 
{
	reserve_KV(haystack, haystack->count + 1);
	haystack->items[haystack->count++] = item;
}

int read_entire_file(const char *path, struct parsed_file* p_file_struct)
{
	FILE* f = fopen(path, "rb");
	if (!f) {
		printf("File read error\n");
		return -1;
	}

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	rewind(f);

	char* buf = malloc(size+1);
	if (!buf) {
		fclose(f);
		printf("Malloc error\n");
		return -1;
	}
	
	size_t n = fread(buf, 1, size, f);
	buf[n] = '\0';

	fclose(f);

	p_file_struct->buf = buf;
	p_file_struct->len = n;
	return 0;
}

int naive_solution(char* buf)
{
	freq_KVs freqs = {0};

	struct timespec begin = {0};
	struct timespec end = {0};
	clock_gettime(CLOCK_MONOTONIC, &begin);

	char* token = strtok(buf, " \n");
	if (!token) return -1;
	size_t count = 1;
	append_KV(&freqs, ((freq_KV) {
				.key = token,
				.value = 1
				}));
	for (; count > 0; ++count) {
		token = strtok(NULL, " \n");
		if (!token) break;

		freq_KV* kv = find_key(freqs, token);
		if (kv) {
			kv->value += 1;
		} else {
			freq_KV new_kv = {
				.key = token,
				.value = 1
			};
			append_KV(&freqs, new_kv);
		}
	}

	clock_gettime(CLOCK_MONOTONIC, &end);

	qsort(freqs.items, freqs.count, sizeof(freqs.items[0]), compare_freqkv_count);
	printf("Top 10 tokens:\n");
	for (size_t i = 0; i < 10 && i < freqs.count; ++i) {
		printf("%s\t%lu\n", freqs.items[i].key, freqs.items[i].value);
	}
	log_elapsed_time(begin, end);
	
	free (freqs.items); //TODO: check leak correctness
	return 0;
}

int main(void)
{
	const char* file_path = "shakespeare.txt";
	struct parsed_file pfile = {0};
	if (read_entire_file(file_path, &pfile) < 0) {
		printf("Error reading file.\n");
		return -1;
	}
	printf("Size of file: %lu\n", pfile.len);

	size_t n = 100;
	size_t m = 0; // nr of occupied slots
	freq_KV *slots = malloc(sizeof(freq_KV) * n);
	memset(slots, 0, sizeof(freq_KV) * n);

	char* token = strtok(pfile.buf, " \n");
	if (!token) return -1;
	uint32_t h = hash((uint8_t*)token, strlen(token))%n;
	printf("  0x%08X\t%s\n", h, token);
	size_t count = 1;
	for (; count < 100 && count > 0; ++count) {
		token = strtok(NULL, " \n");
		if (!token) break;
		uint32_t h = hash((uint8_t*)token, strlen(token))%n;
		printf("  0x%08X\t%s\n", h, token);

		for (size_t i = 0; i < n && slots[h].occupied && strcmp(slots[h].key, token); ++i) { // while slot occupied and not by equal key 
			h = (h + 1) % n; // linear probing
		}

		if (slots[h].occupied) { // found key
			if (strcmp(slots[h].key, token)) {
				printf("Table overflow!\n");
				free(slots);
				free(pfile.buf);
				return -1;
			}
			slots[h].value += 1;
		} else {
			slots[h].value = 1;
			slots[h].occupied = 1;
			slots[h].key = token;
			m += 1;
		}
	}
	
	printf("Slots:\n");
	for (size_t i = 0; i < n; ++i) {
		if (slots[i].occupied) {
			printf("%lu\t%s\n", slots[i].value, slots[i].key);
		} else {
			printf("Free: %lu\n", i);
		}
	}
	// naive_solution(pfile.buf);

	free(pfile.buf);
	free(slots);
	return 0;
}
