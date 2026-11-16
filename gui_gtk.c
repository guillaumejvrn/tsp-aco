#include <gtk/gtk.h>
#include <cairo.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

// Phases pédagogiques d'une itération ACO
typedef enum {
    PHASE_PRETE = 0,       // Prêt pour la phase 1 (Exploration)
    PHASE_EXPLORATION = 1, // Fourmis lancées, chemins visibles, prêt pour Phase 2 (Évaporation)
    PHASE_EVAPORATION = 2, // Évaporation appliquée, prêt pour Phase 3 (Dépôt)
    PHASE_DEPOT = 3        // Dépôt appliqué, prêt pour Phase 4 (Bilan)
} PhaseACO;

typedef struct {
    Graphe graphe;
    double** pheromone;

    // Paramètres ACO
    double alpha;
    double beta;
    double rho;
    double Q;
    int nb_fourmis;

    // État simulation & pas-à-pas détaillé
    int iteration;
    PhaseACO phase;
    int** chemins_vague;
    double* poids_vague;
    int nb_vague;
    double somme_poids_vague;
    double max_depot_vague;
    double t0_vague;

    Resultat meilleur_global;
    Resultat meilleur_iter;
    Resultat meilleur_fb;
    bool afficher_fb;
    bool afficher_phero;
    bool afficher_noms;
    bool afficher_colonie;

    // Historique courbe
    double* hist_meilleur;
    double* hist_moyenne;
    int hist_capacite;

    // Widgets & Timer
    guint timer_id;
    int vitesse_ms;
    GtkWidget* drawing_graph;
    GtkWidget* drawing_chart;
    GtkWidget* lbl_kpi_aco;
    GtkWidget* lbl_kpi_opt;
    GtkWidget* lbl_kpi_iter;
    GtkWidget* lbl_diag;
    GtkWidget* lbl_etape_algo;
    GtkWidget* btn_play;
    GtkWidget* spin_alpha;
    GtkWidget* spin_beta;
    GtkWidget* spin_rho;
    GtkWidget* spin_fourmis;
    GtkWidget* spin_vitesse;
    GtkWidget* spin_alea;
} AppState;

// ============================================================================
// FEUILLE DE STYLE CSS 100 % SANS SVG (ESTHÉTIQUE macOS / iOS DARK MODE)
// ============================================================================
static const char* MACOS_DARK_CSS =
    "* {"
    "  -gtk-icon-source: none;"
    "}"
    "window {"
    "  background-color: #141416;"
    "  color: #F5F5F7;"
    "  font-family: -apple-system, BlinkMacSystemFont, 'SF Pro Text', 'Helvetica Neue', sans-serif;"
    "  font-size: 13px;"
    "}"
    ".card {"
    "  background-color: #1E1E22;"
    "  border: 1px solid #2E2E34;"
    "  border-radius: 14px;"
    "  padding: 11px;"
    "}"
    ".section-title {"
    "  color: #98989D;"
    "  font-size: 11px;"
    "  font-weight: 700;"
    "  letter-spacing: 0.6px;"
    "}"
    ".kpi-box {"
    "  background-color: #26262B;"
    "  border: 1px solid #34343B;"
    "  border-radius: 10px;"
    "  padding: 7px 10px;"
    "}"
    "button {"
    "  background-image: none;"
    "  background-color: #2C2C32;"
    "  color: #F5F5F7;"
    "  border: 1px solid #3A3A42;"
    "  border-radius: 9px;"
    "  padding: 6px 10px;"
    "  font-weight: 600;"
    "  box-shadow: none;"
    "}"
    "button:hover {"
    "  background-color: #36363E;"
    "  border-color: #484852;"
    "}"
    "button:active {"
    "  background-color: #242429;"
    "}"
    "button.primary {"
    "  background-color: #0A84FF;"
    "  border-color: #0A84FF;"
    "  color: #FFFFFF;"
    "}"
    "button.primary:hover {"
    "  background-color: #2494FF;"
    "  border-color: #2494FF;"
    "}"
    "button.step-phase {"
    "  background-color: rgba(255, 159, 10, 0.16);"
    "  border-color: rgba(255, 159, 10, 0.50);"
    "  color: #FF9F0A;"
    "}"
    "button.step-phase:hover {"
    "  background-color: rgba(255, 159, 10, 0.26);"
    "}"
    "button.accent-green {"
    "  background-color: rgba(48, 209, 88, 0.16);"
    "  border-color: rgba(48, 209, 88, 0.45);"
    "  color: #30D158;"
    "}"
    "button.accent-green:hover {"
    "  background-color: rgba(48, 209, 88, 0.25);"
    "}"
    "button.pill-toggle {"
    "  background-color: #26262B;"
    "  border: 1px solid #383840;"
    "  color: #98989D;"
    "  border-radius: 16px;"
    "  padding: 5px 10px;"
    "  font-size: 11px;"
    "}"
    "button.pill-toggle:checked {"
    "  background-color: rgba(10, 132, 255, 0.22);"
    "  border-color: #0A84FF;"
    "  color: #64D2FF;"
    "}"
    "spinbutton {"
    "  background-color: #28282E;"
    "  color: #F5F5F7;"
    "  border: 1px solid #383840;"
    "  border-radius: 8px;"
    "  padding: 2px 4px;"
    "}"
    "spinbutton entry {"
    "  background: transparent;"
    "  color: #F5F5F7;"
    "  border: none;"
    "}"
    "spinbutton button {"
    "  background-image: none;"
    "  background-color: #32323A;"
    "  border: none;"
    "  border-radius: 6px;"
    "  padding: 2px 6px;"
    "  margin: 2px;"
    "}"
    ".diag-pill {"
    "  background-color: rgba(10, 132, 255, 0.12);"
    "  border: 1px solid rgba(10, 132, 255, 0.30);"
    "  border-radius: 9px;"
    "  padding: 7px 10px;"
    "  color: #64D2FF;"
    "  font-size: 11.5px;"
    "}"
    ".algo-box {"
    "  background-color: #25252A;"
    "  border: 1px solid #383842;"
    "  border-radius: 10px;"
    "  padding: 9px 11px;"
    "  color: #E5E5EA;"
    "  font-size: 12px;"
    "}";

