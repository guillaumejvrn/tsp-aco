#include <gtk/gtk.h>
#include <cairo.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>

// ============================================================================
// STRUCTURES DE DONNÉES
// ============================================================================
typedef struct {
    double x;
    double y;
    char nom[64];
} Sommet;

typedef struct {
    int n;
    Sommet* sommets;
    double optimal_ref; // 1ère ligne du fichier (-1 si inconnu)
} Graphe;

typedef struct {
    double poids;
    int* chemin;
    double temps_ms;
} Resultat;

typedef struct {
    // Données du graphe
    Graphe graphe;
    double** pheromone;

    // Paramètres ACO
    double alpha;
    double beta;
    double rho;
    double Q;
    int nb_fourmis;

    // État de la simulation
    int iteration;
    Resultat meilleur_global;
    Resultat meilleur_iter;
    Resultat meilleur_fb;
    bool afficher_fb;
    bool afficher_phero;
    bool afficher_noms;

    // Historique pour la courbe de convergence
    double* hist_meilleur;
    double* hist_moyenne;
    int hist_capacite;

    // Timer & Widgets GTK
    guint timer_id;
    int vitesse_ms;
    GtkWidget* drawing_graph;
    GtkWidget* drawing_chart;
    GtkWidget* lbl_stats;
    GtkWidget* lbl_diag;
    GtkWidget* btn_play;
    GtkWidget* spin_alpha;
    GtkWidget* spin_beta;
    GtkWidget* spin_rho;
    GtkWidget* spin_fourmis;
    GtkWidget* spin_vitesse;
    GtkWidget* spin_alea;
} AppState;

// ============================================================================
// UTILITAIRES MATHÉMATIQUES & MÉMOIRE
// ============================================================================
static double temps_actuel_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static double distance_euc(Sommet a, Sommet b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}

static double calculer_poids(const Graphe* g, const int* chemin) {
    if (g->n < 2 || !chemin) return 0.0;
    double total = 0.0;
    for (int i = 0; i < g->n - 1; i++) {
        total += distance_euc(g->sommets[chemin[i]], g->sommets[chemin[i + 1]]);
    }
    total += distance_euc(g->sommets[chemin[g->n - 1]], g->sommets[chemin[0]]);
    return total;
}

static void liberer_simulation(AppState* app) {
    if (app->pheromone) {
        for (int i = 0; i < app->graphe.n; i++) {
            free(app->pheromone[i]);
        }
        free(app->pheromone);
        app->pheromone = NULL;
    }
    free(app->meilleur_global.chemin);
    app->meilleur_global.chemin = NULL;
    app->meilleur_global.poids = DBL_MAX;

    free(app->meilleur_iter.chemin);
    app->meilleur_iter.chemin = NULL;
    app->meilleur_iter.poids = DBL_MAX;

    free(app->hist_meilleur);
    free(app->hist_moyenne);
    app->hist_meilleur = NULL;
    app->hist_moyenne = NULL;
    app->hist_capacite = 0;
    app->iteration = 0;
}

static void initialiser_simulation(AppState* app) {
    liberer_simulation(app);
    int n = app->graphe.n;
    if (n < 3) return;

    app->pheromone = malloc(n * sizeof(double*));
    for (int i = 0; i < n; i++) {
        app->pheromone[i] = malloc(n * sizeof(double));
        for (int j = 0; j < n; j++) {
            app->pheromone[i][j] = 1.0;
        }
    }

    app->meilleur_global.chemin = malloc(n * sizeof(int));
    app->meilleur_global.poids = DBL_MAX;
    app->meilleur_iter.chemin = malloc(n * sizeof(int));
    app->meilleur_iter.poids = DBL_MAX;

    app->hist_capacite = 256;
    app->hist_meilleur = malloc(app->hist_capacite * sizeof(double));
    app->hist_moyenne = malloc(app->hist_capacite * sizeof(double));
}

