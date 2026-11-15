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

/*l evaporation se fait sur la globalité et pas que sur les chemins utilisés donc pas besoin des p@r@m chemins et n_chemins.
Ca nous mène à cette simple fonction */

void évaporation(int n, double** pheromone, float rho) {

    for(int u = 0; u < n; u++)
    {
        for(int v = 0; v < n; v++)
        {
            pheromone[u][v] *= (1 - rho);
        }
    }

}

int main() {
    //srand(time(NULL));
    return 0;
}