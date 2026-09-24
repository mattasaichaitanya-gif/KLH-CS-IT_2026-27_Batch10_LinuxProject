#ifndef BUFFER_H
#define BUFFER_H

#define BUFFER_SIZE 5

extern int buffer[BUFFER_SIZE];
extern int in;
extern int out;

void insert_item(int item);
int remove_item(void);

#endif