static void appliquer_style_macos(void) {
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, MACOS_DARK_CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER
    );
    g_object_unref(provider);
}

// ============================================================================
// UTILITAIRES CAIRO (RECTANGLES ARRONDIS)
// ============================================================================
static void cairo_rounded_rect(cairo_t* cr, double x, double y, double w, double h, double r) {
    double deg = M_PI / 180.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -90 * deg, 0 * deg);
    cairo_arc(cr, x + w - r, y + h - r, r, 0 * deg, 90 * deg);
    cairo_arc(cr, x + r, y + h - r, r, 90 * deg, 180 * deg);
    cairo_arc(cr, x + r, y + r, r, 180 * deg, 270 * deg);
    cairo_close_path(cr);
}

// ============================================================================
// UTILITAIRES MATHÉMATIQUES & MÉMOIRE (PORTABLE macOS / LINUX / WINDOWS)
// ============================================================================
static double temps_actuel_ms(void) {
    return g_get_monotonic_time() / 1000.0;
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

static void liberer_vague(AppState* app) {
    if (app->chemins_vague) {
        for (int k = 0; k < app->nb_vague; k++) {
            free(app->chemins_vague[k]);
        }
        free(app->chemins_vague);
        app->chemins_vague = NULL;
    }
    free(app->poids_vague);
    app->poids_vague = NULL;
    app->nb_vague = 0;
}

static void liberer_simulation(AppState* app) {
    liberer_vague(app);
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
    app->phase = PHASE_PRETE;
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
    app->phase = PHASE_PRETE;
}

// ============================================================================
// CHARGEMENT & GÉNÉRATION DE GRAPHE (GÈRE "nom x y" ET "x y nom")
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

        char col1[64] = "", col2[64] = "", col3[64] = "";
        int lus = sscanf(p, "%63s %63s %63s", col1, col2, col3);
        if (lus >= 2) {
            char *end1, *end2, *end3;
            double v1 = strtod(col1, &end1);
            double v2 = strtod(col2, &end2);
            double v3 = (lus == 3) ? strtod(col3, &end3) : 0.0;

            bool ok1 = (*end1 == '\0');
            bool ok2 = (*end2 == '\0');
            bool ok3 = (lus == 3 && *end3 == '\0');

            double x = 0.0, y = 0.0;
            char nom[64] = "";

            if (lus == 3 && ok2 && ok3 && (!ok1 || strchr(col1, '.') == NULL)) {
                strncpy(nom, col1, 63);
                nom[63] = '\0';
                x = v2;
                y = v3;
            } else if (ok1 && ok2) {
                x = v1;
                y = v2;
                if (lus == 3) {
                    strncpy(nom, col3, 63);
                    nom[63] = '\0';
                } else {
                    snprintf(nom, 64, "V%d", count + 1);
                }
            } else {
                continue;
            }

            if (count >= cap) {
                cap *= 2;
                tab = realloc(tab, cap * sizeof(Sommet));
            }
            tab[count].x = x;
            tab[count].y = y;
            strcpy(tab[count].nom, nom);
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
// MOTEUR ACO DÉCOMPOSÉ EN 4 SOUS-ÉTAPES PÉDAGOGIQUES
// ============================================================================

// Sous-étape 1 : Construction des m cycles par les fourmis
static void phase1_exploration_fourmis(AppState* app) {
    int n = app->graphe.n;
    if (n < 3) return;

    liberer_vague(app);
    app->t0_vague = temps_actuel_ms();

    int nb_f = app->nb_fourmis;
    app->nb_vague = nb_f;
    app->chemins_vague = malloc(nb_f * sizeof(int*));
    app->poids_vague = malloc(nb_f * sizeof(double));
    app->somme_poids_vague = 0.0;
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
        app->chemins_vague[k] = chemin;
        app->poids_vague[k] = p;
        app->somme_poids_vague += p;

        if (p < app->meilleur_iter.poids) {
            app->meilleur_iter.poids = p;
            memcpy(app->meilleur_iter.chemin, chemin, n * sizeof(int));
        }

        free(visite);
    }

    app->phase = PHASE_EXPLORATION;
}

// Sous-étape 2 : Évaporation globale des phéromones
static void phase2_evaporation(AppState* app) {
    int n = app->graphe.n;
    for (int u = 0; u < n; u++) {
        for (int v = 0; v < n; v++) {
            app->pheromone[u][v] *= (1.0 - app->rho);
            if (app->pheromone[u][v] < 1e-6) app->pheromone[u][v] = 1e-6;
        }
    }
    app->phase = PHASE_EVAPORATION;
}

// Sous-étape 3 : Dépôt proportionnel à Q / poids
static void phase3_depot_pheromones(AppState* app) {
    int n = app->graphe.n;
    app->max_depot_vague = 0.0;
    for (int k = 0; k < app->nb_vague; k++) {
        double depot = app->Q / app->poids_vague[k];
        if (depot > app->max_depot_vague) app->max_depot_vague = depot;
        for (int i = 0; i < n; i++) {
            int u = app->chemins_vague[k][i];
            int v = app->chemins_vague[k][(i + 1) % n];
            app->pheromone[u][v] += depot;
            app->pheromone[v][u] += depot;
        }
    }
    app->phase = PHASE_DEPOT;
}

// Sous-étape 4 : Validation du bilan, courbe et passage à l'itération suivante
static void phase4_bilan_iteration(AppState* app) {
    int n = app->graphe.n;
    if (app->meilleur_iter.poids < app->meilleur_global.poids) {
        app->meilleur_global.poids = app->meilleur_iter.poids;
        memcpy(app->meilleur_global.chemin, app->meilleur_iter.chemin, n * sizeof(int));
    }

    app->meilleur_global.temps_ms = temps_actuel_ms() - app->t0_vague;

    if (app->iteration >= app->hist_capacite) {
        app->hist_capacite *= 2;
        app->hist_meilleur = realloc(app->hist_meilleur, app->hist_capacite * sizeof(double));
        app->hist_moyenne = realloc(app->hist_moyenne, app->hist_capacite * sizeof(double));
    }
    app->hist_meilleur[app->iteration] = app->meilleur_global.poids;
    app->hist_moyenne[app->iteration] = app->somme_poids_vague / app->nb_vague;
    app->iteration++;

    app->phase = PHASE_PRETE;
}

// Exécute 1 sous-étape à la fois
static void executer_une_sous_etape(AppState* app) {
    switch (app->phase) {
        case PHASE_PRETE:       phase1_exploration_fourmis(app); break;
        case PHASE_EXPLORATION: phase2_evaporation(app);         break;
        case PHASE_EVAPORATION: phase3_depot_pheromones(app);    break;
        case PHASE_DEPOT:       phase4_bilan_iteration(app);     break;
    }
}

// Exécute une itération complète (termine la vague en cours si besoin)
static void executer_une_iteration_complete(AppState* app) {
    if (app->phase == PHASE_PRETE) {
        phase1_exploration_fourmis(app);
    }
    if (app->phase == PHASE_EXPLORATION) {
        phase2_evaporation(app);
    }
    if (app->phase == PHASE_EVAPORATION) {
        phase3_depot_pheromones(app);
    }
    if (app->phase == PHASE_DEPOT) {
        phase4_bilan_iteration(app);
    }
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
// RENDU GRAPHIQUE CAIRO : CARTE STYLE APPLE MAPS / KEYNOTE
// ============================================================================
static gboolean on_draw_graph(GtkWidget* widget, cairo_t* cr, gpointer data) {
    AppState* app = (AppState*)data;
    double w = gtk_widget_get_allocated_width(widget);
    double h = gtk_widget_get_allocated_height(widget);

    cairo_rounded_rect(cr, 0.5, 0.5, w - 1.0, h - 1.0, 14.0);
    cairo_set_source_rgb(cr, 0.11, 0.11, 0.13);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.19, 0.19, 0.22);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke_preserve(cr);
    cairo_clip(cr);

    // Grille subtile
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.03);
    cairo_set_line_width(cr, 1.0);
    double step_grid = 36.0;
    for (double gx = step_grid; gx < w; gx += step_grid) {
        cairo_move_to(cr, gx, 0);
        cairo_line_to(cr, gx, h);
    }
    for (double gy = step_grid; gy < h; gy += step_grid) {
        cairo_move_to(cr, 0, gy);
        cairo_line_to(cr, w, gy);
    }
    cairo_stroke(cr);

    int n = app->graphe.n;
    if (n < 3) return FALSE;

    double minX = 1e18, maxX = -1e18, minY = 1e18, maxY = -1e18;
    for (int i = 0; i < n; i++) {
        if (app->graphe.sommets[i].x < minX) minX = app->graphe.sommets[i].x;
        if (app->graphe.sommets[i].x > maxX) maxX = app->graphe.sommets[i].x;
        if (app->graphe.sommets[i].y < minY) minY = app->graphe.sommets[i].y;
        if (app->graphe.sommets[i].y > maxY) maxY = app->graphe.sommets[i].y;
    }

    double pad_top = 56.0, pad_side = 48.0, pad_bottom = 36.0;
    double spanX = (maxX - minX > 1e-6) ? (maxX - minX) : 1.0;
    double spanY = (maxY - minY > 1e-6) ? (maxY - minY) : 1.0;
    double scaleX = (w - 2 * pad_side) / spanX;
    double scaleY = (h - pad_top - pad_bottom) / spanY;
    double scale = (scaleX < scaleY) ? scaleX : scaleY;

    double offsetX = (w - spanX * scale) / 2.0;
    double offsetY = pad_top + ((h - pad_top - pad_bottom) - spanY * scale) / 2.0;

    #define TO_SX(x) (offsetX + ((x) - minX) * scale)
    #define TO_SY(y) (h - (offsetY + ((y) - minY) * scale) + (pad_top - pad_bottom) * 0.5)

    // 0. Tous les chemins explorés par les fourmis de la vague courante (gris translucide)
    if (app->afficher_colonie && app->chemins_vague && app->nb_vague > 0) {
        double alpha_col = (app->phase == PHASE_EXPLORATION) ? 0.22 : 0.08;
        cairo_set_source_rgba(cr, 0.85, 0.85, 0.92, alpha_col);
        cairo_set_line_width(cr, 1.0);
        int max_draw = (app->nb_vague > 40) ? 40 : app->nb_vague;
        for (int k = 0; k < max_draw; k++) {
            for (int i = 0; i < n; i++) {
                int u = app->chemins_vague[k][i];
                int v = app->chemins_vague[k][(i + 1) % n];
                cairo_move_to(cr, TO_SX(app->graphe.sommets[u].x), TO_SY(app->graphe.sommets[u].y));
                cairo_line_to(cr, TO_SX(app->graphe.sommets[v].x), TO_SY(app->graphe.sommets[v].y));
            }
        }
        cairo_stroke(cr);
    }

    // 1. Pistes de phéromones (dès que l'itération > 0 ou pendant les phases 2/3)
    bool has_phero_activity = (app->iteration > 0 || app->phase >= PHASE_EVAPORATION);
    if (has_phero_activity && app->afficher_phero && app->pheromone) {
        double maxP = 0.0;
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++)
                if (app->pheromone[i][j] > maxP) maxP = app->pheromone[i][j];

        if (maxP > 0.0) {
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            for (int i = 0; i < n; i++) {
                for (int j = i + 1; j < n; j++) {
                    double ratio = app->pheromone[i][j] / maxP;
                    if (ratio < 0.08) continue;
                    double op = (app->phase == PHASE_EVAPORATION) ? ratio * 0.45 : ratio * 0.78;
                    cairo_set_source_rgba(cr, 0.20, 0.68, 1.0, op);
                    cairo_set_line_width(cr, 1.0 + ratio * 5.5);
                    cairo_move_to(cr, TO_SX(app->graphe.sommets[i].x), TO_SY(app->graphe.sommets[i].y));
                    cairo_line_to(cr, TO_SX(app->graphe.sommets[j].x), TO_SY(app->graphe.sommets[j].y));
                    cairo_stroke(cr);
                }
            }
        }
    }

    // 2. Cycle Force Brute (vert menthe)
    if (app->afficher_fb && app->meilleur_fb.chemin) {
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_set_source_rgba(cr, 0.19, 0.82, 0.35, 0.80);
        cairo_set_line_width(cr, 4.5);
        for (int i = 0; i < n; i++) {
            int u = app->meilleur_fb.chemin[i];
            int v = app->meilleur_fb.chemin[(i + 1) % n];
            cairo_move_to(cr, TO_SX(app->graphe.sommets[u].x), TO_SY(app->graphe.sommets[u].y));
            cairo_line_to(cr, TO_SX(app->graphe.sommets[v].x), TO_SY(app->graphe.sommets[v].y));
        }
        cairo_stroke(cr);
    }

    // 2bis. Meilleur cycle de la vague en cours (orange, pendant les phases 1 à 3)
    if (app->phase != PHASE_PRETE && app->meilleur_iter.chemin) {
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_source_rgba(cr, 1.0, 0.62, 0.04, 0.90);
        cairo_set_line_width(cr, 2.2);
        double dashes[] = {5.0, 4.0};
        cairo_set_dash(cr, dashes, 2, 0);
        for (int i = 0; i < n; i++) {
            int u = app->meilleur_iter.chemin[i];
            int v = app->meilleur_iter.chemin[(i + 1) % n];
            cairo_move_to(cr, TO_SX(app->graphe.sommets[u].x), TO_SY(app->graphe.sommets[u].y));
            cairo_line_to(cr, TO_SX(app->graphe.sommets[v].x), TO_SY(app->graphe.sommets[v].y));
        }
        cairo_stroke(cr);
        cairo_set_dash(cr, NULL, 0, 0);
    }

    // 3. Meilleur cycle global ACO (rouge corail avec halo)
    if (app->iteration > 0 && app->meilleur_global.chemin) {
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

        cairo_set_source_rgba(cr, 1.0, 0.27, 0.23, 0.22);
        cairo_set_line_width(cr, 6.5);
        for (int i = 0; i < n; i++) {
            int u = app->meilleur_global.chemin[i];
            int v = app->meilleur_global.chemin[(i + 1) % n];
            cairo_move_to(cr, TO_SX(app->graphe.sommets[u].x), TO_SY(app->graphe.sommets[u].y));
            cairo_line_to(cr, TO_SX(app->graphe.sommets[v].x), TO_SY(app->graphe.sommets[v].y));
        }
        cairo_stroke(cr);

        cairo_set_source_rgb(cr, 1.0, 0.27, 0.23);
        cairo_set_line_width(cr, 2.3);
        for (int i = 0; i < n; i++) {
            int u = app->meilleur_global.chemin[i];
            int v = app->meilleur_global.chemin[(i + 1) % n];
            cairo_move_to(cr, TO_SX(app->graphe.sommets[u].x), TO_SY(app->graphe.sommets[u].y));
            cairo_line_to(cr, TO_SX(app->graphe.sommets[v].x), TO_SY(app->graphe.sommets[v].y));
        }
        cairo_stroke(cr);
    }

    // 4. Sommets stylisés
    double r_ext = (n > 50) ? 3.5 : 5.5;
    double r_int = (n > 50) ? 2.0 : 3.0;
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, (n > 50) ? 8.5 : 11.0);

    for (int i = 0; i < n; i++) {
        double sx = TO_SX(app->graphe.sommets[i].x);
        double sy = TO_SY(app->graphe.sommets[i].y);

        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.45);
        cairo_arc(cr, sx, sy + 1.2, r_ext + 1.0, 0, 2 * M_PI);
        cairo_fill(cr);

        cairo_set_source_rgb(cr, 0.96, 0.96, 0.97);
        cairo_arc(cr, sx, sy, r_ext, 0, 2 * M_PI);
        cairo_fill(cr);

        if (i == 0) cairo_set_source_rgb(cr, 1.0, 0.84, 0.04);
        else cairo_set_source_rgb(cr, 0.04, 0.52, 1.0);
        cairo_arc(cr, sx, sy, r_int, 0, 2 * M_PI);
        cairo_fill(cr);

        if (app->afficher_noms) {
            cairo_set_source_rgba(cr, 0.92, 0.92, 0.95, 0.88);
            cairo_move_to(cr, sx + r_ext + 3.0, sy - 3.0);
            cairo_show_text(cr, app->graphe.sommets[i].nom);
        }
    }

    // 5. Pilule de légende flottante
    double hud_w = (app->afficher_fb && app->meilleur_fb.chemin) ? 560.0 : 425.0;
    cairo_rounded_rect(cr, 14.0, 12.0, hud_w, 28.0, 14.0);
    cairo_set_source_rgba(cr, 0.16, 0.16, 0.19, 0.88);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 1.0, 0.27, 0.23);
    cairo_arc(cr, 28.0, 26.0, 4.0, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.92, 0.92, 0.95);
    cairo_move_to(cr, 37.0, 30.0);
    cairo_show_text(cr, "Record ACO");

    cairo_set_source_rgb(cr, 1.0, 0.62, 0.04);
    cairo_arc(cr, 122.0, 26.0, 4.0, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.92, 0.92, 0.95);
    cairo_move_to(cr, 131.0, 30.0);
    cairo_show_text(cr, "Meilleur vague");

    cairo_set_source_rgb(cr, 0.20, 0.68, 1.0);
    cairo_arc(cr, 235.0, 26.0, 4.0, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.92, 0.92, 0.95);
    cairo_move_to(cr, 244.0, 30.0);
    cairo_show_text(cr, "Phéromones");

    cairo_set_source_rgba(cr, 0.85, 0.85, 0.92, 0.65);
    cairo_arc(cr, 335.0, 26.0, 4.0, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.92, 0.92, 0.95);
    cairo_move_to(cr, 344.0, 30.0);
    cairo_show_text(cr, "Fourmis");

    if (app->afficher_fb && app->meilleur_fb.chemin) {
        cairo_set_source_rgb(cr, 0.19, 0.82, 0.35);
        cairo_arc(cr, 412.0, 26.0, 4.0, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 0.92, 0.92, 0.95);
        cairo_move_to(cr, 421.0, 30.0);
        cairo_show_text(cr, "Optimal Force Brute");
    }

    #undef TO_SX
    #undef TO_SY
    return FALSE;
}

