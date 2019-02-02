#include <stdio.h>
#include<stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "jsmn.c"
#include "string.h"

int main() {
    printf("JSON Parser START\n");
    FILE *f = fopen("/home/development/Projects/eInkCalendar/json_parser_stm32/my_january.json", "rb");
    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    fseek(f, 0, SEEK_SET);  //same as rewind(f);

    char *js = (char *) malloc(fsize + 1);
    fread(js, fsize, 1, f);
    fclose(f);

    js[fsize] = 0;

    int r = 0;

    jsmn_parser parser;

    jsmn_init(&parser);


    // Check how many tokens are needed - "r" tokens need to be allocated
    r = jsmn_parse(&parser, js, strlen(js), NULL, 0);
    jsmn_init(&parser);

    printf("Allocation:[%i]tokens, using memory k-byes: %lu\n", r, (r * sizeof(jsmntok_t) / 1024));

    // Allocate storage for the tokens
    jsmntok_t *tokens = (jsmntok_t *) malloc(r * sizeof(jsmntok_t));

    // r: amount of tokens , js: file, tokens: tokens returned

    r = jsmn_parse(&parser, js, strlen(js), tokens, (u_int) r);


//    dump(js, tokens, r, 1);
//    exit(1);

    int st_count = 0;


    jsmntok_t *t;

#define LENGTH 50

    struct st_type {
        jsmntok_t s;
        char key[LENGTH];
        int array_count;
    };

    struct st_type st[20];


    // ************ Parse the programm

    memset(&st[0], 0, sizeof(struct st_type));
    strcpy(st[0].key, "Root\0");
    st[0].s.size = 1;

    for (size_t i = 0; i < r; i++) {

        t = &tokens[i];

        // key found in token ************************
        if ((t->type == JSMN_STRING) && (t->size == 1)) {
            memset(st[st_count].key, 0, LENGTH);
            memcpy(st[st_count].key, js + t->start, (size_t) (t->end - t->start));
        }

        // value found in token ************************
        else {

            st[st_count].s.size--;

            // counting array to have it available  on output
            if (st[st_count].s.type == JSMN_ARRAY) {
                st[st_count].array_count++;
            }

            // if token is a printable value - print the value and entire stack
            if (((t->type == JSMN_STRING) && (t->size == 0)) || (t->type == JSMN_PRIMITIVE)) {

                printf("%lu-Stack[%i] Size[%i]:", i, st_count, st[st_count].s.size);

                for (int k = 0; k <= st_count; k++) {

                    if (st[k].s.type == JSMN_ARRAY) {
                        printf("%s[%i]", st[k].key, st[k].array_count);
                    } else {
                        printf("-%s", st[k].key);
                    }
                }
                printf(" = '%.*s'\n", t->end - t->start, js + t->start);

            }

            // if token is a object or array - add to stack
            if ((t->type == JSMN_ARRAY) || (t->type == JSMN_OBJECT)) {
                st_count++;
                memset(&st[st_count], 0, sizeof(struct st_type));
                st[st_count].s = *t;

            }

            // clean the stack, if multiple objects are done
            while (st[st_count].s.size == 0 && st_count > 0) {
                st_count--;
            }

        }

    }

    free(js);
    free(tokens);

    return 1;
}