// ============================================================================
// CHARGEMENT & GÉNÉRATION DE GRAPHE
// ============================================================================
static bool charger_graphe_fichier(AppState* app, const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) return false;

    char ligne[256];
    double opt = -1.0;
    bool premiere_lue = false;

    int cap = 64;
    int count = 0;
    Sommet* tab = malloc(cap * sizeof(Sommet));

    while (fgets(ligne, sizeof(ligne), f)) {
        // Ignorer lignes vides ou commentaires
        char* p = ligne;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\n' || *p == '\r' || *p == '#') continue;

        if (!premiere_lue) {
            if (sscanf(p, "%lf", &opt) != 1) {
                free(tab);
                fclose(f);
                return false;
            }
            premiere_lue = true;
            continue;
        }

        double x, y;
        char nom[64] = "";
        int lus = sscanf(p, "%lf %lf %63s", &x, &y, nom);
        if (lus >= 2) {
            if (count >= cap) {
                cap *= 2;
                tab = realloc(tab, cap * sizeof(Sommet));
            }
            tab[count].x = x;
            tab[count].y = y;
            if (lus == 3) {
                strncpy(tab[count].nom, nom, 63);
                tab[count].nom[63] = '\0';
            } else {
                snprintf(tab[count].nom, 64, "V%d", count + 1);
            }
            count++;
        }
    }
    fclose(f);

    if (count < 3) {
        free(tab);
        return false;
    }

    liberer_simulation(app);
    free(app->graphe.sommets);
    free(app->meilleur_fb.chemin);
    app->meilleur_fb.chemin = NULL;
    app->meilleur_fb.poids = DBL_MAX;

    app->graphe.sommets = tab;
    app->graphe.n = count;
    app->graphe.optimal_ref = opt;

    initialiser_simulation(app);
    return true;
}

static void generer_graphe_aleatoire(AppState* app, int n) {
    liberer_simulation(app);
    free(app->graphe.sommets);
    free(app->meilleur_fb.chemin);
    app->meilleur_fb.chemin = NULL;
    app->meilleur_fb.poids = DBL_MAX;

    app->graphe.n = n;
    app->graphe.optimal_ref = -1.0;
    app->graphe.sommets = malloc(n * sizeof(Sommet));

    for (int i = 0; i < n; i++) {
        app->graphe.sommets[i].x = 5.0 + (rand() % 900) / 10.0;
        app->graphe.sommets[i].y = 5.0 + (rand() % 900) / 10.0;
        snprintf(app->graphe.sommets[i].nom, 64, "V%d", i + 1);
    }

    initialiser_simulation(app);
}

// ============================================================================
// MOTEUR ACO (PAS À PAS) & FORCE BRUTE
// ============================================================================
static void executer_une_iteration_aco(AppState* app) {
    int n = app->graphe.n;
    if (n < 3) return;

    double t0 = temps_actuel_ms();
    int nb_f = app->nb_fourmis;

    int** chemins_fourmis = malloc(nb_f * sizeof(int*));
    double* poids_fourmis = malloc(nb_f * sizeof(double));
    double somme_poids = 0.0;
    app->meilleur_iter.poids = DBL_MAX;

    for (int k = 0; k < nb_f; k++) {
        int* chemin = malloc(n * sizeof(int));
        bool* visite = calloc(n, sizeof(bool));

        int actuel = rand() % n;
        chemin[0] = actuel;
        visite[actuel] = true;

        for (int step = 1; step < n; step++) {
            double* probs = calloc(n, sizeof(double));
            double somme = 0.0;

            for (int i = 0; i < n; i++) {
                if (!visite[i]) {
                    double d = distance_euc(app->graphe.sommets[actuel], app->graphe.sommets[i]);
                    double vis = (d > 1e-9) ? (1.0 / d) : 1e9;
                    probs[i] = pow(app->pheromone[actuel][i], app->alpha) * pow(vis, app->beta);
                    somme += probs[i];
                }
            }

            int prochain = -1;
            if (somme > 0.0) {
                double tirage = ((double)rand() / RAND_MAX) * somme;
                double cumul = 0.0;
                for (int i = 0; i < n; i++) {
                    if (!visite[i]) {
                        cumul += probs[i];
                        if (tirage <= cumul) {
                            prochain = i;
                            break;
                        }
                    }
                }
            }
            if (prochain == -1) {
                for (int i = 0; i < n; i++) {
                    if (!visite[i]) {
                        prochain = i;
                        break;
                    }
                }
            }

            free(probs);
            chemin[step] = prochain;
            visite[prochain] = true;
            actuel = prochain;
        }

        double p = calculer_poids(&app->graphe, chemin);
        chemins_fourmis[k] = chemin;
        poids_fourmis[k] = p;
        somme_poids += p;

        if (p < app->meilleur_iter.poids) {
            app->meilleur_iter.poids = p;
            memcpy(app->meilleur_iter.chemin, chemin, n * sizeof(int));
        }
        if (p < app->meilleur_global.poids) {
            app->meilleur_global.poids = p;
            memcpy(app->meilleur_global.chemin, chemin, n * sizeof(int));
        }

        free(visite);
    }

    // 1. Évaporation
    for (int u = 0; u < n; u++) {
        for (int v = 0; v < n; v++) {
            app->pheromone[u][v] *= (1.0 - app->rho);
            if (app->pheromone[u][v] < 1e-6) app->pheromone[u][v] = 1e-6;
        }
    }

    // 2. Dépôt des phéromones proportionnel à Q / poids
    for (int k = 0; k < nb_f; k++) {
        double depot = app->Q / poids_fourmis[k];
        for (int i = 0; i < n; i++) {
            int u = chemins_fourmis[k][i];
            int v = chemins_fourmis[k][(i + 1) % n];
            app->pheromone[u][v] += depot;
            app->pheromone[v][u] += depot;
        }
        free(chemins_fourmis[k]);
    }
    free(chemins_fourmis);
    free(poids_fourmis);

    double t1 = temps_actuel_ms();
    app->meilleur_global.temps_ms = t1 - t0;

    // Enregistrement historique
    if (app->iteration >= app->hist_capacite) {
        app->hist_capacite *= 2;
        app->hist_meilleur = realloc(app->hist_meilleur, app->hist_capacite * sizeof(double));
        app->hist_moyenne = realloc(app->hist_moyenne, app->hist_capacite * sizeof(double));
    }
    app->hist_meilleur[app->iteration] = app->meilleur_global.poids;
    app->hist_moyenne[app->iteration] = somme_poids / nb_f;
    app->iteration++;
}

