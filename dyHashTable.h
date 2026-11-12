/**
 * Description: This implements a dynamic chained hash table (malloc/realloc) header-only;
 * used by shell.c to aggregate name counts.
 * Author names: Ebsan Iqbal, Raymond Okolo
 * Author emails: ebsan.iqbal@sjsu.edu, raymond.okolo@sjsu.edu
 * Last modified date: 11/11/2026
 * Creation date: 11/11/2026
 **/
#ifndef DYHASHTABLE_H
#define DYHASHTABLE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INIT_HASHSIZE 10                //The initial size of the table

typedef struct NameCountData {
    char *name;
    int count;
    struct NameCountData *next;         // next entry in hash chain
    struct NameCountData *order_next;   // next entry in first-seen order
} NameCountData;

static NameCountData **hashtab = NULL;  // malloc'd array
static NameCountData *orderHead = NULL; // first name in first-seen order
static NameCountData *orderTail = NULL; // last name in first-seen order
static int hashsize = 0;                // current number of buckets
static int nItems = 0;                   // number of unique names stored

/**
 * Calculates a bucket index for a string.
 * Assumption: s is a valid null-terminated string and the table has been
 * initialized before this function is called.
 * Input parameters: s - string whose hash value is calculated.
 * Returns: the bucket index where s belongs.
 */
static unsigned hash(const char *s) {
    unsigned hashval;
    for (hashval = 0; *s != '\0'; s++) {
        hashval = (unsigned char)*s + 31 * hashval;
    }
    return hashval % hashsize;
}

/**
 * Searches the hash table for a name.
 * Input parameters: s - name to search for.
 * Returns: a pointer to the matching entry, or NULL if the name is not found.
 */
static  NameCountData *lookup(const char *s) {
    for (NameCountData *np = hashtab[hash(s)]; np != NULL; np = np->next) {
        if (strcmp(s, np->name) == 0) {
            return np;
        }
    }
    return NULL;
}

/**
 * Creates an empty hash table using the initial table size.
 * This function allocates the bucket array and sets every bucket to NULL.
 * Returns: nothing. The program exits if memory allocation fails.
 */
static void initializeTable(void) {
    hashsize = INIT_HASHSIZE;
    nItems = 0;
    orderHead = orderTail = NULL;
    hashtab = malloc(hashsize * sizeof(*hashtab));
    if (hashtab == NULL) {
        fprintf(stderr, "allocation failed\n");
        exit(1);
    }
    for (int i = 0; i < hashsize; i++) {
      hashtab[i] = NULL;
    }
}

/**
 * Doubles the number of buckets in the hash table.
 * Existing entries are rehashed and moved into their correct buckets because
 * the bucket index depends on the current value of hashsize.
 * Returns: nothing. The program exits if reallocation fails.
 */
static void growTable(void) {
    int old = hashsize;
    NameCountData **tmp = realloc(hashtab, 2 * old * sizeof(*hashtab));
    if (tmp == NULL) {
        fprintf(stderr, "reallocation failed\n");
        exit(1);
    }
    hashtab = tmp;
    hashsize = 2 * old;
    for (int i = old; i < hashsize; i++) {
        hashtab[i] = NULL;
    }

    // Reinsert entries from the old buckets using the new table size.
    for (int i = 0; i < old; i++) {
        NameCountData **pp = &hashtab[i];
        while (*pp != NULL) {
            NameCountData *np = *pp;
            unsigned h = hash(np->name);
            if (h != (unsigned)i) {     // moves to the new half
                *pp = np->next;
                np->next = hashtab[h];
                hashtab[h] = np;
            } else {
                pp = &np->next;
            }
        }
    }
}

/**
 * Inserts a name or adds to the count of an existing name.
 * New names are added to both the hash chain and the first-seen order list.
 * The table grows automatically when the number of unique names exceeds the
 * number of buckets.
 * Input parameters: name - name to insert; n - amount to add to its count.
 * Returns: a pointer to the inserted or updated table entry.
 */
static NameCountData *insertCount(const char *name, int n) {
    NameCountData *np = lookup(name);
    if (np == NULL) {
        np = malloc(sizeof(*np));
        if (np == NULL) {
            fprintf(stderr, "allocation failed\n");
            exit(1);
        }
        np->name = strdup(name);
        if (np->name == NULL) {
            fprintf(stderr, "strdup failed\n");
            exit(1);
        }
        np->count = n;
        np->order_next = NULL;

        // Add the new entry to its hash bucket.
        unsigned hashval = hash(name);
        np->next = hashtab[hashval];
        hashtab[hashval] = np;

        // Add the new entry to the first-seen order list.
        if (orderTail == NULL) {
            orderHead = np;
        }
        else {
            orderTail->order_next = np;
        }
        orderTail = np;

        nItems++;
        if (nItems > hashsize) {
            growTable();
        }
    } else {
        np->count += n;
    }
    return np;
}

/**
 * Prints every stored name and its count in first-seen order.
 * Returns: nothing.
 */
static void printTable(void) {
    for (NameCountData *np = orderHead; np != NULL; np = np->order_next) {
        printf("%s: %d\n", np->name, np->count);
    }
}

/**
 * Frees all memory used by the table and resets it to an empty state.
 * This releases every stored name, every table entry, and the bucket array.
 * Returns: nothing.
 */
static void freeTable(void) {
    NameCountData *np = orderHead;
    while (np != NULL) {
        NameCountData *next = np->order_next;
        free(np->name);
        free(np);
        np = next;
    }
    free(hashtab);
    hashtab = NULL;
    hashsize = 0;
    nItems = 0;
    orderHead = orderTail = NULL;
}

#endif