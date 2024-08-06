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

struct city phx = { .name = "1", .lat = 1, .lon = 1, .time = 1};
struct city enn = { .name = "2", .lat = 2, .lon = 2, .time = 2};
struct city pra = { .name = "3", .lat = 3, .lon = 3, .time = 3};
struct city tai = { .name = "4", .lat = 4, .lon = 4, .time = 4};
struct city her = { .name = "5", .lat = 5, .lon = 5, .time = 5};
struct city him = { .name = "6", .lat = 6, .lon = 6, .time = 6};

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
    rtree_insert(tr, (double[3]){phx.lon, phx.lat, phx.time}, (double[3]){phx.lon+1, phx.lat+1, phx.time+1}, &phx);
    rtree_insert(tr, (double[3]){enn.lon, enn.lat, enn.time}, (double[3]){enn.lon+1, enn.lat+1, enn.time+1}, &enn);
    rtree_insert(tr, (double[3]){pra.lon, pra.lat, pra.time}, (double[3]){pra.lon+1, pra.lat+1, pra.time+1}, &pra);
    rtree_insert(tr, (double[3]){tai.lon, tai.lat, tai.time}, (double[3]){tai.lon+1, tai.lat+1, tai.time+1}, &tai);
    rtree_insert(tr, (double[3]){her.lon, her.lat, her.time}, (double[3]){her.lon+1, her.lat+1, her.time+1}, &her);
    rtree_insert(tr, (double[3]){him.lon, him.lat, him.time}, (double[3]){him.lon+1, him.lat+1, him.time+1}, &him);

    float l1,l2,l3,u1,u2,u3;

    /* printf("\n-- [2,3][0,0][0,0] --\n"); */
    /* rtree_search(tr, (double[3]){2, 0, 0}, (double[3]){3, 0, 0}, city_iter, NULL); */

    /* printf("\n-- [2,3][-1,2][-1,2] --\n"); */
    /* rtree_search(tr, (double[3]){-1, -1, -1}, (double[3]){8, 2, 2}, city_iter, NULL); */

    /* printf("\n-- [2,4][-1,1][-1,1] --\n"); */
    /* rtree_search(tr, (double[3]){2, -1, -1}, (double[3]){4, 1, 1}, city_iter, NULL); */


    printf("\n-- l1=0;l2=0;l3=0; u1=0;u2=0;u3=0; --\n");
    l1=0;l2=0;l3=0; u1=0;u2=0;u3=0;
    rtree_search(tr, (double[3]){l1, l2, l3}, (double[3]){u1, u2, u3}, city_iter, NULL);

    printf("\n-- l1=0;l2=0;l3=0; u1=1;u2=1;u3=1; --\n");
    l1=0;l2=0;l3=0; u1=1;u2=1;u3=1;
    rtree_search(tr, (double[3]){l1, l2, l3}, (double[3]){u1, u2, u3}, city_iter, NULL);

    printf("\n-- l1=3;l2=3;l3=3; u1=4;u2=4;u3=4; --\n");
    l1=3;l2=3;l3=3; u1=4;u2=4;u3=4;
    rtree_search(tr, (double[3]){l1, l2, l3}, (double[3]){u1, u2, u3}, city_iter, NULL);

    printf("\n-- l1=2;l2=2;l3=2; u1=4;u2=4;u3=4; --\n");
    l1=2;l2=2;l3=2; u1=4;u2=4;u3=4;
    rtree_search(tr, (double[3]){l1, l2, l3}, (double[3]){u1, u2, u3}, city_iter, NULL);

    printf("\n-- l1=-10;l2=-10;l3=-10; u1=10;u2=10;u3=10; --\n");
    l1=-10;l2=-10;l3=-10; u1=10;u2=10;u3=10;
    rtree_search(tr, (double[3]){l1, l2, l3}, (double[3]){u1, u2, u3}, city_iter, NULL);

    printf("\n-- l1=10;l2=10;l3=10; u1=11;u2=11;u3=11; --\n");
    l1=10;l2=10;l3=10; u1=11;u2=11;u3=11;
    rtree_search(tr, (double[3]){l1, l2, l3}, (double[3]){u1, u2, u3}, city_iter, NULL);

    printf("\n-- l1=3.1;l2=3;l3=3; u1=4;u2=4;u3=4; --\n");
    l1=3.1;l2=3;l3=3; u1=4;u2=4;u3=4;
    rtree_search(tr, (double[3]){l1, l2, l3}, (double[3]){u1, u2, u3}, city_iter, NULL);

    printf("\n-- l1=3;l2=0;l3=0; u1=3;u2=4;u3=4; --\n");
    l1=3;l2=0;l3=0; u1=3;u2=4;u3=4;
    rtree_search(tr, (double[3]){l1, l2, l3}, (double[3]){u1, u2, u3}, city_iter, NULL);

    rtree_free(tr);
}
// output:
// -- Northwestern cities --
// Phoenix
// Ennis
//
// Hermosillo is not here BECAUSE it has been filtered out with the 3rd dimension time
// It is still  unclear if the #define DIM 3 should stay like that
