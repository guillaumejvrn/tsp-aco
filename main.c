#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define N 50

typedef struct {
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
    return n * n;
}

double distance_euc(Coord c1, Coord c2) {
    return sqrt(puissance_2(c1.x - c2.x) + puissance_2(c1.y - c2.y));
}

/////////////// PART 4

typedef int* Chemin;

typedef struct {
    double poids;
    Chemin chemin;
} Resultat;

Chemin creer_chemin(int n) {
    Chemin c = malloc((size_t)n * sizeof(int));
    if (c == NULL) {
        return NULL;
    }
    for (int i = 0; i < n; i++) {
        c[i] = i;
    }
    return c;
}

Resultat chemin_aleatoire(Graphe g, int n) {
    Resultat res;
    res.chemin = creer_chemin(n);

    // Algo Fisher-Yates
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = res.chemin[i];
        res.chemin[i] = res.chemin[j];
        res.chemin[j] = temp;
    }

    double total = 0.0;
    for (int i = 0; i < n - 1; i++) {
        total += distance_euc(g.graphe[res.chemin[i]], g.graphe[res.chemin[i + 1]]);
    }
    total += distance_euc(g.graphe[res.chemin[n - 1]], g.graphe[res.chemin[0]]);
    res.poids = total;
    return res;
}

/////////////// PART 4

int main(void) {
    srand((unsigned int)time(NULL));

    Coord points_test1[5] = {
        {0, 0},   // 0 : Bas-gauche
        {0, 10},  // 1 : Haut-gauche
        {10, 10}, // 2 : Haut-droit
        {10, 0},  // 3 : Bas-droit
        {5, 5}    // 4 : Centre
    };

    int n = 5;
    Graphe g = { .graphe = points_test1 };

    Resultat mon_res = chemin_aleatoire(g, n);
    printf("Poids : %f\n", mon_res.poids);
    printf("Chemin : ");
    for (int i = 0; i < n; i++) {
        printf("%d ", mon_res.chemin[i]);
    }
    printf("\n");

    free(mon_res.chemin);
    return 0;
}