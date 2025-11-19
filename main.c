#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

#define MAX_LINE 256

int main() {
    hashTable table;
    initTable(&table);

    FILE *fp = fopen("commands.txt", "r");
    if (!fp) {
        printf("Could not open commands.txt\n");
        return 1;
    }

    char line[MAX_LINE];

    while (fgets(line, MAX_LINE, fp)) {
        char cmd[20];
        char name[50];
        char salaryStr[20];
        char priorityStr[20];

        // Remove newline
        line[strcspn(line, "\n")] = 0;

        // Identify which command it is
        if (strncmp(line, "insert", 6) == 0) {
            sscanf(line, "insert,%49[^,],%19[^,],%19s", name, salaryStr, priorityStr);
            uint32_t salary = atoi(salaryStr);
            uint32_t h = jenkins_hash(name);

            if (insertRecord(&table, name, salary) == 0) {
                printf("Inserted %u,%s,%u\n", h, name, salary);
            } else {
                printf("Insert failed. Entry %u is a duplicate.\n", h);
            }
        }

        else if (strncmp(line, "delete", 6) == 0) {
            sscanf(line, "delete,%49[^,],%19s", name, priorityStr);
            uint32_t h = jenkins_hash(name);

            if (deleteRecord(&table, name) == 0) {
                printf("Deleted record for %u,%s\n", h, name);
            } else {
                printf("Entry %u not deleted. Not in database.\n", h);
            }
        }

        else if (strncmp(line, "update", 6) == 0) {
            sscanf(line, "update,%49[^,],%19s", name, salaryStr);
            uint32_t salary = atoi(salaryStr);
            uint32_t h = jenkins_hash(name);

            if (updateSalary(&table, name, salary) == 0) {
                printf("Updated record %u to salary %u\n", h, salary);
            } else {
                printf("Update failed. Entry %u not found.\n", h);
            }
        }

        else if (strncmp(line, "search", 6) == 0) {
            sscanf(line, "search,%49[^,],%19s", name, priorityStr);

            hashRecord *r = searchRecord(&table, name);
            if (r) {
                printf("Found: %u,%s,%u\n", r->hash, r->name, r->salary);
            } else {
                printf("No Record Found: %s\n", name);
            }
        }

        else if (strncmp(line, "print", 5) == 0) {
            printAll(&table);
        }
    }

    fclose(fp);

    // Final required print
    printAll(&table);

    destroyTable(&table);
    return 0;
}