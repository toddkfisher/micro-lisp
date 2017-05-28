#include <stdlib.h>
#include <stdio.h>

#define HASH_SIZE 1999
#define STR_SIZE (5*(1 << 20))

char str_space[STR_SIZE];
int str_free = 0;
char *str_hash[HASH_SIZE];

char *str_hash_new(unsigned long h, char *s)
{
    strcpy(&str_space[str_free], s);
    str_hash[h2] = &str_space[str_free];
    str_free += strlen(s) + 1;
    return str_hash[h2];

int intern(char *s)
{
    char *sh;
    unsigned long h = hash(s), h2;
    if ((sh = str_hash[h])) {
        if (!strcmp(s, sh)) {
            return sh;
        } else {
            h2 = h;
            do {
                h2 = (h2 + 1) % HASH_SIZE;
                if ((sh = str_hash[h2])) {
                    if (!strcmp(s, sh)) {
                        return sh;
                    }
                    // else continue rehash
                } else {
                    return str_hash_new(h2, s);
                }
            } while (h2 != h);
            fprintf(stderr, "intern() : hash table overflow.\n");
            exit(1);
        }
    } else {
        return str_hash_new(h2, s);
    }
}

unsigned long hash(char *name)
{
    unsigned long h = 0, g;
    while (*name) {
        h = (h << 4) + *name++;
        if (g = h & 0xf0000000) {
            h ^= g >> 24;
        }
        h &= ~g;
    }
    return h % HASH_SIZE;
}

int main(int argc, char **argv)
{
    char word[128];

}