// ============================================================================
// RENDU GRAPHIQUE CAIRO : COURBE DE CONVERGENCE
// ============================================================================
static gboolean on_draw_chart(GtkWidget* widget, cairo_t* cr, gpointer data) {
    AppState* app = (AppState*)data;
    double w = gtk_widget_get_allocated_width(widget);
    double h = gtk_widget_get_allocated_height(widget);

    cairo_rounded_rect(cr, 0.5, 0.5, w - 1.0, h - 1.0, 14.0);
    cairo_set_source_rgb(cr, 0.11, 0.11, 0.13);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.19, 0.19, 0.22);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke_preserve(cr);
    cairo_clip(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.60, 0.60, 0.63);
    cairo_move_to(cr, 16, 22);
    cairo_show_text(cr, "CONVERGENCE DU POIDS  ·  ROUGE = RECORD GLOBAL  |  ORANGE = MOYENNE COLONIE");

    if (app->iteration < 2) {
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 12.0);
        cairo_set_source_rgb(cr, 0.48, 0.48, 0.52);
        cairo_move_to(cr, 16, h / 2.0 + 6);
        cairo_show_text(cr, "Exécutez au moins 2 itérations pour tracer la courbe de convergence.");
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
    double left = 56.0, right = w - 18.0, top = 36.0, bottom = h - 20.0;
    double cw = right - left;
    double ch = bottom - top;

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.06);
    cairo_set_line_width(cr, 1.0);
    for (int g = 0; g <= 2; g++) {
        double gy = top + (g / 2.0) * ch;
        cairo_move_to(cr, left, gy);
        cairo_line_to(cr, right, gy);
    }
    cairo_stroke(cr);

    char buf[32];
    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.55, 0.55, 0.58);
    snprintf(buf, sizeof(buf), "%.1f", maxV);
    cairo_move_to(cr, 10, top + 4);
    cairo_show_text(cr, buf);

    snprintf(buf, sizeof(buf), "%.1f", minV);
    cairo_move_to(cr, 10, bottom);
    cairo_show_text(cr, buf);

    if (app->graphe.optimal_ref > 0.0) {
        double yOpt = bottom - ((app->graphe.optimal_ref - minV) / span) * ch;
        double dashes[] = {4.0, 4.0};
        cairo_set_dash(cr, dashes, 2, 0);
        cairo_set_source_rgb(cr, 0.19, 0.82, 0.35);
        cairo_set_line_width(cr, 1.5);
        cairo_move_to(cr, left, yOpt);
        cairo_line_to(cr, right, yOpt);
        cairo_stroke(cr);
        cairo_set_dash(cr, NULL, 0, 0);
    }

    cairo_pattern_t* grad = cairo_pattern_create_linear(0, top, 0, bottom);
    cairo_pattern_add_color_stop_rgba(grad, 0.0, 1.0, 0.27, 0.23, 0.28);
    cairo_pattern_add_color_stop_rgba(grad, 1.0, 1.0, 0.27, 0.23, 0.0);

    cairo_move_to(cr, left, bottom);
    for (int i = 0; i < app->iteration; i++) {
        double x = left + ((double)i / (app->iteration - 1)) * cw;
        double y = bottom - ((app->hist_meilleur[i] - minV) / span) * ch;
        cairo_line_to(cr, x, y);
    }
    cairo_line_to(cr, right, bottom);
    cairo_close_path(cr);
    cairo_set_source(cr, grad);
    cairo_fill(cr);
    cairo_pattern_destroy(grad);

    cairo_set_source_rgba(cr, 1.0, 0.62, 0.04, 0.65);
    cairo_set_line_width(cr, 1.5);
    for (int i = 0; i < app->iteration; i++) {
        double x = left + ((double)i / (app->iteration - 1)) * cw;
        double y = bottom - ((app->hist_moyenne[i] - minV) / span) * ch;
        if (i == 0) cairo_move_to(cr, x, y);
        else cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 1.0, 0.27, 0.23);
    cairo_set_line_width(cr, 2.4);
    for (int i = 0; i < app->iteration; i++) {
        double x = left + ((double)i / (app->iteration - 1)) * cw;
        double y = bottom - ((app->hist_meilleur[i] - minV) / span) * ch;
        if (i == 0) cairo_move_to(cr, x, y);
        else cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);

    return FALSE;
}