static void explorer_permutations(const Graphe* g, int* actuel, int debut, Resultat* meilleur) {
    if (debut == g->n) {
        double p = calculer_poids(g, actuel);
        if (p < meilleur->poids) {
            meilleur->poids = p;
            memcpy(meilleur->chemin, actuel, g->n * sizeof(int));
        }
        return;
    }
    for (int i = debut; i < g->n; i++) {
        int tmp = actuel[debut];
        actuel[debut] = actuel[i];
        actuel[i] = tmp;

        explorer_permutations(g, actuel, debut + 1, meilleur);

        tmp = actuel[debut];
        actuel[debut] = actuel[i];
        actuel[i] = tmp;
    }
}

static void executer_force_brute(AppState* app) {
    int n = app->graphe.n;
    if (n < 3 || n > 12) return;

    double t0 = temps_actuel_ms();
    free(app->meilleur_fb.chemin);
    app->meilleur_fb.chemin = malloc(n * sizeof(int));
    app->meilleur_fb.poids = DBL_MAX;

    int* actuel = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) actuel[i] = i;

    explorer_permutations(&app->graphe, actuel, 1, &app->meilleur_fb);
    free(actuel);

    app->meilleur_fb.temps_ms = temps_actuel_ms() - t0;
}

// ============================================================================
// RENDU GRAPHIQUE CAIRO : CARTE DU GRAPHE & PHÉROMONES
// ============================================================================
static gboolean on_draw_graph(GtkWidget* widget, cairo_t* cr, gpointer data) {
    AppState* app = (AppState*)data;
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);

    // Fond sombre
    cairo_set_source_rgb(cr, 0.09, 0.10, 0.12);
    cairo_paint(cr);

    int n = app->graphe.n;
    if (n < 3) return FALSE;

    // Boîte englobante
    double minX = 1e18, maxX = -1e18, minY = 1e18, maxY = -1e18;
    for (int i = 0; i < n; i++) {
        if (app->graphe.sommets[i].x < minX) minX = app->graphe.sommets[i].x;
        if (app->graphe.sommets[i].x > maxX) maxX = app->graphe.sommets[i].x;
        if (app->graphe.sommets[i].y < minY) minY = app->graphe.sommets[i].y;
        if (app->graphe.sommets[i].y > maxY) maxY = app->graphe.sommets[i].y;
    }

    double margin = 45.0;
    double spanX = (maxX - minX > 1e-6) ? (maxX - minX) : 1.0;
    double spanY = (maxY - minY > 1e-6) ? (maxY - minY) : 1.0;
    double scaleX = (width - 2 * margin) / spanX;
    double scaleY = (height - 2 * margin) / spanY;
    double scale = (scaleX < scaleY) ? scaleX : scaleY;

    #define TO_SX(x) (margin + ((x) - minX) * scale)
    #define TO_SY(y) (height - (margin + ((y) - minY) * scale))

    // 1. Pistes de phéromones
    if (app->afficher_phero && app->pheromone) {
        double maxP = 0.0;
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++)
                if (app->pheromone[i][j] > maxP) maxP = app->pheromone[i][j];

        if (maxP > 0.0) {
            for (int i = 0; i < n; i++) {
                for (int j = i + 1; j < n; j++) {
                    double ratio = app->pheromone[i][j] / maxP;
                    if (ratio < 0.08) continue;
                    cairo_set_source_rgba(cr, 0.0, 0.75, 1.0, ratio * 0.85);
                    cairo_set_line_width(cr, 1.0 + ratio * 5.5);
                    cairo_move_to(cr, TO_SX(app->graphe.sommets[i].x), TO_SY(app->graphe.sommets[i].y));
                    cairo_line_to(cr, TO_SX(app->graphe.sommets[j].x), TO_SY(app->graphe.sommets[j].y));
                    cairo_stroke(cr);
                }
            }
        }
    }

    // 2. Cycle Force Brute (vert)
    if (app->afficher_fb && app->meilleur_fb.chemin) {
        cairo_set_source_rgba(cr, 0.30, 0.90, 0.50, 0.75);
        cairo_set_line_width(cr, 4.0);
        for (int i = 0; i < n; i++) {
            int u = app->meilleur_fb.chemin[i];
            int v = app->meilleur_fb.chemin[(i + 1) % n];
            cairo_move_to(cr, TO_SX(app->graphe.sommets[u].x), TO_SY(app->graphe.sommets[u].y));
            cairo_line_to(cr, TO_SX(app->graphe.sommets[v].x), TO_SY(app->graphe.sommets[v].y));
        }
        cairo_stroke(cr);
    }

    // 3. Meilleur cycle global ACO (rouge corail)
    if (app->iteration > 0 && app->meilleur_global.chemin) {
        cairo_set_source_rgb(cr, 1.0, 0.33, 0.37);
        cairo_set_line_width(cr, 2.5);
        for (int i = 0; i < n; i++) {
            int u = app->meilleur_global.chemin[i];
            int v = app->meilleur_global.chemin[(i + 1) % n];
            cairo_move_to(cr, TO_SX(app->graphe.sommets[u].x), TO_SY(app->graphe.sommets[u].y));
            cairo_line_to(cr, TO_SX(app->graphe.sommets[v].x), TO_SY(app->graphe.sommets[v].y));
        }
        cairo_stroke(cr);
    }

    // 4. Sommets et noms
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);

    for (int i = 0; i < n; i++) {
        double sx = TO_SX(app->graphe.sommets[i].x);
        double sy = TO_SY(app->graphe.sommets[i].y);

        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.18);
        cairo_arc(cr, sx, sy, 8.5, 0, 2 * M_PI);
        cairo_fill(cr);

        if (i == 0) cairo_set_source_rgb(cr, 1.0, 0.84, 0.0);
        else cairo_set_source_rgb(cr, 0.94, 0.96, 0.98);
        cairo_arc(cr, sx, sy, 4.5, 0, 2 * M_PI);
        cairo_fill(cr);

        if (app->afficher_noms) {
            cairo_set_source_rgb(cr, 0.86, 0.88, 0.92);
            cairo_move_to(cr, sx + 7, sy - 5);
            cairo_show_text(cr, app->graphe.sommets[i].nom);
        }
    }

    // Légende
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 1.0, 0.33, 0.37);
    cairo_move_to(cr, 15, 22);
    cairo_show_text(cr, "— Meilleur cycle ACO");

    cairo_set_source_rgb(cr, 0.0, 0.75, 1.0);
    cairo_move_to(cr, 165, 22);
    cairo_show_text(cr, "— Intensité Phéromones");

    if (app->afficher_fb && app->meilleur_fb.chemin) {
        cairo_set_source_rgb(cr, 0.30, 0.90, 0.50);
        cairo_move_to(cr, 335, 22);
        cairo_show_text(cr, "— Optimal Force Brute");
    }

    #undef TO_SX
    #undef TO_SY
    return FALSE;
}

