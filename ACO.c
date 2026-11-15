#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <float.h>

typedef struct {
    int x;
    int y;
} Coord;

typedef struct {
    Coord* graphe;
} Graphe;

typedef int* Chemin;

typedef struct {
    double poids;
    Chemin chemin;
} Resultat;

Resultat* explorer(int n, double** pheromone, Coord sommet_de_départ, double Q, double alpha, double beta) {
    Resultat* r = malloc(sizeof(Resultat));
    return r;
}

void évaporation(int n, double** pheromone, float rho) {
    return;
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
            Resultat* r = explorer(n, pheromone, graphe.graphe[ville_choisie], Q, alpha, beta);
            if (r -> poids < best_cost) {
                best_cost = r -> poids;
                best_path = r -> chemin;
            }
        }
        évaporation(n, pheromone, rho);
    }

    resultat -> poids = best_cost;
    resultat -> chemin = best_path;
    return resultat;
}

int main() {
    srand(time(NULL));
    return 0;
}