// ============================================================================
// CALLBACKS & MISES À JOUR UI
// ============================================================================
static void mettre_a_jour_diagnostic_params(AppState* app) {
    double a = app->alpha;
    double b = app->beta;
    const char* msg;

    if (a == 0.0 && b == 0.0) {
        msg = "α = 0, β = 0 : Marche purement aléatoire (ni mémoire, ni distance).";
    } else if (a == 0.0) {
        msg = "α = 0 : Phéromones ignorées → comportement glouton stochastique.";
    } else if (b == 0.0) {
        msg = "β = 0 : Distances ignorées → convergence aveugle sur les pistes.";
    } else if (a > b * 1.5) {
        msg = "α dominant : Forte amplification des phéromones → convergence rapide.";
    } else if (b > a * 1.5) {
        msg = "β dominant : Attirance forte vers les villes proches → bon démarrage.";
    } else {
        msg = "Équilibre α / β : Compromis optimal entre mémoire et proximité.";
    }
    gtk_label_set_text(GTK_LABEL(app->lbl_diag), msg);
}

static void mettre_a_jour_panneau_etape(AppState* app) {
    char buf[512];
    switch (app->phase) {
        case PHASE_PRETE:
            if (app->iteration == 0) {
                snprintf(buf, sizeof(buf),
                    "<b>Phase 0 · Initialisation</b>\n"
                    "<span foreground='#98989D'>Matrice τ[i][j] = 1.0 sur toutes les arêtes.\n"
                    "Cliquez sur <b>Sous-étape</b> pour lancer les %d fourmis.</span>",
                    app->nb_fourmis);
            } else {
                snprintf(buf, sizeof(buf),
                    "<b>Phase 4/4 · Itération %d terminée</b>\n"
                    "<span foreground='#30D158'>Record global : %.2f</span>  |  "
                    "<span foreground='#FF9F0A'>Moy. colonie : %.2f</span>\n"
                    "<span foreground='#98989D'>Prêt pour la vague suivante (Phase 1).</span>",
                    app->iteration,
                    app->meilleur_global.poids,
                    app->hist_moyenne[app->iteration - 1]);
            }
            break;

        case PHASE_EXPLORATION:
            snprintf(buf, sizeof(buf),
                "<b>Phase 1/4 · Exploration (%d fourmis)</b>\n"
                "<span foreground='#64D2FF'>Formule : p = τ^%.1f × (1/d)^%.1f</span>\n"
                "Meilleur de la vague : <span foreground='#FF9F0A'><b>%.2f</b></span> (moy. %.2f)\n"
                "<span foreground='#98989D'>→ Prochaine sous-étape : Évaporation (1 − ρ).</span>",
                app->nb_vague, app->alpha, app->beta,
                app->meilleur_iter.poids,
                app->somme_poids_vague / app->nb_vague);
            break;

        case PHASE_EVAPORATION:
            snprintf(buf, sizeof(buf),
                "<b>Phase 2/4 · Évaporation des pistes</b>\n"
                "<span foreground='#64D2FF'>Formule : τ ← (1 − %.2f) × τ = %.2f × τ</span>\n"
                "Les anciennes pistes s'estompent pour éviter les minima locaux.\n"
                "<span foreground='#98989D'>→ Prochaine sous-étape : Dépôt Q / L_k.</span>",
                app->rho, 1.0 - app->rho);
            break;

        case PHASE_DEPOT:
            snprintf(buf, sizeof(buf),
                "<b>Phase 3/4 · Dépôt de phéromones</b>\n"
                "<span foreground='#64D2FF'>Formule : τ ← τ + (%.0f / L_k)</span>\n"
                "Dépôt max sur le meilleur cycle : <b>+%.3f</b>\n"
                "<span foreground='#98989D'>→ Prochaine sous-étape : Bilan & Courbe.</span>",
                app->Q, app->max_depot_vague);
            break;
    }
    gtk_label_set_markup(GTK_LABEL(app->lbl_etape_algo), buf);
}

