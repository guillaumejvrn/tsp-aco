#include <gtk/gtk.h>
#include <cairo.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <float.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BF_MAX 12

// ============================================================================
// PRNG DÉTERMINISTE (MULBERRY32)
// ============================================================================
static uint32_t rng_next_u32(uint32_t* state) {
    uint32_t s = (*state += 0x6D2B79F5u);
    uint32_t t = (s ^ (s >> 15)) * (1u | s);
    t = (t + ((t ^ (t >> 7)) * 61u)) ^ t;
    return t ^ (t >> 14);
}

static double rng_next_double(uint32_t* state) {
    return (double)rng_next_u32(state) / 4294967296.0;
}

// ============================================================================
// STRUCTURES DE DONNÉES
// ============================================================================
typedef struct {
    int n;
    double* x;
    double* y;
    int* ids; // Numéro de sommet affiché (celui du fichier, sinon 0..n-1)
    double* D;
    double x0, x1, y0, y1;
    bool has_ref;
    double ref;
} Graphe;

typedef struct {
    int start;
    int cur;
    uint8_t* vis;
    int* path;
    int path_len;
    double w;
    double* pr;
    double sum;
    bool has_last;
    int last_from;
    int last_to;
    double last_tir;
    double last_sum;
    double last_p;
    bool done;
} Fourmi;

typedef struct {
    double cost;
    int* path;
    int iter;
} MeilleurTour;

typedef struct {
    double cost;
    int* path;
    double ms;
    uint64_t count;
} ResultatFB;

typedef struct {
    Graphe G;
    double* tau;
    int iter;
    MeilleurTour best;
    double* hist_iter_best;
    double* hist_global_best;
    int hist_len;
    int hist_cap;
    double ms;
    uint32_t rnd_state;
    int runs;
    int k;
    double ib;
    Fourmi* A;
    char log_msg[256];
    char last_det_sig[256];

    ResultatFB* BF;
    bool running;
    guint timer_id;
    double acc;
    double last_frame_ms;

    // Widgets Colonne Gauche
    GtkWidget* spin_n;
    GtkWidget* spin_seed;
    GtkWidget* lbl_imp;
    GtkWidget* spin_m;
    GtkWidget* spin_it;
    GtkWidget* spin_Q;
    GtkWidget* spin_a;
    GtkWidget* spin_b;
    GtkWidget* spin_rho;
    GtkWidget* combo_mode;
    GtkWidget* scale_spd;
    GtkWidget* lbl_spdv;
    GtkWidget* btn_run;
    GtkWidget* chk_ph;
    GtkWidget* chk_best;
    GtkWidget* chk_bf;
    GtkWidget* chk_num;

    // Widgets Colonne Centrale
    GtkWidget* cv_graph;
    GtkWidget* det_box;

    // Widgets Colonne Droite
    GtkWidget* lbl_iter;
    GtkWidget* lbl_bi;
    GtkWidget* lbl_bc;
    GtkWidget* lbl_tm;
    GtkWidget* lbl_fi;
    GtkWidget* lbl_fc;
    GtkWidget* lbl_ft;
    GtkWidget* lbl_rc;
    GtkWidget* lbl_gap;
    GtkWidget* btn_bf;
    GtkWidget* lbl_bfn;
    GtkWidget* cv_chart;
} AppState;

// ============================================================================
// FEUILLE DE STYLE CSS (THÈME SAUGE / FORÊT, 100 % SANS SVG)
// ============================================================================
static const char* APP_CSS =
    "* { -gtk-icon-source: none; }"
    "window {"
    "  background-color: #E9EFEC;"
    "  color: #14302E;"
    "  font-family: 'Bricolage Grotesque', -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;"
    "  font-size: 13px;"
    "}"
    "scrolledwindow, viewport {"
    "  background-color: transparent;"
    "  border: none;"
    "}"
    ".header-title {"
    "  font-size: 22px;"
    "  font-weight: 800;"
    "  color: #14302E;"
    "}"
    ".header-sub {"
    "  font-size: 13px;"
    "  color: #5B7270;"
    "}"
    ".box {"
    "  background-color: #F7FAF8;"
    "  border: 1px solid #C5D2CE;"
    "  border-radius: 10px;"
    "  padding: 11px;"
    "}"
    ".box-title {"
    "  font-size: 14px;"
    "  font-weight: 700;"
    "  color: #14302E;"
    "  margin-bottom: 2px;"
    "}"
    ".muted-small {"
    "  font-size: 11px;"
    "  color: #5B7270;"
    "}"
    ".note-text {"
    "  font-size: 11.5px;"
    "  color: #5B7270;"
    "}"
    "spinbutton, combobox button {"
    "  background-image: none;"
    "  background-color: #E9EFEC;"
    "  color: #14302E;"
    "  border: 1px solid #C5D2CE;"
    "  border-radius: 6px;"
    "  padding: 2px 5px;"
    "  box-shadow: none;"
    "}"
    "spinbutton entry {"
    "  background: transparent;"
    "  color: #14302E;"
    "  border: none;"
    "}"
    "spinbutton button {"
    "  background-image: none;"
    "  background-color: #DCE5E1;"
    "  color: #14302E;"
    "  border: none;"
    "  border-radius: 4px;"
    "  padding: 1px 4px;"
    "  margin: 1px;"
    "}"
    "button.primary {"
    "  background-image: none;"
    "  background-color: #14302E;"
    "  color: #F7FAF8;"
    "  border: 1px solid #14302E;"
    "  border-radius: 7px;"
    "  padding: 6px 8px;"
    "  font-weight: 600;"
    "  box-shadow: none;"
    "}"
    "button.primary:hover {"
    "  background-color: #1E4542;"
    "}"
    "button.alt {"
    "  background-image: none;"
    "  background-color: transparent;"
    "  color: #14302E;"
    "  border: 1px solid #C5D2CE;"
    "  border-radius: 7px;"
    "  padding: 6px 8px;"
    "  font-weight: 600;"
    "  box-shadow: none;"
    "}"
    "button.alt:hover {"
    "  background-color: #E9EFEC;"
    "}"
    "button:disabled {"
    "  opacity: 0.45;"
    "}"
    "checkbutton {"
    "  color: #14302E;"
    "}"
    "checkbutton check {"
    "  background-image: none;"
    "  background-color: #E9EFEC;"
    "  border: 1px solid #8DA39E;"
    "  border-radius: 4px;"
    "  min-width: 14px;"
    "  min-height: 14px;"
    "}"
    "checkbutton check:checked {"
    "  background-color: #14302E;"
    "  border-color: #14302E;"
    "  color: #F7FAF8;"
    "}"
    "scale trough {"
    "  background-color: #C5D2CE;"
    "  border-radius: 4px;"
    "  min-height: 5px;"
    "}"
    "scale highlight {"
    "  background-color: #B0176F;"
    "  border-radius: 4px;"
    "}"
    "scale slider {"
    "  background-image: none;"
    "  background-color: #B0176F;"
    "  border: 2px solid #F7FAF8;"
    "  border-radius: 8px;"
    "  min-width: 13px;"
    "  min-height: 13px;"
    "}"
    ".metric-row {"
    "  border-top: 1px solid #C5D2CE;"
    "  padding-top: 5px;"
    "  margin-top: 3px;"
    "}"
    ".metric-val {"
    "  font-size: 17px;"
    "  font-weight: 800;"
    "  color: #14302E;"
    "}"
    ".hl-best {"
    "  border-left: 4px solid #C77F00;"
    "  padding-left: 8px;"
    "}"
    ".hl-bf {"
    "  border-left: 4px solid #1F6FB5;"
    "  padding-left: 8px;"
    "}"
    ".det-sep {"
    "  border-top: 1px solid #C5D2CE;"
    "  padding-top: 8px;"
    "  margin-top: 4px;"
    "}"
    ".tbl-head {"
    "  font-size: 11.5px;"
    "  font-weight: 700;"
    "  color: #5B7270;"
    "  border-bottom: 1px solid #C5D2CE;"
    "  padding-bottom: 2px;"
    "}"
    ".tbl-cell {"
    "  font-size: 12px;"
    "  color: #14302E;"
    "  border-bottom: 1px solid #DCE5E1;"
    "  padding: 1px 0;"
    "}";

static void appliquer_css(void) {
    GtkCssProvider* prov = gtk_css_provider_new();
    gtk_css_provider_load_from_data(prov, APP_CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(prov),
        GTK_STYLE_PROVIDER_PRIORITY_USER
    );
    g_object_unref(prov);
}

// ============================================================================
// UTILITAIRES DE FORMATAGE & TEMPS
// ============================================================================
static inline int lb(const AppState* app, int v) {
    return app->G.ids ? app->G.ids[v] : v;
}

static double now_ms(void) {
    return g_get_monotonic_time() / 1000.0;
}

