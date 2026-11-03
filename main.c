#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#define N = 50

typedef struct  {
    int x;
    int y;
} Coord;

typedef struct {
    Coord* graphe;
} Graphe;

void affiche_point(Coord c) {
    printf("Point : (%d, %d)\n", c.x, c.y);
}

double puissance_2(double n) {
    return n*n;
}

double distance_euc(Coord c1, Coord c2) {
    return sqrt(puissance_2(c1.x - c2.x) + puissance_2(c1.y - c2.y));
}

int main(void) {
    printf("ntm\n");
    return 0;
}

//gcc -Wall -Werror -Wvla -fsanitize=address,undefined -o main main.c