static void rafraichir_interface(AppState* app) {
    char buf_iter[256], buf_aco[256], buf_opt[256];

    snprintf(buf_iter, sizeof(buf_iter),
             "<span size='small' foreground='#98989D'>ITÉRATION</span>\n"
             "<span size='large' weight='bold'>%d</span> <span size='small' foreground='#98989D'>(%d villes)</span>",
             app->iteration, app->graphe.n);
    gtk_label_set_markup(GTK_LABEL(app->lbl_kpi_iter), buf_iter);

    if (app->iteration > 0) {
        snprintf(buf_aco, sizeof(buf_aco),
                 "<span size='small' foreground='#98989D'>MEILLEUR ACO</span>\n"
                 "<span size='large' weight='bold' foreground='#FF453A'>%.2f</span>",
                 app->meilleur_global.poids);
    } else {
        snprintf(buf_aco, sizeof(buf_aco),
                 "<span size='small' foreground='#98989D'>MEILLEUR ACO</span>\n"
                 "<span size='large' weight='bold' foreground='#636366'>—</span>");
    }
    gtk_label_set_markup(GTK_LABEL(app->lbl_kpi_aco), buf_aco);

    double ref = (app->graphe.optimal_ref > 0.0) ? app->graphe.optimal_ref
               : (app->meilleur_fb.chemin ? app->meilleur_fb.poids : -1.0);

    if (ref > 0.0) {
        if (app->iteration > 0) {
            double ecart = ((app->meilleur_global.poids - ref) / ref) * 100.0;
            snprintf(buf_opt, sizeof(buf_opt),
                     "<span size='small' foreground='#98989D'>OPTIMAL / ÉCART</span>\n"
                     "<span size='large' weight='bold' foreground='#30D158'>%.2f</span> "
                     "<span size='small' foreground='#F5F5F7'>(+%.1f%%)</span>",
                     ref, ecart);
        } else {
            snprintf(buf_opt, sizeof(buf_opt),
                     "<span size='small' foreground='#98989D'>OPTIMAL RÉF.</span>\n"
                     "<span size='large' weight='bold' foreground='#30D158'>%.2f</span>",
                     ref);
        }
    } else {
        snprintf(buf_opt, sizeof(buf_opt),
                 "<span size='small' foreground='#98989D'>OPTIMAL RÉF.</span>\n"
                 "<span size='large' weight='bold' foreground='#636366'>N/A</span>");
    }
    gtk_label_set_markup(GTK_LABEL(app->lbl_kpi_opt), buf_opt);

    mettre_a_jour_panneau_etape(app);
    gtk_widget_queue_draw(app->drawing_graph);
    gtk_widget_queue_draw(app->drawing_chart);
}