static void fmt_num(double v, char* buf, size_t sz) {
    if (fabs(v) < 10.0) snprintf(buf, sz, "%.3f", v);
    else snprintf(buf, sz, "%.1f", v);
    for (char* p = buf; *p; p++) if (*p == '.') *p = ',';
}

static void fmt_ms(double v, char* buf, size_t sz) {
    if (v < 1000.0) {
        if (v < 10.0) snprintf(buf, sz, "%.1f ms", v);
        else snprintf(buf, sz, "%.0f ms", v);
    } else {
        snprintf(buf, sz, "%.2f s", v / 1000.0);
    }
    for (char* p = buf; *p; p++) if (*p == '.') *p = ',';
}

static uint64_t fact_u64(int k) {
    uint64_t r = 1;
    for (int i = 2; i <= k; i++) r *= (uint64_t)i;
    return r;
}

// ============================================================================
// GESTION DE LA FOURMI & DE L'ÉTAT ACO
// ============================================================================
static void free_fourmi(Fourmi* A) {
    if (!A) return;
    free(A->vis);
    free(A->path);
    free(A->pr);
    free(A);
}

static void free_bf(AppState* app) {
    if (app->BF) {
        free(app->BF->path);
        free(app->BF);
        app->BF = NULL;
    }
}

static void reset_aco(AppState* app);

static void set_graph(AppState* app, int n, double* x, double* y, int* ids,
                      double x0, double x1, double y0, double y1,
                      bool has_ref, double ref) {
    free(app->G.x);
    free(app->G.y);
    free(app->G.ids);
    free(app->G.D);

    if (!ids) {
        ids = malloc(n * sizeof(int));
        for (int i = 0; i < n; i++) ids[i] = i;
    }

    app->G.n = n;
    app->G.x = x;
    app->G.y = y;
    app->G.ids = ids;
    app->G.x0 = x0;
    app->G.x1 = x1;
    app->G.y0 = y0;
    app->G.y1 = y1;
    app->G.has_ref = has_ref;
    app->G.ref = ref;

    app->G.D = malloc((size_t)n * n * sizeof(double));
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            app->G.D[i * n + j] = hypot(x[i] - x[j], y[i] - y[j]);
        }
    }

    free_bf(app);
    reset_aco(app);
}

static void reset_aco(AppState* app) {
    if (app->timer_id > 0) {
        g_source_remove(app->timer_id);
        app->timer_id = 0;
    }
    app->running = false;
    app->acc = 0.0;

    int n = app->G.n;
    double Q = gtk_spin_button_get_value(GTK_SPIN_BUTTON(app->spin_Q));
    uint32_t seed = (uint32_t)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->spin_seed));

    free(app->tau);
    app->tau = malloc((size_t)n * n * sizeof(double));
    for (int i = 0; i < n * n; i++) app->tau[i] = Q;

    free(app->best.path);
    app->best.path = NULL;
    app->best.cost = INFINITY;
    app->best.iter = 0;

    free(app->hist_iter_best);
    free(app->hist_global_best);
    app->hist_cap = 256;
    app->hist_len = 0;
    app->hist_iter_best = malloc(app->hist_cap * sizeof(double));
    app->hist_global_best = malloc(app->hist_cap * sizeof(double));

    free_fourmi(app->A);
    app->A = NULL;

    app->iter = 0;
    app->ms = 0.0;
    app->runs++;
    app->rnd_state = seed * 7919u + (uint32_t)app->runs;
    app->k = 0;
    app->ib = INFINITY;
    app->log_msg[0] = '\0';
    app->last_det_sig[0] = '\0';
}

static void new_random_graph(AppState* app) {
    int n = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->spin_n));
    uint32_t seed = (uint32_t)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->spin_seed));
    uint32_t r = seed;

    double* x = malloc(n * sizeof(double));
    double* y = malloc(n * sizeof(double));

    for (int i = 0; i < n; i++) {
        int a, b;
        bool dup;
        do {
            a = (int)floor(rng_next_double(&r) * 1000.0);
            b = (int)floor(rng_next_double(&r) * 1000.0);
            dup = false;
            for (int j = 0; j < i; j++) {
                if ((int)x[j] == a && (int)y[j] == b) { dup = true; break; }
            }
        } while (dup);
        x[i] = (double)a;
        y[i] = (double)b;
    }

    gtk_label_set_text(GTK_LABEL(app->lbl_imp),
        "Ligne 0 : poids connu, puis « numéro x y » (numérotation libre, ex. à partir de 1).");
    set_graph(app, n, x, y, NULL, 0.0, 1000.0, 0.0, 1000.0, false, 0.0);
}

// ============================================================================
// IMPORT FICHIER .TXT (CONSERVE LES NUMÉROS DE SOMMETS D'ORIGINE)
// ============================================================================
typedef struct {
    int id;
    double x;
    double y;
} RawPt;

static int cmp_raw_pt(const void* a, const void* b) {
    return ((const RawPt*)a)->id - ((const RawPt*)b)->id;
}

static bool parse_txt_file(const char* filename, int* out_n, double** out_x, double** out_y,
                           int** out_ids, bool* out_has_ref, double* out_ref,
                           char* err_msg, size_t err_sz) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        snprintf(err_msg, err_sz, "impossible d'ouvrir le fichier");
        return false;
    }

    int cap = 64, count = 0;
    RawPt* pts = malloc(cap * sizeof(RawPt));
    bool has_ref = false;
    double ref = 0.0;
    char line[512];
    int line_no = 0;

    while (fgets(line, sizeof(line), f)) {
        line_no++;
        char* hash = strchr(line, '#');
        if (hash) *hash = '\0';

        for (char* p = line; *p; p++) {
            if (*p == ',' || *p == ';') *p = ' ';
        }

        char tok[4][64];
        int nt = sscanf(line, "%63s %63s %63s %63s", tok[0], tok[1], tok[2], tok[3]);
        if (nt <= 0) continue;

        double vals[3];
        for (int i = 0; i < nt && i < 3; i++) {
            char* endp;
            vals[i] = strtod(tok[i], &endp);
            if (*endp != '\0' || !isfinite(vals[i])) {
                snprintf(err_msg, err_sz, "ligne %d illisible", line_no);
                free(pts);
                fclose(f);
                return false;
            }
        }

        if (nt == 1 && count == 0 && !has_ref) {
            has_ref = true;
            ref = vals[0];
            continue;
        }
        if (nt != 2 && nt != 3) {
            snprintf(err_msg, err_sz, "ligne %d : 2 ou 3 nombres attendus", line_no);
            free(pts);
            fclose(f);
            return false;
        }

        int id = (nt == 3) ? (int)vals[0] : count;
        double px = vals[nt - 2];
        double py = vals[nt - 1];

        for (int i = 0; i < count; i++) {
            if (pts[i].x == px && pts[i].y == py) {
                snprintf(err_msg, err_sz, "ligne %d : point en double", line_no);
                free(pts);
                fclose(f);
                return false;
            }
        }

        if (count >= cap) {
            cap *= 2;
            pts = realloc(pts, cap * sizeof(RawPt));
        }
        pts[count].id = id;
        pts[count].x = px;
        pts[count].y = py;
        count++;
    }
    fclose(f);

    if (count < 3 || count > 200) {
        snprintf(err_msg, err_sz, "%d sommets trouvés (3 à 200 requis)", count);
        free(pts);
        return false;
    }

    qsort(pts, count, sizeof(RawPt), cmp_raw_pt);

    for (int i = 1; i < count; i++) {
        if (pts[i].id == pts[i - 1].id) {
            snprintf(err_msg, err_sz, "le numéro de sommet %d apparaît deux fois", pts[i].id);
            free(pts);
            return false;
        }
    }

    double* x = malloc(count * sizeof(double));
    double* y = malloc(count * sizeof(double));
    int* ids = malloc(count * sizeof(int));
    for (int i = 0; i < count; i++) {
        x[i] = pts[i].x;
        y[i] = pts[i].y;
        ids[i] = pts[i].id;
    }
    free(pts);

    *out_n = count;
    *out_x = x;
    *out_y = y;
    *out_ids = ids;
    *out_has_ref = has_ref;
    *out_ref = ref;
    return true;
}

// ============================================================================
// LOGIQUE ACO
// ============================================================================
static Fourmi* ant_new(AppState* app) {
    int n = app->G.n;
    int s = (int)floor(rng_next_double(&app->rnd_state) * n);
    if (s >= n) s = n - 1;

    Fourmi* A = calloc(1, sizeof(Fourmi));
    A->start = s;
    A->cur = s;
    A->vis = calloc(n, sizeof(uint8_t));
    A->vis[s] = 1;
    A->path = malloc((n + 1) * sizeof(int));
    A->path[0] = s;
    A->path_len = 1;
    A->pr = calloc(n, sizeof(double));
    return A;
}