// ============================================================================
// RENDU GRAPHIQUE CAIRO : COURBE DE CONVERGENCE (INFLUENCE ALPHA / BETA)
// ============================================================================
static gboolean on_draw_chart(GtkWidget* widget, cairo_t* cr, gpointer data) {
    AppState* app = (AppState*)data;
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);

    cairo_set_source_rgb(cr, 0.12, 0.13, 0.16);
    cairo_paint(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.75, 0.78, 0.82);
    cairo_move_to(cr, 12, 20);
    cairo_show_text(cr, "Convergence du Poids (Jaune = Moyenne colonie | Rouge = Meilleur ACO | Vert = Optimal)");

    if (app->iteration < 2) {
        cairo_set_source_rgb(cr, 0.55, 0.58, 0.62);
        cairo_move_to(cr, 12, height / 2);
        cairo_show_text(cr, "Exécutez au moins 2 itérations pour tracer la courbe d'influence de α et β.");
        return FALSE;
    }

    double minV = app->hist_meilleur[0];
    double maxV = app->hist_moyenne[0];
    for (int i = 0; i < app->iteration; i++) {
        if (app->hist_meilleur[i] < minV) minV = app->hist_meilleur[i];
        if (app->hist_moyenne[i] > maxV) maxV = app->hist_moyenne[i];
    }
    if (app->graphe.optimal_ref > 0.0 && app->graphe.optimal_ref < minV) {
        minV = app->graphe.optimal_ref;
    }

    double span = (maxV - minV > 1e-6) ? (maxV - minV) : 1.0;
    double left = 55.0, right = width - 20.0, top = 35.0, bottom = height - 22.0;
    double w = right - left;
    double h = bottom - top;

    // Axes
    cairo_set_source_rgb(cr, 0.35, 0.38, 0.42);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, left, top);
    cairo_line_to(cr, left, bottom);
    cairo_line_to(cr, right, bottom);
    cairo_stroke(cr);

    char buf[32];
    cairo_set_font_size(cr, 10.0);
    snprintf(buf, sizeof(buf), "%.1f", maxV);
    cairo_move_to(cr, 5, top + 5);
    cairo_show_text(cr, buf);

    snprintf(buf, sizeof(buf), "%.1f", minV);
    cairo_move_to(cr, 5, bottom);
    cairo_show_text(cr, buf);

    // Ligne optimale de référence
    if (app->graphe.optimal_ref > 0.0) {
        double yOpt = bottom - ((app->graphe.optimal_ref - minV) / span) * h;
        double dashes[] = {5.0, 5.0};
        cairo_set_dash(cr, dashes, 2, 0);
        cairo_set_source_rgb(cr, 0.30, 0.90, 0.50);
        cairo_move_to(cr, left, yOpt);
        cairo_line_to(cr, right, yOpt);
        cairo_stroke(cr);
        cairo_set_dash(cr, NULL, 0, 0);
    }

    // Courbe moyenne (jaune)
    cairo_set_source_rgba(cr, 1.0, 0.75, 0.25, 0.7);
    cairo_set_line_width(cr, 1.5);
    for (int i = 0; i < app->iteration; i++) {
        double x = left + ((double)i / (app->iteration - 1)) * w;
        double y = bottom - ((app->hist_moyenne[i] - minV) / span) * h;
        if (i == 0) cairo_move_to(cr, x, y);
        else cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);

    // Courbe meilleur (rouge)
    cairo_set_source_rgb(cr, 1.0, 0.33, 0.37);
    cairo_set_line_width(cr, 2.2);
    for (int i = 0; i < app->iteration; i++) {
        double x = left + ((double)i / (app->iteration - 1)) * w;
        double y = bottom - ((app->hist_meilleur[i] - minV) / span) * h;
        if (i == 0) cairo_move_to(cr, x, y);
        else cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);

    return FALSE;
}