static void on_substep_clicked(GtkButton* btn, gpointer data) {
    (void)btn;
    AppState* app = (AppState*)data;
    executer_une_sous_etape(app);
    rafraichir_interface(app);
}

static void on_step_clicked(GtkButton* btn, gpointer data) {
    (void)btn;
    AppState* app = (AppState*)data;
    executer_une_iteration_complete(app);
    rafraichir_interface(app);
}

static gboolean on_timer_tick(gpointer data) {
    AppState* app = (AppState*)data;
    executer_une_iteration_complete(app);
    rafraichir_interface(app);
    return G_SOURCE_CONTINUE;
}

static void on_play_clicked(GtkButton* btn, gpointer data) {
    AppState* app = (AppState*)data;
    if (app->timer_id > 0) {
        g_source_remove(app->timer_id);
        app->timer_id = 0;
        gtk_button_set_label(btn, "▶  Lancer");
    } else {
        app->timer_id = g_timeout_add(app->vitesse_ms, on_timer_tick, app);
        gtk_button_set_label(btn, "⏸  Pause");
    }
}

static void on_reset_clicked(GtkButton* btn, gpointer data) {
    (void)btn;
    AppState* app = (AppState*)data;
    if (app->timer_id > 0) {
        g_source_remove(app->timer_id);
        app->timer_id = 0;
        gtk_button_set_label(GTK_BUTTON(app->btn_play), "▶  Lancer");
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
        gtk_button_set_label(GTK_BUTTON(app->btn_play), "▶  Lancer");
    }
    int n = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->spin_alea));
    generer_graphe_aleatoire(app, n);
    rafraichir_interface(app);
}