static void ant_probs(AppState* app, Fourmi* A) {
    int n = app->G.n;
    double a = gtk_spin_button_get_value(GTK_SPIN_BUTTON(app->spin_a));
    double b = gtk_spin_button_get_value(GTK_SPIN_BUTTON(app->spin_b));
    A->sum = 0.0;

    for (int i = 0; i < n; i++) {
        A->pr[i] = 0.0;
        if (A->vis[i]) continue;
        double d = app->G.D[A->cur * n + i];
        double tv = app->tau[A->cur * n + i];
        if (d > 0.0 && tv > 0.0) {
            double pa = (a == 1.0) ? tv : pow(tv, a);
            double pb = (b == 1.0) ? (1.0 / d) : pow(1.0 / d, b);
            A->pr[i] = pa * pb;
            A->sum += A->pr[i];
        }
    }
}

static void ant_move(AppState* app, Fourmi* A) {
    int n = app->G.n;
    double Q = gtk_spin_button_get_value(GTK_SPIN_BUTTON(app->spin_Q));
    int mode = gtk_combo_box_get_active(GTK_COMBO_BOX(app->combo_mode));

    double tir = rng_next_double(&app->rnd_state) * A->sum;
    double cum = 0.0;
    int nx = -1;

    for (int i = 0; i < n; i++) {
        cum += A->pr[i];
        if (!A->vis[i] && A->pr[i] > 0.0 && tir <= cum) {
            nx = i;
            break;
        }
    }
    if (nx < 0) {
        for (int i = 0; i < n; i++) {
            if (!A->vis[i]) { nx = i; break; }
        }
    }

    A->has_last = true;
    A->last_from = A->cur;
    A->last_to = nx;
    A->last_tir = tir;
    A->last_sum = A->sum;
    A->last_p = (A->sum > 0.0) ? (A->pr[nx] / A->sum) : 0.0;

    A->w += app->G.D[A->cur * n + nx];
    if (mode == 0) {
        app->tau[A->cur * n + nx] += Q;
        app->tau[nx * n + A->cur] += Q;
    }

    A->path[A->path_len++] = nx;
    A->vis[nx] = 1;
    A->cur = nx;

    if (A->path_len == n) {
        A->w += app->G.D[A->cur * n + A->start];
        if (mode == 0) {
            app->tau[A->cur * n + A->start] += Q;
            app->tau[A->start * n + A->cur] += Q;
        } else {
            double dep = Q / A->w;
            for (int i = 0; i < n; i++) {
                int u = A->path[i];
                int v = A->path[(i + 1) % n];
                app->tau[u * n + v] += dep;
                app->tau[v * n + u] += dep;
            }
        }
        A->done = true;
    }
}

static void end_ant(AppState* app, Fourmi* A) {
    int n = app->G.n;
    int m = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->spin_m));
    double rho = gtk_spin_button_get_value(GTK_SPIN_BUTTON(app->spin_rho));

    if (A->w < app->ib) app->ib = A->w;
    if (A->w < app->best.cost) {
        app->best.cost = A->w;
        app->best.iter = app->iter + 1;
        free(app->best.path);
        app->best.path = malloc(n * sizeof(int));
        memcpy(app->best.path, A->path, n * sizeof(int));
    }

    app->k++;
    char b_w[32];
    fmt_num(A->w, b_w, sizeof(b_w));
    snprintf(app->log_msg, sizeof(app->log_msg),
             "Fourmi %d enregistrée : tour de longueur %s.", app->k, b_w);

    if (app->k >= m) {
        double f = 1.0 - rho;
        for (int i = 0; i < n * n; i++) app->tau[i] *= f;

        if (app->hist_len >= app->hist_cap) {
            app->hist_cap *= 2;
            app->hist_iter_best = realloc(app->hist_iter_best, app->hist_cap * sizeof(double));
            app->hist_global_best = realloc(app->hist_global_best, app->hist_cap * sizeof(double));
        }
        app->hist_iter_best[app->hist_len] = app->ib;
        app->hist_global_best[app->hist_len] = app->best.cost;
        app->hist_len++;

        app->iter++;
        app->k = 0;
        app->ib = INFINITY;

        char b_f[32];
        fmt_num(f, b_f, sizeof(b_f));
        size_t len = strlen(app->log_msg);
        snprintf(app->log_msg + len, sizeof(app->log_msg) - len,
                 " Fin itération %d : évaporation × %s.",
                 app->iter, b_f);
    }
}

static void micro_step(AppState* app) {
    if (!app->A) {
        app->A = ant_new(app);
        ant_probs(app, app->A);
        app->log_msg[0] = '\0';
        return;
    }
    if (app->A->done) {
        end_ant(app, app->A);
        free_fourmi(app->A);
        app->A = NULL;
        return;
    }
    ant_move(app, app->A);
    if (!app->A->done) {
        ant_probs(app, app->A);
    }
}

static void full_iteration_step(AppState* app) {
    double t0 = now_ms();
    int target_it = app->iter;
    do {
        Fourmi* A = app->A ? app->A : ant_new(app);
        app->A = NULL;
        while (!A->done) {
            ant_probs(app, A);
            ant_move(app, A);
        }
        end_ant(app, A);
        free_fourmi(A);
    } while (app->iter == target_it);
    app->ms += (now_ms() - t0);
}

// ============================================================================
// FORCE BRUTE
// ============================================================================
static void bf_rec(const Graphe* G, int* cur, int d, double w,
                   double* best, int* best_path, uint64_t* count) {
    int n = G->n;
    if (d == n) {
        (*count)++;
        double tot = w + G->D[cur[n - 1] * n + cur[0]];
        if (tot < *best) {
            *best = tot;
            memcpy(best_path, cur, n * sizeof(int));
        }
        return;
    }
    for (int i = d; i < n; i++) {
        int tmp = cur[d]; cur[d] = cur[i]; cur[i] = tmp;
        bf_rec(G, cur, d + 1, w + G->D[cur[d - 1] * n + cur[d]], best, best_path, count);
        tmp = cur[d]; cur[d] = cur[i]; cur[i] = tmp;
    }
}

static void run_brute_force(AppState* app) {
    int n = app->G.n;
    if (n > BF_MAX) return;
    double t0 = now_ms();
    free_bf(app);

    ResultatFB* res = calloc(1, sizeof(ResultatFB));
    res->cost = INFINITY;
    res->path = malloc(n * sizeof(int));

    int* cur = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) cur[i] = i;

    bf_rec(&app->G, cur, 1, 0.0, &res->cost, res->path, &res->count);
    free(cur);
    res->ms = now_ms() - t0;
    app->BF = res;
}