// ============================================================================
// MISE À JOUR INTERFACE & CALLBACKS GTK
// ============================================================================
static void mettre_a_jour_diagnostic_params(AppState* app) {
    double a = app->alpha;
    double b = app->beta;
    const char* msg;

    if (a == 0.0 && b == 0.0) {
        msg = "<i>α=0, β=0 : Marche purement aléatoire (ni mémoire, ni distance).</i>";
    } else if (a == 0.0) {
        msg = "<i>α=0 : Phéromones ignorées → recherche gloutonne probabiliste.</i>";
    } else if (b == 0.0) {
        msg = "<i>β=0 : Distances ignorées → convergence lente ou bloquée.</i>";
    } else if (a > b * 1.5) {
        msg = "<i>α dominant : Forte amplification des pistes → convergence rapide (risque minimum local).</i>";
    } else if (b > a * 1.5) {
        msg = "<i>β dominant : Forte attirance vers les voisins proches → bon départ, exploration réduite.</i>";
    } else {
        msg = "<i>Équilibre α / β : Compromis optimal entre exploration et exploitation.</i>";
    }
    gtk_label_set_markup(GTK_LABEL(app->lbl_diag), msg);
}

static void rafraichir_interface(AppState* app) {
    char stats[512];
    double ref = (app->graphe.optimal_ref > 0.0) ? app->graphe.optimal_ref
               : (app->meilleur_fb.chemin ? app->meilleur_fb.poids : -1.0);

    if (app->iteration > 0 && ref > 0.0) {
        double ecart = ((app->meilleur_global.poids - ref) / ref) * 100.0;
        snprintf(stats, sizeof(stats),
                 "<b>Sommets :</b> %d  |  <b>Itération :</b> %d\n"
                 "<b>Meilleur ACO :</b> <span foreground='#e04856'>%.2f</span> (%.2f ms/pas)\n"
                 "<b>Optimal réf. :</b> <span foreground='#2e9e5b'>%.2f</span> (Écart : <b>+%.2f %%</b>)",
                 app->graphe.n, app->iteration,
                 app->meilleur_global.poids, app->meilleur_global.temps_ms,
                 ref, ecart);
    } else if (app->iteration > 0) {
        snprintf(stats, sizeof(stats),
                 "<b>Sommets :</b> %d  |  <b>Itération :</b> %d\n"
                 "<b>Meilleur ACO :</b> <span foreground='#e04856'>%.2f</span> (%.2f ms/pas)",
                 app->graphe.n, app->iteration,
                 app->meilleur_global.poids, app->meilleur_global.temps_ms);
    } else {
        snprintf(stats, sizeof(stats),
                 "<b>Sommets :</b> %d  |  <b>Itération :</b> 0\n"
                 "<b>Optimal fichier :</b> %s",
                 app->graphe.n,
                 (app->graphe.optimal_ref > 0.0) ? "Chargé" : "Non défini");
    }

    gtk_label_set_markup(GTK_LABEL(app->lbl_stats), stats);
    gtk_widget_queue_draw(app->drawing_graph);
    gtk_widget_queue_draw(app->drawing_chart);
}