static void on_charger_clicked(GtkButton* btn, gpointer data) {
    (void)btn;
    AppState* app = (AppState*)data;
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        "Ouvrir un fichier graphe (Ligne 1 = Optimal, puis x y nom ou nom x y)",
        NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
        "Annuler", GTK_RESPONSE_CANCEL,
        "Ouvrir", GTK_RESPONSE_ACCEPT, NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (app->timer_id > 0) {
            g_source_remove(app->timer_id);
            app->timer_id = 0;
            gtk_button_set_label(GTK_BUTTON(app->btn_play), "▶  Lancer");
        }
        if (!charger_graphe_fichier(app, filename)) {
            GtkWidget* err = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK, "Format invalide.\nAttendu : Ligne 1 = poids minimal, puis 'x y nom' ou 'nom x y'.");
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
            GTK_BUTTONS_OK, "Force brute limitée à 12 sommets maximum (12! = 479M permutations).");
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
    gtk_button_set_label(GTK_BUTTON(tb), app->afficher_phero ? "● Phéromones" : "○ Phéromones");
    gtk_widget_queue_draw(app->drawing_graph);
}

static void on_toggle_noms(GtkToggleButton* tb, gpointer data) {
    AppState* app = (AppState*)data;
    app->afficher_noms = gtk_toggle_button_get_active(tb);
    gtk_button_set_label(GTK_BUTTON(tb), app->afficher_noms ? "● Noms" : "○ Noms");
    gtk_widget_queue_draw(app->drawing_graph);
}

static void on_toggle_colonie(GtkToggleButton* tb, gpointer data) {
    AppState* app = (AppState*)data;
    app->afficher_colonie = gtk_toggle_button_get_active(tb);
    gtk_button_set_label(GTK_BUTTON(tb), app->afficher_colonie ? "● Fourmis" : "○ Fourmis");
    gtk_widget_queue_draw(app->drawing_graph);
}

static GtkWidget* creer_carte(const char* titre, GtkWidget** contenu_box) {
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "card");

    GtkWidget* lbl = gtk_label_new(titre);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "section-title");
    gtk_box_pack_start(GTK_BOX(card), lbl, FALSE, FALSE, 0);

    *contenu_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);
    gtk_box_pack_start(GTK_BOX(card), *contenu_box, FALSE, FALSE, 0);
    return card;
}

