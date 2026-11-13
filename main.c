#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <float.h>

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

double calculer_poids(Graphe g, Chemin c, int n) {
    double total = 0.0;
    for (int i = 0; i < n - 1; i++) {
        total += distance_euc(g.graphe[c[i]], g.graphe[c[i + 1]]);
    }
    total += distance_euc(g.graphe[c[n - 1]], g.graphe[c[0]]);
    return total;
}

void explorer_permutations(Graphe g, int n, Chemin actuel, int debut, Resultat* meilleur) {
    if (debut == n) {
        double p = calculer_poids(g, actuel, n);
        if (p < meilleur->poids) {
            meilleur->poids = p;
            for (int i = 0; i < n; i++) {
                meilleur->chemin[i] = actuel[i];
            }
        }
        return;
    }

    for (int i = debut; i < n; i++) {
        // 1. Échange
        int temp = actuel[debut];
        actuel[debut] = actuel[i];
        actuel[i] = temp;

        explorer_permutations(g, n, actuel, debut + 1, meilleur);

        temp = actuel[debut];
        actuel[debut] = actuel[i];
        actuel[i] = temp;
    }
}

Resultat force_brute(Graphe g, int n) {
    Resultat meilleur;
    meilleur.poids = DBL_MAX; 
    meilleur.chemin = creer_chemin(n);

    Chemin actuel = creer_chemin(n);

    explorer_permutations(g, n, actuel, 1, &meilleur);

    free(actuel);
    return meilleur;
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

    Resultat res_fb = force_brute(g, n);
printf("Force brute - Poids : %f\n", res_fb.poids);
printf("Meilleur chemin : ");
for (int i = 0; i < n; i++) {
    printf("%d ", res_fb.chemin[i]);
}
printf("\n");

free(res_fb.chemin);
    return 0;
}


// gcc -Wall -Wextra -Werror -Wvla -fsanitize=address,undefined -o main main.c -lm && ./main