static void on_step_clicked(GtkButton* btn, gpointer data) {
    (void)btn;
    AppState* app = (AppState*)data;
    executer_une_iteration_aco(app);
    rafraichir_interface(app);
}

static gboolean on_timer_tick(gpointer data) {
    AppState* app = (AppState*)data;
    executer_une_iteration_aco(app);
    rafraichir_interface(app);
    return G_SOURCE_CONTINUE;
}

static void on_play_clicked(GtkButton* btn, gpointer data) {
    AppState* app = (AppState*)data;
    if (app->timer_id > 0) {
        g_source_remove(app->timer_id);
        app->timer_id = 0;
        gtk_button_set_label(btn, "▶ Lancer");
    } else {
        app->timer_id = g_timeout_add(app->vitesse_ms, on_timer_tick, app);
        gtk_button_set_label(btn, "⏸ Pause");
    }
}

static void on_reset_clicked(GtkButton* btn, gpointer data) {
    (void)btn;
    AppState* app = (AppState*)data;
    if (app->timer_id > 0) {
        g_source_remove(app->timer_id);
        app->timer_id = 0;
        gtk_button_set_label(GTK_BUTTON(app->btn_play), "▶ Lancer");
    }
    initialiser_simulation(app);
    rafraichir_interface(app);
}

static void on_alea_clicked(GtkButton* btn, gpointer data) {
    (void)btn;
    AppState* app = (AppState*)data;
    if (app->timer_id > 0) {
        g_source_remove(app->timer_id);
        app->timer_id = 0;
        gtk_button_set_label(GTK_BUTTON(app->btn_play), "▶ Lancer");
    }
    int n = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->spin_alea));
    generer_graphe_aleatoire(app, n);
    rafraichir_interface(app);
}

static void on_charger_clicked(GtkButton* btn, gpointer data) {
    (void)btn;
    AppState* app = (AppState*)data;
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        "Ouvrir un graphe (Ligne 1 = Optimal, puis x y nom)",
        NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Annuler", GTK_RESPONSE_CANCEL,
        "_Ouvrir", GTK_RESPONSE_ACCEPT, NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (app->timer_id > 0) {
            g_source_remove(app->timer_id);
            app->timer_id = 0;
            gtk_button_set_label(GTK_BUTTON(app->btn_play), "▶ Lancer");
        }
        if (!charger_graphe_fichier(app, filename)) {
            GtkWidget* err = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK, "Format invalide.\nAttendu : Ligne 1 = poids minimal, puis 'x y nom'.");
            gtk_dialog_run(GTK_DIALOG(err));
            gtk_widget_destroy(err);
        }
        g_free(filename);
        rafraichir_interface(app);
    }
    gtk_widget_destroy(dialog);
}

static void on_brute_clicked(GtkButton* btn, gpointer data) {
    (void)btn;
    AppState* app = (AppState*)data;
    if (app->graphe.n > 12) {
        GtkWidget* msg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
            GTK_BUTTONS_OK, "Force brute limitée à 12 sommets max (12! = 479M permutations).");
        gtk_dialog_run(GTK_DIALOG(msg));
        gtk_widget_destroy(msg);
        return;
    }
    executer_force_brute(app);
    app->afficher_fb = true;
    rafraichir_interface(app);
}