// ============================================================================
// RENDU CAIRO : CANEVAS PRINCIPAL DU GRAPHE
// ============================================================================
static gboolean on_draw_graph(GtkWidget* widget, cairo_t* cr, gpointer data) {
    AppState* app = (AppState*)data;
    double W = gtk_widget_get_allocated_width(widget);
    double H = gtk_widget_get_allocated_height(widget);

    cairo_set_source_rgb(cr, 0.914, 0.937, 0.925);
    cairo_rectangle(cr, 0, 0, W, H);
    cairo_fill(cr);

    int n = app->G.n;
    if (n < 3 || W < 40 || H < 40) return FALSE;

    double pad = 22.0;
    double s = ((W < H) ? W : H) - 2.0 * pad;
    if (s < 10.0) s = 10.0;
    double rx = app->G.x1 - app->G.x0;
    double ry = app->G.y1 - app->G.y0;
    double max_span = (rx > ry) ? rx : ry;
    if (max_span < 1e-9) max_span = 1.0;
    double k = s / max_span;
    double ox = (W - rx * k) / 2.0;
    double oy = (H - ry * k) / 2.0;

    cairo_set_source_rgb(cr, 0.773, 0.824, 0.808);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, ox - 8.0, oy - 8.0, rx * k + 16.0, ry * k + 16.0);
    cairo_stroke(cr);

    #define PTX(v) (ox + (app->G.x[v] - app->G.x0) * k)
    #define PTY(v) (oy + (app->G.y1 - app->G.y[v]) * k)

    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    // 1. Phéromones (#B0176F)
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->chk_ph))) {
        double mx = 0.0, mn = INFINITY;
        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                double v = app->tau[i * n + j];
                if (v > mx) mx = v;
                if (v < mn) mn = v;
            }
        }
        double rg = mx - mn;
        if (rg > 1e-9 * mx) {
            for (int i = 0; i < n; i++) {
                for (int j = i + 1; j < n; j++) {
                    double r = (app->tau[i * n + j] - mn) / rg;
                    if (r < 0.03) continue;
                    double alpha = 0.1 + 0.75 * r * r;
                    if (alpha > 1.0) alpha = 1.0;
                    cairo_set_source_rgba(cr, 0.690, 0.090, 0.435, alpha);
                    cairo_set_line_width(cr, 0.6 + ((n > 60) ? 3.0 : 6.0) * r * r);
                    cairo_move_to(cr, PTX(i), PTY(i));
                    cairo_line_to(cr, PTX(j), PTY(j));
                    cairo_stroke(cr);
                }
            }
        }
    }

    // 2. Meilleur tour ACO (#C77F00)
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->chk_best)) && app->best.path) {
        cairo_set_source_rgb(cr, 0.780, 0.498, 0.0);
        cairo_set_line_width(cr, 3.0);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        for (int i = 0; i < n; i++) {
            int v = app->best.path[i];
            if (i == 0) cairo_move_to(cr, PTX(v), PTY(v));
            else cairo_line_to(cr, PTX(v), PTY(v));
        }
        cairo_close_path(cr);
        cairo_stroke(cr);
    }

    // 3. Tour optimal Force Brute (#1F6FB5 en pointillés)
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->chk_bf)) && app->BF && app->BF->path) {
        double dashes[] = {9.0, 6.0};
        cairo_set_dash(cr, dashes, 2, 0.0);
        cairo_set_source_rgb(cr, 0.122, 0.435, 0.710);
        cairo_set_line_width(cr, 2.3);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        for (int i = 0; i < n; i++) {
            int v = app->BF->path[i];
            if (i == 0) cairo_move_to(cr, PTX(v), PTY(v));
            else cairo_line_to(cr, PTX(v), PTY(v));
        }
        cairo_close_path(cr);
        cairo_stroke(cr);
        cairo_set_dash(cr, NULL, 0, 0.0);
    }

    // 4. Fourmi en cours (#14302E)
    if (app->A) {
        Fourmi* A = app->A;
        if (!A->done && A->sum > 0.0) {
            double dashes[] = {3.0, 5.0};
            cairo_set_dash(cr, dashes, 2, 0.0);
            cairo_set_source_rgba(cr, 0.078, 0.188, 0.180, 0.75);
            for (int i = 0; i < n; i++) {
                if (A->pr[i] > 0.0) {
                    cairo_set_line_width(cr, 1.0 + 9.0 * (A->pr[i] / A->sum));
                    cairo_move_to(cr, PTX(A->cur), PTY(A->cur));
                    cairo_line_to(cr, PTX(i), PTY(i));
                    cairo_stroke(cr);
                }
            }
            cairo_set_dash(cr, NULL, 0, 0.0);
        }

        cairo_set_source_rgb(cr, 0.078, 0.188, 0.180);
        cairo_set_line_width(cr, 2.8);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        for (int i = 0; i < A->path_len; i++) {
            int v = A->path[i];
            if (i == 0) cairo_move_to(cr, PTX(v), PTY(v));
            else cairo_line_to(cr, PTX(v), PTY(v));
        }
        if (A->done) cairo_close_path(cr);
        cairo_stroke(cr);

        cairo_set_line_width(cr, 2.4);
        cairo_arc(cr, PTX(A->cur), PTY(A->cur), 10.0, 0, 2.0 * M_PI);
        cairo_stroke(cr);
    }

    // 5. Sommets & numéros d'origine (lb(v))
    double r = (n > 60) ? 2.5 : 4.8;
    bool show_num = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->chk_num)) && (n <= 30);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);

    for (int v = 0; v < n; v++) {
        double a = PTX(v), b = PTY(v);
        cairo_set_source_rgb(cr, 0.969, 0.980, 0.973);
        cairo_arc(cr, a, b, r, 0, 2.0 * M_PI);
        cairo_fill_preserve(cr);

        cairo_set_source_rgb(cr, 0.078, 0.188, 0.180);
        cairo_set_line_width(cr, 1.5);
        cairo_stroke(cr);

        if (show_num) {
            char nb[16];
            snprintf(nb, sizeof(nb), "%d", lb(app, v));
            cairo_text_extents_t ext;
            cairo_text_extents(cr, nb, &ext);
            cairo_move_to(cr, a - ext.width / 2.0, b - r - 4.0);
            cairo_show_text(cr, nb);
        }
    }

    #undef PTX
    #undef PTY
    return FALSE;
}

// ============================================================================
// RENDU CAIRO : COURBE DE CONVERGENCE
// ============================================================================
static gboolean on_draw_chart(GtkWidget* widget, cairo_t* cr, gpointer data) {
    AppState* app = (AppState*)data;
    double W = gtk_widget_get_allocated_width(widget);
    double H = gtk_widget_get_allocated_height(widget);

    cairo_set_source_rgb(cr, 0.969, 0.980, 0.973);
    cairo_rectangle(cr, 0, 0, W, H);
    cairo_fill(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11.0);

    if (app->hist_len == 0) {
        cairo_set_source_rgb(cr, 0.357, 0.447, 0.439);
        const char* txt = "Lancez l’ACO pour voir la courbe";
        cairo_text_extents_t ext;
        cairo_text_extents(cr, txt, &ext);
        cairo_move_to(cr, (W - ext.width) / 2.0, H / 2.0);
        cairo_show_text(cr, txt);
        return FALSE;
    }

    double pl = 42.0, pr = 8.0, pt = 8.0, pb = 18.0;
    double hi = app->hist_iter_best[0];
    double lo = app->best.cost;
    for (int i = 0; i < app->hist_len; i++) {
        if (app->hist_iter_best[i] > hi) hi = app->hist_iter_best[i];
        if (app->hist_global_best[i] < lo) lo = app->hist_global_best[i];
    }
    if (app->BF && app->BF->cost < lo) lo = app->BF->cost;
    if (app->G.has_ref && app->G.ref < lo) lo = app->G.ref;

    if (hi - lo <= 0.0) hi = lo + fmax(1e-9, fabs(lo) * 0.01);
    double e = (hi - lo) * 0.06;
    hi += e;
    lo -= e;

    int it_max = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->spin_it));
    int N = (it_max > 2) ? it_max : 2;
    if (app->hist_len > N) N = app->hist_len;

    #define CX(i) (pl + (W - pl - pr) * (double)(i) / (double)(N - 1))
    #define CY(v) (pt + (H - pt - pb) * (1.0 - ((v) - lo) / (hi - lo)))

    cairo_set_source_rgb(cr, 0.773, 0.824, 0.808);
    cairo_set_line_width(cr, 1.0);
    for (int k = 0; k < 3; k++) {
        double v = lo + (hi - lo) * (double)k / 2.0;
        double yy = CY(v);
        cairo_move_to(cr, pl, yy);
        cairo_line_to(cr, W - pr, yy);
        cairo_stroke(cr);

        char b[24];
        snprintf(b, sizeof(b), "%.3g", v);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, b, &ext);
        cairo_set_source_rgb(cr, 0.357, 0.447, 0.439);
        cairo_move_to(cr, pl - 4.0 - ext.width, yy + 4.0);
        cairo_show_text(cr, b);
        cairo_set_source_rgb(cr, 0.773, 0.824, 0.808);
    }

    cairo_set_source_rgb(cr, 0.357, 0.447, 0.439);
    cairo_move_to(cr, (pl + W - pr) / 2.0 - 20.0, H - 3.0);
    cairo_show_text(cr, "itérations");

    cairo_set_source_rgba(cr, 0.357, 0.447, 0.439, 0.5);
    cairo_set_line_width(cr, 1.0);
    for (int j = 0; j < app->hist_len; j++) {
        if (j == 0) cairo_move_to(cr, CX(j), CY(app->hist_iter_best[j]));
        else cairo_line_to(cr, CX(j), CY(app->hist_iter_best[j]));
    }
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 0.780, 0.498, 0.0);
    cairo_set_line_width(cr, 2.3);
    for (int j = 0; j < app->hist_len; j++) {
        if (j == 0) cairo_move_to(cr, CX(j), CY(app->hist_global_best[j]));
        else cairo_line_to(cr, CX(j), CY(app->hist_global_best[j]));
    }
    cairo_stroke(cr);

    if (app->G.has_ref) {
        double d1[] = {2.0, 4.0};
        cairo_set_dash(cr, d1, 2, 0.0);
        cairo_set_source_rgb(cr, 0.078, 0.188, 0.180);
        cairo_set_line_width(cr, 2.0);
        cairo_move_to(cr, pl, CY(app->G.ref));
        cairo_line_to(cr, W - pr, CY(app->G.ref));
        cairo_stroke(cr);
        cairo_set_dash(cr, NULL, 0, 0.0);
    }

    if (app->BF) {
        double d2[] = {7.0, 5.0};
        cairo_set_dash(cr, d2, 2, 0.0);
        cairo_set_source_rgb(cr, 0.122, 0.435, 0.710);
        cairo_set_line_width(cr, 2.0);
        cairo_move_to(cr, pl, CY(app->BF->cost));
        cairo_line_to(cr, W - pr, CY(app->BF->cost));
        cairo_stroke(cr);
        cairo_set_dash(cr, NULL, 0, 0.0);
    }

    #undef CX
    #undef CY
    return FALSE;
}

