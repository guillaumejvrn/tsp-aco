Resultat* explorer(Graphe graphe, int n, double** pheromone, int sommet_depart, double Q, double alpha, double beta) {
    
    Resultat* resultat = malloc(sizeof(Resultat));

    resultat->chemin = malloc((n + 1) * sizeof(int));

    int* visite = calloc(n, sizeof(int));

    int actuel = sommet_depart;
    int nb_visites = 1;

    double poids = 0.0;

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

                    probabilites[i] =
                        pow(pheromone[actuel][i], alpha)
                        * pow(visibilite, beta);

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

        poids += distance;

        pheromone[actuel][prochain] += Q;
        pheromone[prochain][actuel] += Q;

        resultat->chemin[nb_visites] = prochain;

        visite[prochain] = 1;
        nb_visites++;

        actuel = prochain;
    }

    poids += distance_euc(
        graphe.graphe[actuel],
        graphe.graphe[sommet_depart]
    );

    pheromone[actuel][sommet_depart] += Q;
    pheromone[sommet_depart][actuel] += Q;

    resultat->chemin[n] = sommet_depart;

    resultat->poids = poids;

    free(visite);

    return resultat;

}