static void on_param_changed(GtkSpinButton* spin, gpointer data) {
    (void)spin;
    AppState* app = (AppState*)data;
    app->alpha = gtk_spin_button_get_value(GTK_SPIN_BUTTON(app->spin_alpha));
    app->beta = gtk_spin_button_get_value(GTK_SPIN_BUTTON(app->spin_beta));
    app->rho = gtk_spin_button_get_value(GTK_SPIN_BUTTON(app->spin_rho));
    app->nb_fourmis = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->spin_fourmis));
    app->vitesse_ms = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->spin_vitesse));

    if (app->timer_id > 0) {
        g_source_remove(app->timer_id);
        app->timer_id = g_timeout_add(app->vitesse_ms, on_timer_tick, app);
    }
    mettre_a_jour_diagnostic_params(app);
}

static void on_toggle_phero(GtkToggleButton* tb, gpointer data) {
    AppState* app = (AppState*)data;
    app->afficher_phero = gtk_toggle_button_get_active(tb);
    gtk_widget_queue_draw(app->drawing_graph);
}

static void on_toggle_noms(GtkToggleButton* tb, gpointer data) {
    AppState* app = (AppState*)data;
    app->afficher_noms = gtk_toggle_button_get_active(tb);
    gtk_widget_queue_draw(app->drawing_graph);
}