// ============================================================================
// TABLEAU DÉTAILLÉ DES PROBABILITÉS (AVEC NUMÉROS D'ORIGINE lb(v))
// ============================================================================
typedef struct {
    int city;
    double pr;
} CandEntry;

static int cmp_cand_desc(const void* a, const void* b) {
    double diff = ((const CandEntry*)b)->pr - ((const CandEntry*)a)->pr;
    return (diff > 0) ? 1 : (diff < 0 ? -1 : 0);
}

static void draw_detail_box(AppState* app) {
    char sig[512];
    if (!app->A) {
        snprintf(sig, sizeof(sig), "IDLE|%s", app->log_msg);
    } else {
        snprintf(sig, sizeof(sig), "ANT|%d|%d|%d|%d",
                 app->iter, app->k, app->A->path_len, app->A->done ? 1 : 0);
    }

    if (strcmp(sig, app->last_det_sig) == 0) return;
    strcpy(app->last_det_sig, sig);

    GList* children = gtk_container_get_children(GTK_CONTAINER(app->det_box));
    for (GList* l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    int m = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->spin_m));
    int n = app->G.n;

    if (!app->A) {
        GtkWidget* title = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(title), "<b>Détail d’une itération</b>");
        gtk_label_set_xalign(GTK_LABEL(title), 0.0);
        gtk_box_pack_start(GTK_BOX(app->det_box), title, FALSE, FALSE, 0);

        const char* txt = (app->log_msg[0] != '\0')
            ? app->log_msg
            : "« Pas de fourmi » avance d’une décision : villes candidates, probabilités, tirage et dépôt.";
        GtkWidget* p = gtk_label_new(txt);
        gtk_label_set_line_wrap(GTK_LABEL(p), TRUE);
        gtk_label_set_xalign(GTK_LABEL(p), 0.0);
        gtk_style_context_add_class(gtk_widget_get_style_context(p), "note-text");
        gtk_box_pack_start(GTK_BOX(app->det_box), p, FALSE, FALSE, 0);
        gtk_widget_show_all(app->det_box);
        return;
    }

    Fourmi* A = app->A;
    char head_buf[256];
    snprintf(head_buf, sizeof(head_buf),
             "<b>Itér. %d · Fourmi %d/%d</b> — Ville <b>%d</b> (%d/%d)",
             app->iter + 1, app->k + 1, m, lb(app, A->cur), A->path_len, n);
    GtkWidget* lbl_head = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_head), head_buf);
    gtk_label_set_xalign(GTK_LABEL(lbl_head), 0.0);
    gtk_box_pack_start(GTK_BOX(app->det_box), lbl_head, FALSE, FALSE, 0);

    // Chaîne du tour avec numéros d'origine
    GString* tour_str = g_string_new("Tour : ");
    for (int i = 0; i < A->path_len; i++) {
        if (i > 0) g_string_append(tour_str, " → ");
        g_string_append_printf(tour_str, "%d", lb(app, A->path[i]));
    }
    GtkWidget* lbl_tour = gtk_label_new(tour_str->str);
    g_string_free(tour_str, TRUE);
    gtk_label_set_line_wrap(GTK_LABEL(lbl_tour), TRUE);
    gtk_label_set_xalign(GTK_LABEL(lbl_tour), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl_tour), "note-text");
    gtk_box_pack_start(GTK_BOX(app->det_box), lbl_tour, FALSE, FALSE, 0);

    if (A->has_last) {
        char btir[32], bsum[32], last_buf[256];
        fmt_num(A->last_tir, btir, sizeof(btir));
        fmt_num(A->last_sum, bsum, sizeof(bsum));
        snprintf(last_buf, sizeof(last_buf),
                 "Choix : tirage %s / %s → ville <b>%d</b> (%.1f %%)",
                 btir, bsum, lb(app, A->last_to), A->last_p * 100.0);
        GtkWidget* lbl_last = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(lbl_last), last_buf);
        gtk_label_set_line_wrap(GTK_LABEL(lbl_last), TRUE);
        gtk_label_set_xalign(GTK_LABEL(lbl_last), 0.0);
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl_last), "note-text");
        gtk_box_pack_start(GTK_BOX(app->det_box), lbl_last, FALSE, FALSE, 0);
    }

    if (A->done) {
        char bw[32], done_buf[160];
        fmt_num(A->w, bw, sizeof(bw));
        snprintf(done_buf, sizeof(done_buf),
                 "Tour terminé, longueur <b>%s</b>. Cliquez sur « Pas de fourmi » pour enregistrer.", bw);
        GtkWidget* lbl_done = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(lbl_done), done_buf);
        gtk_label_set_line_wrap(GTK_LABEL(lbl_done), TRUE);
        gtk_label_set_xalign(GTK_LABEL(lbl_done), 0.0);
        gtk_box_pack_start(GTK_BOX(app->det_box), lbl_done, FALSE, FALSE, 0);
        gtk_widget_show_all(app->det_box);
        return;
    }

    CandEntry* cands = malloc(n * sizeof(CandEntry));
    int nc = 0;
    for (int i = 0; i < n; i++) {
        if (!A->vis[i] && A->pr[i] > 0.0) {
            cands[nc].city = i;
            cands[nc].pr = A->pr[i];
            nc++;
        }
    }
    qsort(cands, nc, sizeof(CandEntry), cmp_cand_desc);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 2);

    const char* headers[] = { "Cand.", "Dist.", "τ", "Score", "Proba" };
    for (int col = 0; col < 5; col++) {
        GtkWidget* h = gtk_label_new(headers[col]);
        gtk_label_set_xalign(GTK_LABEL(h), col == 0 ? 0.0 : 1.0);
        gtk_widget_set_hexpand(h, TRUE);
        gtk_style_context_add_class(gtk_widget_get_style_context(h), "tbl-head");
        gtk_grid_attach(GTK_GRID(grid), h, col, 0, 1, 1);
    }

    int show_rows = (nc > 5) ? 5 : nc;
    for (int r = 0; r < show_rows; r++) {
        int c = cands[r].city;
        double dist = app->G.D[A->cur * n + c];
        double tv = app->tau[A->cur * n + c];
        double score = A->pr[c];
        double prob = (A->sum > 0.0) ? (score / A->sum) : 0.0;

        char s_c[16], s_d[24], s_t[24], s_s[24], s_p[32];
        snprintf(s_c, sizeof(s_c), "%d", lb(app, c));
        fmt_num(dist, s_d, sizeof(s_d));
        fmt_num(tv, s_t, sizeof(s_t));
        snprintf(s_s, sizeof(s_s), "%.3g", score);
        snprintf(s_p, sizeof(s_p), "%.1f %%", prob * 100.0);

        const char* vals[] = { s_c, s_d, s_t, s_s, s_p };
        for (int col = 0; col < 5; col++) {
            GtkWidget* cell = gtk_label_new(vals[col]);
            gtk_label_set_xalign(GTK_LABEL(cell), col == 0 ? 0.0 : 1.0);
            gtk_style_context_add_class(gtk_widget_get_style_context(cell), "tbl-cell");
            gtk_grid_attach(GTK_GRID(grid), cell, col, r + 1, 1, 1);
        }
    }
    gtk_box_pack_start(GTK_BOX(app->det_box), grid, FALSE, FALSE, 0);

    if (nc > 5) {
        char more[64];
        snprintf(more, sizeof(more), "… + %d autres candidates", nc - 5);
        GtkWidget* lm = gtk_label_new(more);
        gtk_label_set_xalign(GTK_LABEL(lm), 0.0);
        gtk_style_context_add_class(gtk_widget_get_style_context(lm), "note-text");
        gtk_box_pack_start(GTK_BOX(app->det_box), lm, FALSE, FALSE, 0);
    }

    free(cands);
    gtk_widget_show_all(app->det_box);
}

// ============================================================================
// RAFRAÎCHISSEMENT DE L'INTERFACE
// ============================================================================
static void update_run_btn(AppState* app) {
    int it_max = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->spin_it));
    if (app->running) {
        gtk_button_set_label(GTK_BUTTON(app->btn_run), "Pause");
    } else if (app->iter > 0 && app->iter < it_max) {
        gtk_button_set_label(GTK_BUTTON(app->btn_run), "Reprendre");
    } else {
        gtk_button_set_label(GTK_BUTTON(app->btn_run), "Lancer l’ACO");
    }
}