// ============================================================================
// MAIN
// ============================================================================
int main(int argc, char** argv) {
    srand((unsigned int)time(NULL));
    gtk_init(&argc, &argv);
    appliquer_style_macos();

    AppState app = {0};
    app.alpha = 1.0;
    app.beta = 2.0;
    app.rho = 0.35;
    app.Q = 100.0;
    app.nb_fourmis = 20;
    app.vitesse_ms = 110;
    app.afficher_phero = true;
    app.afficher_noms = true;
    app.afficher_colonie = true;

    generer_graphe_aleatoire(&app, 10);

    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "TSP · Colonies de Fourmis (ACO)");
    gtk_window_set_default_size(GTK_WINDOW(window), 1260, 820);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_container_set_border_width(GTK_CONTAINER(root), 14);
    gtk_container_add(GTK_CONTAINER(window), root);

    // --- COLONNE GAUCHE : GRAPHE + COURBE ---
    GtkWidget* left_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_pack_start(GTK_BOX(root), left_col, TRUE, TRUE, 0);

    app.drawing_graph = gtk_drawing_area_new();
    gtk_widget_set_size_request(app.drawing_graph, 680, 500);
    g_signal_connect(app.drawing_graph, "draw", G_CALLBACK(on_draw_graph), &app);

    app.drawing_chart = gtk_drawing_area_new();
    gtk_widget_set_size_request(app.drawing_chart, 680, 195);
    g_signal_connect(app.drawing_chart, "draw", G_CALLBACK(on_draw_chart), &app);

    gtk_box_pack_start(GTK_BOX(left_col), app.drawing_graph, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(left_col), app.drawing_chart, FALSE, FALSE, 0);

    // --- COLONNE DROITE : PANNEAU DE CONTRÔLE ---
    GtkWidget* right_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_size_request(right_col, 390, -1);
    gtk_box_pack_start(GTK_BOX(root), right_col, FALSE, FALSE, 0);

    // 0. Cartes KPIs
    GtkWidget* kpi_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    app.lbl_kpi_iter = gtk_label_new("");
    app.lbl_kpi_aco = gtk_label_new("");
    app.lbl_kpi_opt = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(app.lbl_kpi_iter), 0.0);
    gtk_label_set_xalign(GTK_LABEL(app.lbl_kpi_aco), 0.0);
    gtk_label_set_xalign(GTK_LABEL(app.lbl_kpi_opt), 0.0);

    gtk_style_context_add_class(gtk_widget_get_style_context(app.lbl_kpi_iter), "kpi-box");
    gtk_style_context_add_class(gtk_widget_get_style_context(app.lbl_kpi_aco), "kpi-box");
    gtk_style_context_add_class(gtk_widget_get_style_context(app.lbl_kpi_opt), "kpi-box");

    gtk_box_pack_start(GTK_BOX(kpi_row), app.lbl_kpi_iter, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(kpi_row), app.lbl_kpi_aco, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(kpi_row), app.lbl_kpi_opt, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(right_col), kpi_row, FALSE, FALSE, 0);

    // 1. Carte Exécution Pas-à-Pas & Déroulement en direct
    GtkWidget* box_exec;
    GtkWidget* card_exec = creer_carte("EXÉCUTION PAS-À-PAS & DÉROULEMENT", &box_exec);

    GtkWidget* row_btns1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* btn_substep = gtk_button_new_with_label("▸ Sous-étape");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_substep), "step-phase");
    GtkWidget* btn_step = gtk_button_new_with_label("⏭ 1 Itération");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_step), "primary");
    app.btn_play = gtk_button_new_with_label("▶ Lancer");
    GtkWidget* btn_reset = gtk_button_new_with_label("↺");

    g_signal_connect(btn_substep, "clicked", G_CALLBACK(on_substep_clicked), &app);
    g_signal_connect(btn_step, "clicked", G_CALLBACK(on_step_clicked), &app);
    g_signal_connect(app.btn_play, "clicked", G_CALLBACK(on_play_clicked), &app);
    g_signal_connect(btn_reset, "clicked", G_CALLBACK(on_reset_clicked), &app);

    gtk_box_pack_start(GTK_BOX(row_btns1), btn_substep, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row_btns1), btn_step, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row_btns1), app.btn_play, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row_btns1), btn_reset, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box_exec), row_btns1, FALSE, FALSE, 0);

    // Encart pédagogique affichant la phase courante et les calculs
    app.lbl_etape_algo = gtk_label_new("");
    gtk_label_set_line_wrap(GTK_LABEL(app.lbl_etape_algo), TRUE);
    gtk_label_set_xalign(GTK_LABEL(app.lbl_etape_algo), 0.0);
    gtk_widget_set_size_request(app.lbl_etape_algo, 340, 68);
    gtk_style_context_add_class(gtk_widget_get_style_context(app.lbl_etape_algo), "algo-box");
    gtk_box_pack_start(GTK_BOX(box_exec), app.lbl_etape_algo, FALSE, FALSE, 0);

    GtkWidget* row_speed = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    app.spin_vitesse = gtk_spin_button_new_with_range(10, 2000, 10);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app.spin_vitesse), app.vitesse_ms);
    g_signal_connect(app.spin_vitesse, "value-changed", G_CALLBACK(on_param_changed), &app);
    GtkWidget* lbl_speed = gtk_label_new("Délai animation (ms) :");
    gtk_label_set_xalign(GTK_LABEL(lbl_speed), 0.0);
    gtk_box_pack_start(GTK_BOX(row_speed), lbl_speed, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row_speed), app.spin_vitesse, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box_exec), row_speed, FALSE, FALSE, 0);

    GtkWidget* btn_brute = gtk_button_new_with_label("⚡  Comparer avec Force Brute (n ≤ 12)");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_brute), "accent-green");
    g_signal_connect(btn_brute, "clicked", G_CALLBACK(on_brute_clicked), &app);
    gtk_box_pack_start(GTK_BOX(box_exec), btn_brute, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(right_col), card_exec, FALSE, FALSE, 0);

    // 2. Carte Paramètres Alpha / Beta
    GtkWidget* box_params;
    GtkWidget* card_params = creer_carte("INFLUENCE DES PARAMÈTRES (α & β)", &box_params);

    GtkWidget* grid_p = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid_p), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid_p), 10);

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

    const char* labels_p[] = {
        "α  (Poids des phéromones)",
        "β  (Attirance distance 1/d)",
        "ρ  (Taux d'évaporation)",
        "Nombre de fourmis"
    };
    GtkWidget* spins_p[] = { app.spin_alpha, app.spin_beta, app.spin_rho, app.spin_fourmis };

    for (int i = 0; i < 4; i++) {
        GtkWidget* l = gtk_label_new(labels_p[i]);
        gtk_label_set_xalign(GTK_LABEL(l), 0.0);
        gtk_widget_set_hexpand(l, TRUE);
        gtk_grid_attach(GTK_GRID(grid_p), l, 0, i, 1, 1);
        gtk_grid_attach(GTK_GRID(grid_p), spins_p[i], 1, i, 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(box_params), grid_p, FALSE, FALSE, 0);

    app.lbl_diag = gtk_label_new("");
    gtk_label_set_line_wrap(GTK_LABEL(app.lbl_diag), TRUE);
    gtk_label_set_xalign(GTK_LABEL(app.lbl_diag), 0.0);
    gtk_widget_set_size_request(app.lbl_diag, 340, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(app.lbl_diag), "diag-pill");
    gtk_box_pack_start(GTK_BOX(box_params), app.lbl_diag, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(right_col), card_params, FALSE, FALSE, 0);

    // 3. Carte Données du Graphe & Calques
    GtkWidget* box_graph;
    GtkWidget* card_graph = creer_carte("GRAPHE & CALQUES", &box_graph);

    GtkWidget* btn_charger = gtk_button_new_with_label("Importer un fichier (.txt : Opt + x y nom)");
    g_signal_connect(btn_charger, "clicked", G_CALLBACK(on_charger_clicked), &app);
    gtk_box_pack_start(GTK_BOX(box_graph), btn_charger, FALSE, FALSE, 0);

    GtkWidget* row_alea = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    app.spin_alea = gtk_spin_button_new_with_range(3, 150, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app.spin_alea), 10);
    GtkWidget* btn_alea = gtk_button_new_with_label("Générer aléatoire");
    g_signal_connect(btn_alea, "clicked", G_CALLBACK(on_alea_clicked), &app);
    gtk_box_pack_start(GTK_BOX(row_alea), app.spin_alea, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row_alea), btn_alea, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box_graph), row_alea, FALSE, FALSE, 0);

    GtkWidget* row_pills = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* pill_phero = gtk_toggle_button_new_with_label("● Phéromones");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pill_phero), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(pill_phero), "pill-toggle");
    g_signal_connect(pill_phero, "toggled", G_CALLBACK(on_toggle_phero), &app);

    GtkWidget* pill_colonie = gtk_toggle_button_new_with_label("● Fourmis");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pill_colonie), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(pill_colonie), "pill-toggle");
    g_signal_connect(pill_colonie, "toggled", G_CALLBACK(on_toggle_colonie), &app);

    GtkWidget* pill_noms = gtk_toggle_button_new_with_label("● Noms");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pill_noms), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(pill_noms), "pill-toggle");
    g_signal_connect(pill_noms, "toggled", G_CALLBACK(on_toggle_noms), &app);

    gtk_box_pack_start(GTK_BOX(row_pills), pill_phero, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row_pills), pill_colonie, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row_pills), pill_noms, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box_graph), row_pills, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(right_col), card_graph, FALSE, FALSE, 0);

    mettre_a_jour_diagnostic_params(&app);
    rafraichir_interface(&app);

    gtk_widget_show_all(window);
    gtk_main();

    liberer_simulation(&app);
    free(app.graphe.sommets);
    free(app.meilleur_fb.chemin);
    return 0;
}