// ============================================================================
// POINT D'ENTRÉE & CONSTRUCTION DE L'INTERFACE
// ============================================================================
int main(int argc, char** argv) {
    srand((unsigned int)time(NULL));
    gtk_init(&argc, &argv);

    AppState app = {0};
    app.alpha = 1.0;
    app.beta = 2.0;
    app.rho = 0.35;
    app.Q = 100.0;
    app.nb_fourmis = 20;
    app.vitesse_ms = 120;
    app.afficher_phero = true;
    app.afficher_noms = true;

    generer_graphe_aleatoire(&app, 10);

    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Démonstrateur TSP : ACO vs Force Brute (GTK3 / C)");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 760);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 8);
    gtk_container_add(GTK_CONTAINER(window), hbox);

    // --- ZONE GAUCHE : GRAPHE + COURBE ---
    GtkWidget* vpaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(hbox), vpaned, TRUE, TRUE, 0);

    app.drawing_graph = gtk_drawing_area_new();
    gtk_widget_set_size_request(app.drawing_graph, 650, 460);
    g_signal_connect(app.drawing_graph, "draw", G_CALLBACK(on_draw_graph), &app);

    app.drawing_chart = gtk_drawing_area_new();
    gtk_widget_set_size_request(app.drawing_chart, 650, 200);
    g_signal_connect(app.drawing_chart, "draw", G_CALLBACK(on_draw_chart), &app);

    gtk_paned_pack1(GTK_PANED(vpaned), app.drawing_graph, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(vpaned), app.drawing_chart, FALSE, FALSE);

    // --- ZONE DROITE : CONTRÔLES ---
    GtkWidget* vbox_ctrl = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_size_request(vbox_ctrl, 360, -1);
    gtk_box_pack_start(GTK_BOX(hbox), vbox_ctrl, FALSE, FALSE, 0);

    // 1. Données du graphe
    GtkWidget* frame_g = gtk_frame_new("1. Données du Graphe");
    GtkWidget* box_g = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(box_g), 8);
    gtk_container_add(GTK_CONTAINER(frame_g), box_g);

    GtkWidget* btn_charger = gtk_button_new_with_label("Charger fichier (.txt : Opt + x y nom)");
    g_signal_connect(btn_charger, "clicked", G_CALLBACK(on_charger_clicked), &app);
    gtk_box_pack_start(GTK_BOX(box_g), btn_charger, FALSE, FALSE, 0);

    GtkWidget* box_alea = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    app.spin_alea = gtk_spin_button_new_with_range(3, 150, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app.spin_alea), 10);
    GtkWidget* btn_alea = gtk_button_new_with_label("Générer aléatoire");
    g_signal_connect(btn_alea, "clicked", G_CALLBACK(on_alea_clicked), &app);
    gtk_box_pack_start(GTK_BOX(box_alea), gtk_label_new("Sommets :"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box_alea), app.spin_alea, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box_alea), btn_alea, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box_g), box_alea, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_ctrl), frame_g, FALSE, FALSE, 0);

    // 2. Exécution Pas à Pas
    GtkWidget* frame_e = gtk_frame_new("2. Exécution Pas-à-Pas");
    GtkWidget* grid_e = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid_e), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid_e), 6);
    gtk_container_set_border_width(GTK_CONTAINER(grid_e), 8);
    gtk_container_add(GTK_CONTAINER(frame_e), grid_e);

    GtkWidget* btn_step = gtk_button_new_with_label("⏭ 1 Pas (Itération)");
    app.btn_play = gtk_button_new_with_label("▶ Lancer");
    GtkWidget* btn_reset = gtk_button_new_with_label("↺ Réinitialiser");
    app.spin_vitesse = gtk_spin_button_new_with_range(10, 2000, 10);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app.spin_vitesse), 120);
    GtkWidget* btn_brute = gtk_button_new_with_label("⚡ Comparer Force Brute (n ≤ 12)");

    g_signal_connect(btn_step, "clicked", G_CALLBACK(on_step_clicked), &app);
    g_signal_connect(app.btn_play, "clicked", G_CALLBACK(on_play_clicked), &app);
    g_signal_connect(btn_reset, "clicked", G_CALLBACK(on_reset_clicked), &app);
    g_signal_connect(app.spin_vitesse, "value-changed", G_CALLBACK(on_param_changed), &app);
    g_signal_connect(btn_brute, "clicked", G_CALLBACK(on_brute_clicked), &app);

    gtk_grid_attach(GTK_GRID(grid_e), btn_step, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_e), app.btn_play, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_e), btn_reset, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_e), app.spin_vitesse, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_e), btn_brute, 0, 2, 2, 1);
    gtk_box_pack_start(GTK_BOX(vbox_ctrl), frame_e, FALSE, FALSE, 0);

    // 3. Influence des paramètres Alpha / Beta
    GtkWidget* frame_p = gtk_frame_new("3. Influence des Paramètres (α et β)");
    GtkWidget* grid_p = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid_p), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid_p), 6);
    gtk_container_set_border_width(GTK_CONTAINER(grid_p), 8);
    gtk_container_add(GTK_CONTAINER(frame_p), grid_p);

    app.spin_alpha = gtk_spin_button_new_with_range(0.0, 10.0, 0.2);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app.spin_alpha), app.alpha);
    app.spin_beta = gtk_spin_button_new_with_range(0.0, 10.0, 0.2);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app.spin_beta), app.beta);
    app.spin_rho = gtk_spin_button_new_with_range(0.01, 0.99, 0.05);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app.spin_rho), app.rho);
    app.spin_fourmis = gtk_spin_button_new_with_range(1, 200, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app.spin_fourmis), app.nb_fourmis);

    g_signal_connect(app.spin_alpha, "value-changed", G_CALLBACK(on_param_changed), &app);
    g_signal_connect(app.spin_beta, "value-changed", G_CALLBACK(on_param_changed), &app);
    g_signal_connect(app.spin_rho, "value-changed", G_CALLBACK(on_param_changed), &app);
    g_signal_connect(app.spin_fourmis, "value-changed", G_CALLBACK(on_param_changed), &app);

    app.lbl_diag = gtk_label_new("");
    gtk_label_set_line_wrap(GTK_LABEL(app.lbl_diag), TRUE);
    gtk_widget_set_size_request(app.lbl_diag, 320, -1);

    gtk_grid_attach(GTK_GRID(grid_p), gtk_label_new("α (Phéromones) :"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_p), app.spin_alpha, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_p), gtk_label_new("β (Distance 1/d) :"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_p), app.spin_beta, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_p), gtk_label_new("ρ (Évaporation) :"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_p), app.spin_rho, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_p), gtk_label_new("Nb fourmis :"), 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_p), app.spin_fourmis, 1, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_p), app.lbl_diag, 0, 4, 2, 1);
    gtk_box_pack_start(GTK_BOX(vbox_ctrl), frame_p, FALSE, FALSE, 0);

    // 4. Métriques & Options d'affichage
    GtkWidget* frame_s = gtk_frame_new("4. Résultats & Affichage");
    GtkWidget* box_s = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(box_s), 8);
    gtk_container_add(GTK_CONTAINER(frame_s), box_s);

    app.lbl_stats = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(app.lbl_stats), 0.0);
    gtk_box_pack_start(GTK_BOX(box_s), app.lbl_stats, FALSE, FALSE, 0);

    GtkWidget* chk_phero = gtk_check_button_new_with_label("Afficher les phéromones");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_phero), TRUE);
    g_signal_connect(chk_phero, "toggled", G_CALLBACK(on_toggle_phero), &app);

    GtkWidget* chk_noms = gtk_check_button_new_with_label("Afficher les noms des sommets");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_noms), TRUE);
    g_signal_connect(chk_noms, "toggled", G_CALLBACK(on_toggle_noms), &app);

    gtk_box_pack_start(GTK_BOX(box_s), chk_phero, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box_s), chk_noms, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_ctrl), frame_s, FALSE, FALSE, 0);

    mettre_a_jour_diagnostic_params(&app);
    rafraichir_interface(&app);

    gtk_widget_show_all(window);
    gtk_main();

    // Nettoyage final
    liberer_simulation(&app);
    free(app.graphe.sommets);
    free(app.meilleur_fb.chemin);
    return 0;
}