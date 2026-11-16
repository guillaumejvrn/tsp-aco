#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <math.h>
#include <time.h>
#include <float.h>
#define N 100

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

Resultat* force_brute(Graphe g, int n) {
    Resultat* meilleur = malloc(sizeof(Resultat));
    meilleur -> poids = DBL_MAX; 
    meilleur -> chemin = creer_chemin(n);

    Chemin actuel = creer_chemin(n);

    explorer_permutations(g, n, actuel, 1, meilleur);

    free(actuel);
    return meilleur;
}

Coord creer_coord(int x, int y) {
    Coord c;
    c.x = x;
    c.y = y;
    return c;
}

bool coord_est_dans_tableau(Coord* tab, int n, Coord c) {
    for (int i = 0; i < n; i++) {
        if (tab[i].x == c.x && tab[i].y == c.y) {
            return true;
        }
    }
    return false;
}

Graphe creer_graphe_aleatoire(int nb_sommet) {
    Graphe g;
    Coord* tableau_coord = malloc(nb_sommet*sizeof(Coord));
    for (int i = 0; i < nb_sommet; i++) {
        int x = rand() % N; 
        int y = rand() % N;
        Coord c = creer_coord(x, y);
        while (coord_est_dans_tableau(tableau_coord, i, c)) {
            x = rand() % N;
            y = rand() % N;
            c = creer_coord(x, y);
        }
        tableau_coord[i] = c;
    }
    g.graphe = tableau_coord;
    return g;
}

/////////////// PART 4

Resultat* explorer(Graphe graphe, int n, double** pheromone, int sommet_depart, double Q, double alpha, double beta) {
    
    Resultat* resultat = malloc(sizeof(Resultat));

    resultat->chemin = malloc((n + 1) * sizeof(int));

    int* visite = calloc(n, sizeof(int));

    int actuel = sommet_depart;
    int nb_visites = 1;

    resultat->chemin[0] = sommet_depart;
    visite[sommet_depart] = 1;

    while (nb_visites < n) {

        double* probabilites = calloc(n, sizeof(double));
        if (probabilites == NULL) {
            free(visite);
            free(resultat->chemin);
            free(resultat);
            return NULL;
        }

        double somme = 0.0;

        for (int i = 0; i < n; i++) {

            if (!visite[i]) {

                double distance = distance_euc(
                    graphe.graphe[actuel],
                    graphe.graphe[i]
                );

                if (distance > 0.0 && pheromone[actuel][i] > 0.0) {

                    double visibilite = 1.0 / distance;

                    probabilites[i] = pow(pheromone[actuel][i], alpha) * pow(visibilite, beta);

                    somme += probabilites[i];
                }
            }
        }

        double tirage = ((double)rand() / RAND_MAX) * somme;

        double cumul = 0.0;
        int prochain = -1;

        for (int i = 0; i < n; i++) {

            cumul += probabilites[i];

            if (!visite[i] && probabilites[i] > 0.0
                && tirage <= cumul) {

                prochain = i;
                break;
            }
        }

        if (prochain == -1) {
            for (int i = 0; i < n; i++) {
                if (!visite[i] && probabilites[i] > 0.0) {
                    prochain = i;
                    break;
                }
            }
        }

        free(probabilites);

        if (prochain == -1) {
            free(visite);
            free(resultat->chemin);
            free(resultat);
            return NULL;
        }

        double distance = distance_euc(
            graphe.graphe[actuel],
            graphe.graphe[prochain]
        );

        pheromone[actuel][prochain] += Q;
        pheromone[prochain][actuel] += Q;

        resultat->chemin[nb_visites] = prochain;

        visite[prochain] = 1;
        nb_visites++;

        actuel = prochain;
    }

    pheromone[actuel][sommet_depart] += Q;
    pheromone[sommet_depart][actuel] += Q;

    resultat->chemin[n] = sommet_depart;

    resultat->poids = calculer_poids(graphe, resultat->chemin, n);

    free(visite);

    return resultat;

}

void évaporation(int n, double** pheromone, float rho) {

    for(int u = 0; u < n; u++)
    {
        for(int v = 0; v < n; v++)
        {
            pheromone[u][v] *= (1 - rho);
        }
    }

}

void free_double(double** tab, int n) {
    for (int i = 0; i < n; i ++) {
        free(tab[i]);
    }
    free(tab);
}

Resultat* ACO(Graphe graphe, int n, int nb_itérations, int nb_fourmis, double alpha, double beta ,double Q, double rho) {
    
    Resultat* resultat = malloc(sizeof(Resultat));
    double** pheromone = malloc(n * sizeof(double*));

    for (int i = 0; i < n; i++) {
        double* tab = malloc(n * sizeof(double)); 
        for (int j = 0; j < n; j++) {
            tab[j] = Q;
        }
        pheromone[i] = tab;
    }
    Chemin best_path = malloc(sizeof(Chemin));
    best_path = NULL;
    int best_cost = INT_MAX;
    for (int i = 0; i < nb_itérations; i++) {
        for (int j = 0; j < nb_fourmis; j++) {
            int ville_choisie = rand() % n;
            Resultat* r = explorer(graphe, n, pheromone, ville_choisie, Q, alpha, beta);
            if (r -> poids < best_cost) {
                best_cost = r -> poids;
                best_path = r -> chemin;
            }
        }
        évaporation(n, pheromone, rho);
    }
    free_double(pheromone, n);
    resultat -> poids = best_cost;
    resultat -> chemin = best_path;
    free(best_path);
    return resultat;
}

void test(int nb_test) {
    for (int i = 3; i < nb_test; i++) {
        Graphe g = creer_graphe_aleatoire(i);
        time_t t0_aco;
        time(&t0_aco);
        Resultat* r_aco = ACO(g, i, 100, 15, 1, 1, 100, 0.35);
        time_t t1_aco;
        time(&t1_aco);
        time_t t_aco = t1_aco - t0_aco;
        printf("temps pour executer ACO : %ld\n", t_aco);
        time_t t0;
        time(&t0);
        Resultat* r_brute = force_brute(g, i);
        time_t t1;
        time(&t1);
        time_t t_brute = t1 - t0;
        printf("temps pour executer brute force : %ld\n", t_brute);
        printf("poids du cycle trouvé par aco: %lf\n", r_aco->poids);
        printf("poids du cycle trouvé par brute force: %lf\n", r_brute->poids);
        free(g.graphe);
        free(r_aco);
        free(r_brute);
    }
    
}

int main(void) {
    srand((unsigned int)time(NULL));

    test(13);

    /*Coord points_test1[5] = {
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

    Resultat* res_fb = force_brute(g, n);
    printf("Force brute - Poids : %f\n", res_fb->poids);
    printf("Meilleur chemin : ");
    for (int i = 0; i < n; i++) {
        printf("%d ", res_fb->chemin[i]);
    }
    printf("\n");

    free(res_fb->chemin);
        return 0;*/
}


// gcc -Wall -Wextra -Werror -std=c17 -g main.c -o main -lm
// ./main
