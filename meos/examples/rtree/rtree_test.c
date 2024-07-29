#include <stdio.h>
#include <string.h>
#include <math.h>
#include "rtree.h"

struct city {
    char *name;
    double lat;
    double lon;
    double time;
};

struct city phx = { .name = "Phoenix", .lat = 33.448, .lon = -112.073, .time = 50};
struct city enn = { .name = "Ennis", .lat = 52.843, .lon = -8.986 , .time = 50 };
struct city pra = { .name = "Prague", .lat = 50.088, .lon = 14.420 , .time = 50  };
struct city tai = { .name = "Taipei", .lat = 25.033, .lon = 121.565 , .time= 50};
struct city her = { .name = "Hermosillo", .lat = 29.102, .lon = -110.977 , .time= 500};
struct city him = { .name = "Himeji", .lat = 34.816, .lon = 134.700 , .time= 50};

bool city_iter(const double *min, const double *max, const void *item, void *udata) {
    const struct city *city = item;
    printf("%s\n", city->name);
    return true;
}

int main() {
    // Create a new rtree where each item is a `struct city*`.
    struct rtree *tr = rtree_new();

    // Load some cities into the rtree. Each insert operation performs a copy
    // of the data that is pointed to in the second and third arguments.
    // The R-tree expects a rectangle, which is two arrays of doubles.
    // The first N values as the minimum corner of the rect, and the next
    // N values as the maximum corner of the rect, where N is the number of
    // dimensions. The default R-tree has 2 dimensions.
    // When inserting points, the max coordinates are optional.
    rtree_insert(tr, (double[3]){phx.lon, phx.lat, phx.time}, NULL, &phx);
    rtree_insert(tr, (double[3]){enn.lon, enn.lat, enn.time}, NULL, &enn);
    rtree_insert(tr, (double[3]){pra.lon, pra.lat, pra.time}, NULL, &pra);
    rtree_insert(tr, (double[3]){tai.lon, tai.lat, tai.time}, NULL, &tai);
    rtree_insert(tr, (double[3]){her.lon, her.lat, her.time}, NULL, &her);
    rtree_insert(tr, (double[3]){him.lon, him.lat, him.time}, NULL, &him);

    printf("\n-- Northwestern cities --\n");
    rtree_search(tr, (double[3]){-180, 0, 0}, (double[3]){0, 90, 100}, city_iter, NULL);

    rtree_free(tr);
}
// output:
// -- Northwestern cities --
// Phoenix
// Ennis
//
// Hermosillo is not here BECAUSE it has been filtered out with the 3rd dimension time
// It is still  unclear if the #define DIM 3 should stay like that
