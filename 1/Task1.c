#include <stdio.h>
#define SIZE 1024
double a[SIZE][SIZE];
int i;
double  b[SIZE][SIZE];
void main(int argc, char **argv)
{
    int j;
    double c[SIZE];
    for(i=0; i < SIZE; i++) {
        c[i] = 0;
        for(j = 0; j < SIZE; j++)
            c[i] += (b[i][j] = 20.19);
    }
    for(j = 0; j < SIZE; j++)
        for(i = 0; i < SIZE; i++)
            a[i][j] = c[i] + c[j];
}