static void refresh_ui(AppState* app) {
    int it_max = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->spin_it));
    char buf[128];

    snprintf(buf, sizeof(buf), "%d / %d", app->iter, it_max);
    gtk_label_set_text(GTK_LABEL(app->lbl_iter), buf);

    if (app->best.path) {
        fmt_num(app->best.cost, buf, sizeof(buf));
        gtk_label_set_text(GTK_LABEL(app->lbl_bc), buf);
        snprintf(buf, sizeof(buf), "itér. %d", app->best.iter);
        gtk_label_set_text(GTK_LABEL(app->lbl_bi), buf);
    } else {
        gtk_label_set_text(GTK_LABEL(app->lbl_bc), "—");
        gtk_label_set_text(GTK_LABEL(app->lbl_bi), "");
    }

    if (app->iter > 0) {
        fmt_ms(app->ms, buf, sizeof(buf));
        gtk_label_set_text(GTK_LABEL(app->lbl_tm), buf);
    } else {
        gtk_label_set_text(GTK_LABEL(app->lbl_tm), "—");
    }

    if (app->BF) {
        fmt_num(app->BF->cost, buf, sizeof(buf));
        gtk_label_set_text(GTK_LABEL(app->lbl_fc), buf);
        fmt_ms(app->BF->ms, buf, sizeof(buf));
        gtk_label_set_text(GTK_LABEL(app->lbl_ft), buf);
        snprintf(buf, sizeof(buf), "%llu tours", (unsigned long long)app->BF->count);
        gtk_label_set_text(GTK_LABEL(app->lbl_fi), buf);
    } else {
        gtk_label_set_text(GTK_LABEL(app->lbl_fc), "—");
        gtk_label_set_text(GTK_LABEL(app->lbl_ft), "—");
        gtk_label_set_text(GTK_LABEL(app->lbl_fi), "");
    }

    if (app->G.has_ref) {
        fmt_num(app->G.ref, buf, sizeof(buf));
        gtk_label_set_text(GTK_LABEL(app->lbl_rc), buf);
    } else {
        gtk_label_set_text(GTK_LABEL(app->lbl_rc), "—");
    }

    bool has_target = app->G.has_ref || (app->BF != NULL);
    double ref_val = app->G.has_ref ? app->G.ref : (app->BF ? app->BF->cost : 0.0);
    if (has_target && app->best.path && ref_val > 0.0) {
        double dg = ((app->best.cost - ref_val) / ref_val) * 100.0;
        if (fabs(dg) < 1e-4) snprintf(buf, sizeof(buf), "0 %%");
        else snprintf(buf, sizeof(buf), "%s%.2f %%", dg > 0 ? "+" : "", dg);
        gtk_label_set_text(GTK_LABEL(app->lbl_gap), buf);
    } else {
        gtk_label_set_text(GTK_LABEL(app->lbl_gap), "—");
    }

    bool can_bf = (app->G.n <= BF_MAX);
    gtk_widget_set_sensitive(app->btn_bf, can_bf);
    if (!can_bf) {
        snprintf(buf, sizeof(buf), "Désactivé au-delà de %d sommets.", BF_MAX);
        gtk_label_set_text(GTK_LABEL(app->lbl_bfn), buf);
    } else if (!app->BF) {
        snprintf(buf, sizeof(buf), "Teste (%d−1)! = %llu tours.",
                 app->G.n, (unsigned long long)fact_u64(app->G.n - 1));
        gtk_label_set_text(GTK_LABEL(app->lbl_bfn), buf);
    } else {
        gtk_label_set_text(GTK_LABEL(app->lbl_bfn), "");
    }

    update_run_btn(app);
    gtk_widget_queue_draw(app->cv_graph);
    gtk_widget_queue_draw(app->cv_chart);
    draw_detail_box(app);
}

// ============================================================================
// CALLBACKS ÉVÉNEMENTS
// ============================================================================
static gboolean on_anim_tick(gpointer data) {
    AppState* app = (AppState*)data;
    if (!app->running) {
        app->timer_id = 0;
        return G_SOURCE_REMOVE;
    }

    double ts = now_ms();
    double dt = ts - app->last_frame_ms;
    if (dt > 100.0) dt = 100.0;
    app->last_frame_ms = ts;

    double spd = gtk_range_get_value(GTK_RANGE(app->scale_spd));
    app->acc += (dt / 1000.0) * spd;
    if (app->acc > 6.0) app->acc = 6.0;

    int it_max = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->spin_it));
    double t0 = now_ms();
    while (app->acc >= 1.0 && app->iter < it_max && (now_ms() - t0) < 12.0) {
        full_iteration_step(app);
        app->acc -= 1.0;
    }

    if (app->iter >= it_max) {
        app->running = false;
        app->timer_id = 0;
        refresh_ui(app);
        return G_SOURCE_REMOVE;
    }

    refresh_ui(app);
    return G_SOURCE_CONTINUE;
}

static void on_run_clicked(GtkButton* b, gpointer data) {
    (void)b;
    AppState* app = (AppState*)data;
    int it_max = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->spin_it));

    if (app->running) {
        app->running = false;
        if (app->timer_id > 0) { g_source_remove(app->timer_id); app->timer_id = 0; }
        refresh_ui(app);
        return;
    }
    if (app->iter >= it_max) {
        reset_aco(app);
    }
    app->running = true;
    app->acc = 1.0;
    app->last_frame_ms = now_ms();
    app->timer_id = g_timeout_add(16, on_anim_tick, app);
    refresh_ui(app);
}

static void on_one_clicked(GtkButton* b, gpointer data) {
    (void)b;
    AppState* app = (AppState*)data;
    int it_max = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->spin_it));
    if (app->running) {
        app->running = false;
        if (app->timer_id > 0) { g_source_remove(app->timer_id); app->timer_id = 0; }
    }
    if (app->iter >= it_max) reset_aco(app);
    full_iteration_step(app);
    refresh_ui(app);
}

static void on_mic_clicked(GtkButton* b, gpointer data) {
    (void)b;
    AppState* app = (AppState*)data;
    int it_max = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->spin_it));
    if (app->running) {
        app->running = false;
        if (app->timer_id > 0) { g_source_remove(app->timer_id); app->timer_id = 0; }
    }
    if (app->iter >= it_max && !app->A && app->k == 0) reset_aco(app);
    micro_step(app);
    refresh_ui(app);
}

static void on_rst_clicked(GtkButton* b, gpointer data) {
    (void)b;
    AppState* app = (AppState*)data;
    reset_aco(app);
    refresh_ui(app);
}

static void on_newg_clicked(GtkButton* b, gpointer data) {
    (void)b;
    AppState* app = (AppState*)data;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->spin_seed), rand() % 100000);
    new_random_graph(app);
    refresh_ui(app);
}

static void on_graph_param_changed(GtkSpinButton* s, gpointer data) {
    (void)s;
    AppState* app = (AppState*)data;
    new_random_graph(app);
    refresh_ui(app);
}

static void on_reset_trigger(GtkWidget* w, gpointer data) {
    (void)w;
    AppState* app = (AppState*)data;
    reset_aco(app);
    refresh_ui(app);
}

static void on_it_changed(GtkSpinButton* s, gpointer data) {
    (void)s;
    refresh_ui((AppState*)data);
}

static void on_spd_changed(GtkRange* r, gpointer data) {
    AppState* app = (AppState*)data;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d itér./s", (int)gtk_range_get_value(r));
    gtk_label_set_text(GTK_LABEL(app->lbl_spdv), buf);
}

static void on_bf_clicked(GtkButton* b, gpointer data) {
    (void)b;
    AppState* app = (AppState*)data;
    gtk_label_set_text(GTK_LABEL(app->lbl_bfn), "Calcul en cours…");
    while (gtk_events_pending()) gtk_main_iteration();
    run_brute_force(app);
    refresh_ui(app);
}

static void on_chk_toggled(GtkToggleButton* tb, gpointer data) {
    (void)tb;
    gtk_widget_queue_draw(((AppState*)data)->cv_graph);
}

static void on_file_clicked(GtkButton* b, gpointer data) {
    (void)b;
    AppState* app = (AppState*)data;
    GtkWidget* dlg = gtk_file_chooser_dialog_new(
        "Importer un graphe (.txt)", NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
        "Annuler", GTK_RESPONSE_CANCEL, "Ouvrir", GTK_RESPONSE_ACCEPT, NULL);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char* fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        int n;
        double *x, *y, ref;
        int* ids;
        bool has_ref;
        char err[256];

        if (parse_txt_file(fname, &n, &x, &y, &ids, &has_ref, &ref, err, sizeof(err))) {
            double x0 = x[0], x1 = x[0], y0 = y[0], y1 = y[0];
            for (int i = 1; i < n; i++) {
                if (x[i] < x0) x0 = x[i];
                if (x[i] > x1) x1 = x[i];
                if (y[i] < y0) y0 = y[i];
                if (y[i] > y1) y1 = y[i];
            }
            g_signal_handlers_block_by_func(app->spin_n, on_graph_param_changed, app);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->spin_n), n);
            g_signal_handlers_unblock_by_func(app->spin_n, on_graph_param_changed, app);

            set_graph(app, n, x, y, ids, x0, x1, y0, y1, has_ref, ref);

            char msg[256], bref[32];
            char* base = g_path_get_basename(fname);
            if (has_ref) {
                fmt_num(ref, bref, sizeof(bref));
                snprintf(msg, sizeof(msg), "Importé : %s (%d sommets, réf. %s).", base, n, bref);
            } else {
                snprintf(msg, sizeof(msg), "Importé : %s (%d sommets).", base, n);
            }
            g_free(base);
            gtk_label_set_text(GTK_LABEL(app->lbl_imp), msg);
            refresh_ui(app);
        } else {
            char msg[320];
            snprintf(msg, sizeof(msg), "Import impossible — %s.", err);
            gtk_label_set_text(GTK_LABEL(app->lbl_imp), msg);
        }
        g_free(fname);
    }
    gtk_widget_destroy(dlg);
}

