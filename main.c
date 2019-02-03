#include <stdio.h>
#include<stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "jsmn.c"
#include "string.h"
#include "time.h"

struct cal_entry_type {
    struct tm *tm;
    char *summary;
};


int cmp_dates_descend(const void *d1, const void *d2)
{

    struct cal_entry_type date_1 = *(const struct cal_entry_type *)d1;
    struct cal_entry_type date_2 = *(const struct cal_entry_type *)d2;

    double d = difftime(mktime(date_1.tm), mktime(date_2.tm));

    return (d > 0) - (d < 0);

}

int dayofweek(int d, int m, int y)
{
    static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    y -= m < 3;
    return ( y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

void GetDate(char *value,bool with_time, struct tm *my_time) {

    // no time format:   2019-01-07
    // with time format: Format: 2019-01-21T09:00:00+01:00

     memset(my_time,0,sizeof(struct tm));

    int year;
    long gmtoff;

    if (with_time) {
        if (sscanf(value, "%4d-%2d-%2dT%2d:%2d:%2d+%ld", &year,
                &my_time->tm_mon,
                &my_time->tm_mday,
                &my_time->tm_hour,
                &my_time->tm_min,
                &my_time->tm_sec,
                &gmtoff
                ) == EOF) exit(-1);
        my_time->tm_gmtoff=gmtoff*60;
    }

    else {
        if (sscanf(value, "%4d-%2d-%2d", &year, &my_time->tm_mon, &my_time->tm_mday) == EOF) exit(-1);
    }

    my_time->tm_year = year - 1900;
    my_time->tm_mon--;
    my_time->tm_wday = dayofweek(my_time->tm_mday, my_time->tm_mon + 1, my_time->tm_year + 1990);
}

int  search_json(char *js, jsmntok_t *tokens, int end_line, char *search_text, char *value) {

    #define MAX_LENGTH_OF_KEY 50
    #define MAX_STACK_SIZE 8
    #define PRINT_JSON_TREE 0


    #define DBT(fmt, ...)  do { if (PRINT_JSON_TREE) fprintf(stdout, fmt, __VA_ARGS__); } while (0)

    int st_count = 0;


    jsmntok_t *t;

    struct st_type {
        jsmntok_t s;
        char key[MAX_LENGTH_OF_KEY];
        int array_count;
    };

    struct st_type st[MAX_STACK_SIZE];

    memset(&st[0], 0, sizeof(struct st_type));
    strcpy(st[0].key, "Root\0");
    st[0].s.size = tokens->size;

    char token_text[100];


    for (int i = 0; i < end_line; i++) {

        t = &tokens[i];

        // key found in token ************************
        if ((t->type == JSMN_STRING) && (t->size == 1)) {
            memset(st[st_count].key, 0, MAX_LENGTH_OF_KEY);
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

                DBT("%i-Stack[%i] Size[%i]:", i, st_count, st[st_count].s.size);


                // build the token-string, to be compared with the search-string
                int len = 0;

                for (int k = 0; k <= st_count; k++) {
                    if (st[k].s.type == JSMN_ARRAY) {
                        len += sprintf(token_text+len, "%s[%i]", st[k].key, st[k].array_count);
                    } else {
                        len += sprintf(token_text+len, "-%s", st[k].key);
                    }
                }

                DBT("%s",token_text);
                DBT(" = '%.*s'\n", t->end - t->start, js + t->start);

                if (strcmp(search_text,token_text)==0) {
                    sprintf(value,"%.*s", t->end - t->start, js + t->start);
                    return i;
                }

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

    return 0;
}


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


    char value[100];
    char searchstr[50];
    char cal_entry[200];
    int cal_entries_cnt=0;
    int items=1;
    struct cal_entry_type cal_entries[100];


    while(1) {

        sprintf(searchstr,"-Root-items[%i]-kind",items);

        if (search_json(js, tokens, r , searchstr,value)==0) break;

        if (strcmp(value,"calendar#event")==0) {

            memset(cal_entry,32,sizeof(cal_entry));

            cal_entries[cal_entries_cnt].tm=malloc(sizeof(struct tm));

            // Find Start-Date
            sprintf(searchstr,"-Root-items[%i]-start-date",items);
            if (search_json(js, tokens,  r , searchstr,value)!=0) {
                // Format 2019-01-07

                GetDate(value,false,cal_entries[cal_entries_cnt].tm);

                strftime(cal_entry,80,"%A", cal_entries[cal_entries_cnt].tm);
                cal_entry[strlen(cal_entry)]=' ';

                strftime(cal_entry+10,80,"%d.%m.%Y", cal_entries[cal_entries_cnt].tm);
                cal_entry[strlen(cal_entry)]=' ';

            } else {
                // Format: 2018-10-08T09:07:30.000Z
                sprintf(searchstr,"-Root-items[%i]-start-dateTime",items);
                if (search_json(js, tokens, r , searchstr,value)!=0) {
                    GetDate(value,true,cal_entries[cal_entries_cnt].tm);

                    strftime(cal_entry,80,"%A", cal_entries[cal_entries_cnt].tm);
                    cal_entry[strlen(cal_entry)]=' ';

                    strftime(cal_entry+10,80,"%d.%m.%Y   %H:%M", cal_entries[cal_entries_cnt].tm);
                    cal_entry[strlen(cal_entry)]=' ';

                } else return -1;

            }

            // Find Summary
            sprintf(searchstr,"-Root-items[%i]-summary",items);
            if (search_json(js, tokens,  r , searchstr,value)!=0) {
                sprintf(cal_entry+28, ": %s", value);
            } else return -1;

            // Copy into array of events
            cal_entries[cal_entries_cnt].summary=strdup(cal_entry);

            cal_entries_cnt++;
            items++;
        }

    }


    // Sort
    qsort(cal_entries, (size_t ) cal_entries_cnt, sizeof *cal_entries, cmp_dates_descend);

    // Print the entries
    for (int i=0;i<cal_entries_cnt;i++){
        printf("ENTRY: %s\n",cal_entries[i].summary);
    }

    // Free memory
    for (int i=0;i<cal_entries_cnt;i++){
        free(cal_entries[i].summary);
        free(cal_entries[i].tm);
    }

    free(js);
    free(tokens);

    return 1;
}