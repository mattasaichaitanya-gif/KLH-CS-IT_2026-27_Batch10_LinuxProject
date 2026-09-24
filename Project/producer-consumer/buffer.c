#include <stdio.h>
#include "buffer.h"

int buffer[BUFFER_SIZE];

int in = 0;
int out = 0;

void insert_item(int item)
{
    buffer[in] = item;

    printf("Inserted %d at position %d\n", item, in);

    in = (in + 1) % BUFFER_SIZE;
}

int remove_item(void)
{
    int item = buffer[out];

    printf("Removed %d from position %d\n", item, out);

    out = (out + 1) % BUFFER_SIZE;

    return item;
}