// ============================================================================
// HELPERS DE CONSTRUCTION D'INTERFACE
// ============================================================================
static GtkWidget* make_card(const char* title, GtkWidget** inner_box) {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_style_context_add_class(gtk_widget_get_style_context(box), "box");

    GtkWidget* lbl = gtk_label_new(title);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "box-title");
    gtk_box_pack_start(GTK_BOX(box), lbl, FALSE, FALSE, 0);

    *inner_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(box), *inner_box, FALSE, FALSE, 0);
    return box;
}

static void add_param_row(GtkWidget* parent, const char* label, const char* sub, GtkWidget* ctrl) {
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget* l = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(l), 0.0);
    gtk_box_pack_start(GTK_BOX(left), l, FALSE, FALSE, 0);

    if (sub) {
        GtkWidget* s = gtk_label_new(sub);
        gtk_label_set_xalign(GTK_LABEL(s), 0.0);
        gtk_style_context_add_class(gtk_widget_get_style_context(s), "muted-small");
        gtk_box_pack_start(GTK_BOX(left), s, FALSE, FALSE, 0);
    }

    gtk_widget_set_size_request(ctrl, 82, -1);
    gtk_box_pack_start(GTK_BOX(row), left, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), ctrl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(parent), row, FALSE, FALSE, 0);
}

static GtkWidget* add_metric_row(GtkWidget* parent, const char* label, const char* sub,
                                 const char* hl_class, GtkWidget** out_sub_lbl, bool is_first) {
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    if (!is_first) gtk_style_context_add_class(gtk_widget_get_style_context(row), "metric-row");
    if (hl_class) gtk_style_context_add_class(gtk_widget_get_style_context(row), hl_class);

    GtkWidget* left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* l = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(l), 0.0);
    gtk_box_pack_start(GTK_BOX(left), l, FALSE, FALSE, 0);

    if (sub != NULL) {
        GtkWidget* s = gtk_label_new(sub);
        gtk_label_set_xalign(GTK_LABEL(s), 0.0);
        gtk_style_context_add_class(gtk_widget_get_style_context(s), "muted-small");
        gtk_box_pack_start(GTK_BOX(left), s, FALSE, FALSE, 0);
        if (out_sub_lbl) *out_sub_lbl = s;
    }

    GtkWidget* val = gtk_label_new("—");
    gtk_label_set_xalign(GTK_LABEL(val), 1.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(val), "metric-val");

    gtk_box_pack_start(GTK_BOX(row), left, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), val, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(parent), row, FALSE, FALSE, 0);
    return val;
}

static GtkWidget* wrap_scrollable(GtkWidget* child, int min_width) {
    GtkWidget* sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(sw), TRUE);
    gtk_widget_set_size_request(sw, min_width, -1);
    gtk_container_add(GTK_CONTAINER(sw), child);
    return sw;
}

// ============================================================================
// MAIN
// ============================================================================
int main(int argc, char** argv) {
    gtk_init(&argc, &argv);
    appliquer_css();

    AppState app = {0};

    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Démonstrateur ACO — voyageur de commerce");
    gtk_window_set_default_size(GTK_WINDOW(window), 1160, 740);
    gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget* outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(outer), 12);
    gtk_container_add(GTK_CONTAINER(window), outer);

    // --- EN-TÊTE ---
    GtkWidget* hdr = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget* h1 = gtk_label_new("Colonie de fourmis et voyageur de commerce");
    gtk_label_set_xalign(GTK_LABEL(h1), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(h1), "header-title");
    GtkWidget* hsub = gtk_label_new("Réglez les paramètres, lancez l’ACO et comparez le tour trouvé avec le tour optimal de la force brute.");
    gtk_label_set_xalign(GTK_LABEL(hsub), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(hsub), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(hsub), "header-sub");
    gtk_box_pack_start(GTK_BOX(hdr), h1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hdr), hsub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer), hdr, FALSE, FALSE, 0);

    // --- GRILLE 3 COLONNES ADAPTATIVES ---
    GtkWidget* cols = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(outer), cols, TRUE, TRUE, 0);

    // ==================== COLONNE 1 : GAUCHE (SCROLLABLE) ====================
    GtkWidget* col_left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    GtkWidget* b_graphe;
    GtkWidget* c_graphe = make_card("Graphe", &b_graphe);
    app.spin_n = gtk_spin_button_new_with_range(3, 200, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app.spin_n), 12);
    app.spin_seed = gtk_spin_button_new_with_range(0, 999999, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app.spin_seed), 42);
    add_param_row(b_graphe, "Sommets", NULL, app.spin_n);
    add_param_row(b_graphe, "Graine aléatoire", "même graine, même graphe", app.spin_seed);

    GtkWidget* btn_newg = gtk_button_new_with_label("Nouveau graphe aléatoire");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_newg), "alt");
    gtk_box_pack_start(GTK_BOX(b_graphe), btn_newg, FALSE, FALSE, 1);

    GtkWidget* btn_file = gtk_button_new_with_label("Importer un graphe (.txt)");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_file), "alt");
    gtk_box_pack_start(GTK_BOX(b_graphe), btn_file, FALSE, FALSE, 1);

    app.lbl_imp = gtk_label_new("");
    gtk_label_set_line_wrap(GTK_LABEL(app.lbl_imp), TRUE);
    gtk_label_set_xalign(GTK_LABEL(app.lbl_imp), 0.0);
    gtk_label_set_max_width_chars(GTK_LABEL(app.lbl_imp), 30);
    gtk_style_context_add_class(gtk_widget_get_style_context(app.lbl_imp), "note-text");
    gtk_box_pack_start(GTK_BOX(b_graphe), app.lbl_imp, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(col_left), c_graphe, FALSE, FALSE, 0);

    GtkWidget* b_params;
    GtkWidget* c_params = make_card("Paramètres de la colonie", &b_params);
    app.spin_m = gtk_spin_button_new_with_range(1, 200, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app.spin_m), 15);
    app.spin_it = gtk_spin_button_new_with_range(1, 5000, 10);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app.spin_it), 100);
    app.spin_Q = gtk_spin_button_new_with_range(0.1, 100000.0, 10.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app.spin_Q), 100.0);
    app.spin_a = gtk_spin_button_new_with_range(0.0, 10.0, 0.1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app.spin_a), 1.0);
    app.spin_b = gtk_spin_button_new_with_range(0.0, 10.0, 0.1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app.spin_b), 1.0);
    app.spin_rho = gtk_spin_button_new_with_range(0.0, 0.99, 0.01);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app.spin_rho), 0.35);

    add_param_row(b_params, "Fourmis", NULL, app.spin_m);
    add_param_row(b_params, "Itérations", NULL, app.spin_it);
    add_param_row(b_params, "Phéromone de base Q", "initiale et dépôt", app.spin_Q);
    add_param_row(b_params, "α", "poids de la phéromone", app.spin_a);
    add_param_row(b_params, "β", "poids de la proximité", app.spin_b);
    add_param_row(b_params, "ρ", "taux d’évaporation", app.spin_rho);

    app.combo_mode = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.combo_mode), "Dépôt : Q par arête (code C)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.combo_mode), "Dépôt : Q ÷ longueur (classique)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app.combo_mode), 0);
    gtk_box_pack_start(GTK_BOX(b_params), app.combo_mode, FALSE, FALSE, 2);

    app.scale_spd = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1, 200, 1);
    gtk_scale_set_draw_value(GTK_SCALE(app.scale_spd), FALSE);
    gtk_range_set_value(GTK_RANGE(app.scale_spd), 10);
    GtkWidget* spd_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* spd_left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* lbl_spd = gtk_label_new("Vitesse");
    gtk_label_set_xalign(GTK_LABEL(lbl_spd), 0.0);
    app.lbl_spdv = gtk_label_new("10 itér./s");
    gtk_label_set_xalign(GTK_LABEL(app.lbl_spdv), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(app.lbl_spdv), "muted-small");
    gtk_box_pack_start(GTK_BOX(spd_left), lbl_spd, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(spd_left), app.lbl_spdv, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(spd_row), spd_left, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(spd_row), app.scale_spd, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(b_params), spd_row, FALSE, FALSE, 0);

    GtkWidget* btn_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(btn_grid), 6);
    gtk_grid_set_row_spacing(GTK_GRID(btn_grid), 6);
    gtk_grid_set_column_homogeneous(GTK_GRID(btn_grid), TRUE);

    app.btn_run = gtk_button_new_with_label("Lancer l’ACO");
    gtk_style_context_add_class(gtk_widget_get_style_context(app.btn_run), "primary");
    GtkWidget* btn_mic = gtk_button_new_with_label("Pas de fourmi");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_mic), "alt");
    GtkWidget* btn_one = gtk_button_new_with_label("Une itération");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_one), "alt");
    GtkWidget* btn_rst = gtk_button_new_with_label("Réinitialiser");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_rst), "alt");

    gtk_grid_attach(GTK_GRID(btn_grid), app.btn_run, 0, 0, 2, 1);
    gtk_grid_attach(GTK_GRID(btn_grid), btn_mic,     0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(btn_grid), btn_one,     1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(btn_grid), btn_rst,     0, 2, 2, 1);
    gtk_box_pack_start(GTK_BOX(b_params), btn_grid, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(col_left), c_params, FALSE, FALSE, 0);

    GtkWidget* b_aff;
    GtkWidget* c_aff = make_card("Affichage", &b_aff);
    app.chk_ph = gtk_check_button_new_with_label("Phéromones");
    app.chk_best = gtk_check_button_new_with_label("Meilleur tour ACO");
    app.chk_bf = gtk_check_button_new_with_label("Tour optimal (force brute)");
    app.chk_num = gtk_check_button_new_with_label("Numéros des sommets");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.chk_ph), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.chk_best), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.chk_bf), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.chk_num), TRUE);
    gtk_box_pack_start(GTK_BOX(b_aff), app.chk_ph, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(b_aff), app.chk_best, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(b_aff), app.chk_bf, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(b_aff), app.chk_num, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(col_left), c_aff, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(cols), wrap_scrollable(col_left, 250), FALSE, FALSE, 0);

    // ==================== COLONNE 2 : CENTRE (STAGE EXTENSIBLE) ====================
    GtkWidget* stage = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(stage), "box");
    gtk_box_pack_start(GTK_BOX(cols), stage, TRUE, TRUE, 0);

    app.cv_graph = gtk_drawing_area_new();
    gtk_widget_set_size_request(app.cv_graph, 280, 240);
    gtk_widget_set_hexpand(app.cv_graph, TRUE);
    gtk_widget_set_vexpand(app.cv_graph, TRUE);
    g_signal_connect(app.cv_graph, "draw", G_CALLBACK(on_draw_graph), &app);
    gtk_box_pack_start(GTK_BOX(stage), app.cv_graph, TRUE, TRUE, 0);

    GtkWidget* leg = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(leg),
        "<span foreground='#B0176F'><b>━━</b></span> <span foreground='#5B7270'>Phéromone</span>  "
        "<span foreground='#C77F00'><b>━━</b></span> <span foreground='#5B7270'>Meilleur ACO</span>  "
        "<span foreground='#1F6FB5'><b>╌╌</b></span> <span foreground='#5B7270'>Optimal</span>  "
        "<span foreground='#14302E'><b>━━</b></span> <span foreground='#5B7270'>Fourmi</span>");
    gtk_label_set_xalign(GTK_LABEL(leg), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(leg), TRUE);
    gtk_box_pack_start(GTK_BOX(stage), leg, FALSE, FALSE, 0);

    app.det_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(app.det_box), "det-sep");
    gtk_box_pack_start(GTK_BOX(stage), app.det_box, FALSE, FALSE, 0);

    // ==================== COLONNE 3 : DROITE (SCROLLABLE) ====================
    GtkWidget* col_right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    GtkWidget* b_res;
    GtkWidget* c_res = make_card("Résultats", &b_res);
    app.lbl_iter = add_metric_row(b_res, "Itération", NULL, NULL, NULL, true);
    app.lbl_bc   = add_metric_row(b_res, "Meilleur tour ACO", "", "hl-best", &app.lbl_bi, false);
    app.lbl_tm   = add_metric_row(b_res, "Temps ACO", NULL, NULL, NULL, false);
    app.lbl_fc   = add_metric_row(b_res, "Optimum force brute", "", "hl-bf", &app.lbl_fi, false);
    app.lbl_ft   = add_metric_row(b_res, "Temps force brute", NULL, NULL, NULL, false);
    app.lbl_rc   = add_metric_row(b_res, "Référence (fichier)", NULL, NULL, NULL, false);
    app.lbl_gap  = add_metric_row(b_res, "Écart ACO / réf.", NULL, NULL, NULL, false);

    app.btn_bf = gtk_button_new_with_label("Calculer la force brute");
    gtk_style_context_add_class(gtk_widget_get_style_context(app.btn_bf), "alt");
    gtk_box_pack_start(GTK_BOX(b_res), app.btn_bf, FALSE, FALSE, 4);

    app.lbl_bfn = gtk_label_new("");
    gtk_label_set_line_wrap(GTK_LABEL(app.lbl_bfn), TRUE);
    gtk_label_set_xalign(GTK_LABEL(app.lbl_bfn), 0.0);
    gtk_label_set_max_width_chars(GTK_LABEL(app.lbl_bfn), 30);
    gtk_style_context_add_class(gtk_widget_get_style_context(app.lbl_bfn), "note-text");
    gtk_box_pack_start(GTK_BOX(b_res), app.lbl_bfn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(col_right), c_res, FALSE, FALSE, 0);

    GtkWidget* b_conv;
    GtkWidget* c_conv = make_card("Convergence", &b_conv);
    app.cv_chart = gtk_drawing_area_new();
    gtk_widget_set_size_request(app.cv_chart, 230, 150);
    g_signal_connect(app.cv_chart, "draw", G_CALLBACK(on_draw_chart), &app);
    gtk_box_pack_start(GTK_BOX(b_conv), app.cv_chart, FALSE, FALSE, 0);

    GtkWidget* cleg = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(cleg),
        "<span foreground='#C77F00'><b>━━</b></span> <span foreground='#5B7270'>Global</span>  "
        "<span foreground='#8EA9A5'><b>━━</b></span> <span foreground='#5B7270'>Itér.</span>  "
        "<span foreground='#14302E'><b>···</b></span> <span foreground='#5B7270'>Réf.</span>");
    gtk_label_set_xalign(GTK_LABEL(cleg), 0.0);
    gtk_box_pack_start(GTK_BOX(b_conv), cleg, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(col_right), c_conv, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(cols), wrap_scrollable(col_right, 260), FALSE, FALSE, 0);

    // Connexion des signaux
    g_signal_connect(app.btn_run, "clicked", G_CALLBACK(on_run_clicked), &app);
    g_signal_connect(btn_mic, "clicked", G_CALLBACK(on_mic_clicked), &app);
    g_signal_connect(btn_one, "clicked", G_CALLBACK(on_one_clicked), &app);
    g_signal_connect(btn_rst, "clicked", G_CALLBACK(on_rst_clicked), &app);
    g_signal_connect(btn_newg, "clicked", G_CALLBACK(on_newg_clicked), &app);
    g_signal_connect(btn_file, "clicked", G_CALLBACK(on_file_clicked), &app);
    g_signal_connect(app.btn_bf, "clicked", G_CALLBACK(on_bf_clicked), &app);

    g_signal_connect(app.spin_n, "value-changed", G_CALLBACK(on_graph_param_changed), &app);
    g_signal_connect(app.spin_seed, "value-changed", G_CALLBACK(on_graph_param_changed), &app);
    g_signal_connect(app.spin_Q, "value-changed", G_CALLBACK(on_reset_trigger), &app);
    g_signal_connect(app.combo_mode, "changed", G_CALLBACK(on_reset_trigger), &app);
    g_signal_connect(app.spin_it, "value-changed", G_CALLBACK(on_it_changed), &app);
    g_signal_connect(app.scale_spd, "value-changed", G_CALLBACK(on_spd_changed), &app);

    g_signal_connect(app.chk_ph, "toggled", G_CALLBACK(on_chk_toggled), &app);
    g_signal_connect(app.chk_best, "toggled", G_CALLBACK(on_chk_toggled), &app);
    g_signal_connect(app.chk_bf, "toggled", G_CALLBACK(on_chk_toggled), &app);
    g_signal_connect(app.chk_num, "toggled", G_CALLBACK(on_chk_toggled), &app);

    new_random_graph(&app);
    refresh_ui(&app);

    gtk_widget_show_all(window);
    gtk_main();

    free_fourmi(app.A);
    free_bf(&app);
    free(app.tau);
    free(app.best.path);
    free(app.hist_iter_best);
    free(app.hist_global_best);
    free(app.G.x);
    free(app.G.y);
    free(app.G.ids);
    free(app.G.D);
    return 0;
}