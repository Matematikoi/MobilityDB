/*****************************************************************************
 *
 * This MobilityDB code is provided under The PostgreSQL License.
 * Copyright (c) 2016-2024, Université libre de Bruxelles and MobilityDB
 * contributors
 *
 * MobilityDB includes portions of PostGIS version 3 source code released
 * under the GNU General Public License (GPLv2 or later).
 * Copyright (c) 2001-2024, PostGIS contributors
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without a written
 * agreement is hereby granted, provided that the above copyright notice and
 * this paragraph and the following two paragraphs appear in all copies.
 *
 * IN NO EVENT SHALL UNIVERSITE LIBRE DE BRUXELLES BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
 * LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
 * EVEN IF UNIVERSITE LIBRE DE BRUXELLES HAS BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * UNIVERSITE LIBRE DE BRUXELLES SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE PROVIDED HEREUNDER IS ON
 * AN "AS IS" BASIS, AND UNIVERSITE LIBRE DE BRUXELLES HAS NO OBLIGATIONS TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 *****************************************************************************/

/**
 * @brief A simple program that reads AIS data from a CSV file containing one
 * full day of observations provided by the Danish Maritime Authority in
 * https://web.ais.dk/aisdata/, constructs for each ship temporal values for
 * the trip and the SOG, and outputs for each ship the MMSI, the number
 * of records and instants used for contruct the temporal values, the distance
 * travelled and the time-weighted average of the SOG.
 *
 * Please notice that the `data` directory DOES NOT contain the input CSV file,
 * you must download it from the website above.
 *
 * Please read the assumptions made about the input CSV file in the file
 * `02_ais_read.c` in the same directory. The program assumes that the input
 * file contains at most a given number of records for at most a given number
 * of ships, as defined by two compilation constraints. Also, the program copes
 * with minimal error correction, that is,
 * - It supposes that the observations have increasing timestamp values
 * - It verifies that the latitude, longitude, and SOG values read are in
 *   specified ranges, and
 * - It takes only the first observation when there are two or more
 *   observations for the same ship with equal timestamp values.
 *
 * The program can be build as follows
 * @code
 * gcc -Wall -g -I/usr/local/include -o ais_expand_full ais_expand_full.c -L/usr/local/lib -lmeos
 * @endcode
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* For memset */
#include <time.h>
#include <meos.h>
/* The expandable functions are in the internal MEOS API */
#include <meos_internal.h>
#include "rtree/rtree.h"

/*
 * IMPORTANT !!!
 * Please fix the values MAX_NO_RECORDS and MAX_SHIPS according to the
 * available memory in your computer
 */
/* Maximum number of records read in the CSV file */
#define MAX_NO_RECORDS 2000000000
/* Maximum number of trips */
#define MAX_SHIPS 100000
/* Number of instants in a batch for printing a marker */
#define NO_RECORDS_BATCH 100000
/* Initial number of allocated instants for an input trip and SOG */
#define INITIAL_INSTANTS 64
/* Maximum length in characters of a record in the input CSV file */
#define MAX_LENGTH_LINE 1024
/* Maximum length in characters of a point in the input data */
#define MAX_LENGTH_POINT 64
/* Maximum length in characters of a timestamp in the input data */
#define MAX_LENGTH_TIMESTAMP 32
/* Maximum length in characters of all other strings in the input data */
#define MAX_LENGTH_STRING 64

bool index_distances[MAX_SHIPS];
bool real_distances[MAX_SHIPS];



typedef struct
{
  Timestamp T;
  long int MMSI;
  double Latitude;
  double Longitude;
  double SOG;
} AIS_record;

typedef struct
{
  long int MMSI;          /* Identifier of the trip */
  int no_records;         /* Number of input records for the trip */
  int no_trip_instants;   /* Number of input instants for the trip */
  int no_SOG_instants;    /* Number of input instants for the SOG */
  TSequence *trip;        /* Sequence accumulating the trip observations */
  TSequence *SOG;         /* Sequence accumulating the SOG observations */
} trip_record;


void no_ships_close_to_ship(trip_record* trips, int no_ships, int ship_to_search,float max_distance, bool distances[]){
  int cnt = 0;
  for (int i = 0; i < no_ships; ++i){
    if (trips[i].trip == NULL){
      continue;
    }
    double distance = nad_tpoint_tpoint(
      (Temporal *)tgeogpoint_to_tgeompoint(tpoint_transform((Temporal *)trips[i].trip, 3034)),
      (Temporal *)tgeogpoint_to_tgeompoint(tpoint_transform((Temporal *)trips[ship_to_search].trip, 3034)));
    /* double distance = nad_tpoint_tpoint( */
    /*   trips[i].trip, */
    /*   trips[ship_to_search].trip); */
    /* printf("\nitem %d distance %f", i, distance); */

    if(max_distance > distance ){
      cnt++;
      distances[i] = true;
    }
  }
  printf("\nTotal number of ships close to %d: %d\n", ship_to_search, cnt);
}


struct rtree* rtree_create ( STBox** boxes, long int* ids, int no_boxes){
  struct rtree *tr = rtree_new();
  for (int i = 0; i<no_boxes; ++i){
    if (boxes[i] == NULL){
      continue;
    }
    rtree_insert(
                tr,
                (double[3]){boxes[i]->xmin, boxes[i]->ymin, boxes[i]->zmin},
                (double[3]){boxes[i]->xmax, boxes[i]->ymax, boxes[i]->zmax},
                &ids[i]
                 );
  }
  return tr;
}

struct rtree* create_index(trip_record* trip, int no_ships, STBox ** boxes){
  long int* ids = malloc(sizeof ( long int ) * no_ships );
  STBox *box = malloc(sizeof(STBox));
  for (int i = 0; i<no_ships; i++){
    if(trip[i].trip == NULL){
      boxes[i] = NULL;
      continue;
    }
    tspatialseq_set_stbox(trip[i].trip, box);
    STBox *box_2 = stbox_transform(box, 25832);
    /* printf("\nSTBOX %d %f %f %s, %s",i , box_2->xmin, box_2->ymin, pg_timestamp_out(box_2->period.lower), pg_timestamp_out(box_2->period.upper)); */
    boxes[i] = box_2;
    ids[i] = i;
  }
  return rtree_create(boxes, ids, no_ships);
}

bool ship_iter(const double* min, const double *max, const void* item, void *udata){
  const int * shipId = item;
  /* printf("\nFound ship %d", *shipId); */
  index_distances[*shipId] = true;
  int *foo = udata;
  ++*foo;
  return true;
}

long int* query_index(int ship_to_search, float max_distance, const struct rtree* tr, STBox ** boxes, trip_record* trips, int no_ships){
  int i = ship_to_search;
  int found = 0;
  rtree_search(
               tr,
               (double[3]){boxes[i]->xmin-max_distance, boxes[i]->ymin-max_distance, boxes[i]->zmin},
               (double[3]){boxes[i]->xmax+max_distance, boxes[i]->ymax+max_distance, boxes[i]->zmax},
               ship_iter,
               (void*) &found
               );
  printf("\n Found in the index %d elements", found);
  for (int i=0 ; i< no_ships; ++i){
    if (index_distances[i]){
      double distance = nad_tpoint_tpoint(
        (Temporal *)tgeogpoint_to_tgeompoint(tpoint_transform((Temporal *)trips[i].trip, 3034)),
        (Temporal *)tgeogpoint_to_tgeompoint(tpoint_transform((Temporal *)trips[ship_to_search].trip, 3034)));
      index_distances[i] = distance < max_distance;
    }
  }
  return NULL;
}

/* Main program */
int main(void)
{
  /* Input buffers to read the CSV file */
  char line_buffer[MAX_LENGTH_LINE];
  char point_buffer[MAX_LENGTH_POINT];
  /* Allocate space to build the trips */
  trip_record trips[MAX_SHIPS] = {0};
  /* For the confusion matrix */
  /* Record storing one line read from of the CSV file*/
  AIS_record rec;
  /* Number of records read */
  int no_records = 0;
  /* Number of erroneous records */
  int no_err_records = 0;
  /* Number of ships */
  int no_ships = 0;
  /* Iterator variables */
  int i, j;
  /* Exit value initialized to 1 (i.e., error) to quickly exit upon error */
  int exit_value = 1;

  /* Initialize MEOS */
  meos_initialize("UTC", NULL);

  /* Get start time */
  clock_t t = clock();

  /*
   * Open the input file
   * Substitute the full file path in the first argument of fopen
   */
  FILE *file = fopen("data/aisdk-2023-08-01.csv", "r");
  if (! file)
  {
    printf("Error opening input file\n");
    goto cleanup;
  }

  /* Read the first line of the file with the headers */
  fscanf(file, "%1023[^\n]\n", line_buffer);
  printf("Processing records\n");
  printf("  one '*' marker every %d records\n", NO_RECORDS_BATCH);
  /* Uncomment the next lines to display a marker each time and incomplete
   * record or an erroneous field has been read */
  // printf("  one '-' marker every incomplete or erroneous records\n");
  // printf("  one 'T' marker every record with duplicate timestamp and position\n");
  // printf("  one 'S' marker every record with duplicate timestamp and SGO\n");

  /* Continue reading the file */
  do
  {
    fscanf(file, "%1023[^\n]\n", line_buffer);
    if (ferror(file))
    {
      printf("\nError reading input file\n");
      fclose(file);
      goto cleanup;
    }

    no_records++;
    /* Print a marker every X records read */
    if (no_records % NO_RECORDS_BATCH == 0)
    {
      printf("*");
      fflush(stdout);
    }
    /* Break if maximum number of records read */
    if (no_records == MAX_NO_RECORDS)
      break;

    /* Initialize record to 0 */
    memset(&rec, 0, sizeof(rec));
    int field = 0;
    char *token = strtok(line_buffer, ",");
    bool has_t = false, has_mmsi = false, has_lat = false,
      has_long = false, has_sog = false;
    while (token != NULL)
    {
      if (strlen(token) != 0 && strcmp(token, "Unknown") != 0)
      {
        switch (field)
        {
          case 0:
            rec.T = pg_timestamp_in(token, -1);
            if (rec.T != 0)
              has_t = true;
            break;
          case 2:
            rec.MMSI = strtoll(token, NULL, 0);
            /* The next line can be used to filter the records to a given MMSI */
            // if (rec.MMSI == -1961112608 )
            if (rec.MMSI != 0)
              has_mmsi = true;
            break;
          case 3:
            rec.Latitude = strtold(token, NULL);
            /* The next line ensures the validity of the latitude value */
            // if (rec.Latitude >= -90.0 && rec.Latitude <= 90.0)
            /* The next line ensures the validity of the latitude value given
             * that the input file reports observations around Denmark */
            if (rec.Latitude >= 40.18 && rec.Latitude <= 84.17)
              has_lat = true;
            break;
          case 4:
            rec.Longitude = strtold(token, NULL);
            /* The next line ensures the validity of the longitude value */
            // if (rec.Longitude >= -180.0 && rec.Longitude <= 180.0)
            /* The next line ensures the validity of the longitude value given
             * that the input file reports observations around Denmark */
            if (rec.Longitude >= -16.1 && rec.Longitude <= 32.88)
              has_long = true;
            break;
          case 7:
            rec.SOG = strtold(token, NULL);
            /* Speed over ground in 1/10 knot steps (0-102.2 knots)
             * See https://www.navcen.uscg.gov/ais-class-a-reports */
            if (rec.SOG >= 0 && rec.SOG <= 1022)
              has_sog = true;
            break;
          default:
            break;
        }
      }
      token = strtok(NULL, ",");
      field++;
      if (field > 7)
        break;
    }

    if (! has_t || ! has_mmsi || ! ( ( has_lat && has_long ) || has_sog) )
    {
      /* Uncomment the next lines to display a marker each time
       * an incomplete record or an erroneous field has been read */
      // printf("-");
      // fflush(stdout);
      no_err_records++;
      continue;
    }

    /* Find the place to store the new instant */
    j = -1;
    // TODO: this should me a map
    for (i = 0; i < no_ships; i++)
    {
      if (trips[i].MMSI == rec.MMSI)
      {
        j = i;
        break;
      }
    }
    if (j < 0)
    {
      /* If we have reached the maximum number of ships */
      if (no_ships == MAX_SHIPS)
        continue;
      j = no_ships++;
      trips[j].MMSI = rec.MMSI;
    }
    trips[j].no_records++;

    /*
     * Create the instants and store them in the arrays of the ship.
     * In the input file it is assumed that
     * - The coordinates are given in the WGS84 geographic coordinate system
     * - The timestamps are given in GMT time zone
     */

    TInstant *inst;
    TSequence *new_seq;
    const TInstant *last;
    /* Create an Trip instant from the record */
    if (has_lat && has_long)
    {
      char *t_out = pg_timestamp_out(rec.T);
      sprintf(point_buffer, "SRID=4326;Point(%lf %lf)@%s+00", rec.Longitude,
        rec.Latitude, t_out);

      free(t_out);
      inst = (TInstant *) tgeogpoint_in(point_buffer);
      /* Ensure there is still space for storing the temporal point instant */
      if (trips[j].trip == NULL)
      {
        trips[j].trip = tsequence_make_exp((const TInstant **) &inst, 1,
          INITIAL_INSTANTS, true, true, LINEAR, false);
        if (trips[j].trip == NULL)
        {
          printf("\nMSSI: %ld, there is no more memory to expand the trip\n",
            trips[j].MMSI);
          fclose(file);
          goto cleanup;
        }
        trips[j].no_records++;
        trips[j].no_trip_instants++;
      }
      /* Ignore the instant if has the same timestamp as the last one */
      const TInstant *last = TSEQUENCE_INST_N(trips[j].trip,
        trips[j].trip->count - 1);
      if (last->t == inst->t)
      {
        free(inst);
        /* Uncomment the next lines to display a marker each time a Trip
         * observation has the same timestamp as the previous one */
        // printf("T");
        // fflush(stdout);
      }
      else
      {
        new_seq = (TSequence *) tsequence_append_tinstant(trips[j].trip, inst,
          0.0, NULL, true);
        /* Uncomment the next lines to display debug messages showing how the
         * the data structures are expanded */
        // if (trips[j].trip && trips[j].trip != new_seq)
        // {
          // printf("MMSI: %ld ", trips[j].MMSI);
          // printf("Trip %d -> %d ", new_seq->maxcount / 2, new_seq->maxcount);
          // fflush(stdout);
        // }
        trips[j].trip = new_seq;
        trips[j].no_trip_instants++;
      }
    }
    /* Create an SOG instant from the record */
    if (has_sog)
    {
      inst = (TInstant *) tfloatinst_make(rec.SOG, rec.T);
      /* Ensure there is still space for storing the temporal float instant */
      if (trips[j].SOG == NULL)
      {
        /* Uncomment the next lines to display debug messages showing how
         * the data structures are expanded */
        // printf("MMSI: %ld ", trips[j].MMSI);
        // printf("Speed %d -> %d ", trips[j].no_SOG_instants,
          // trips[j].no_SOG_instants * 2);
        // fflush(stdout);
        trips[j].SOG = tsequence_make_exp((const TInstant **) &inst, 1,
          INITIAL_INSTANTS, true,  true, LINEAR, false);
        if (trips[j].SOG == NULL)
        {
          printf("\nMSSI: %ld, there is no more memory to expand the speed\n",
            trips[j].MMSI);
          fclose(file);
          goto cleanup;
        }
      }
      /* Ignore the instant if has the same timestamp as the last one */
      last = TSEQUENCE_INST_N(trips[j].SOG, trips[j].SOG->count - 1);
      if (last->t == inst->t)
      {
        free(inst);
        // Uncomment the next lines to display a marker each time a SOG
        // observation has the same timestamp as the previous one
        // printf("S");
        // fflush(stdout);
      }
      else
      {
        new_seq = (TSequence *) tsequence_append_tinstant(trips[j].SOG, inst,
          0.0, NULL, true);
        /* Uncomment the next lines to display debug messages showing how the
         * the data structures are expanded */
        // if (trips[j].SOG && trips[j].SOG != new_seq)
        // {
          // printf("MMSI: %ld ", trips[j].MMSI);
          // printf("SOG %d -> %d ", new_seq->maxcount / 2, new_seq->maxcount);
          // fflush(stdout);
        // }
        trips[j].SOG = new_seq;
        trips[j].no_SOG_instants++;
      }
    }
  } while (! feof(file));

  /* Close the file */
  fclose(file);
  printf("-----------------------------------------------------------------------------\n");
  printf("\n%d records read.\n%d erroneous records ignored.\n", no_records,
    no_err_records);
  printf("%d trips read.\n", no_ships);
  const int selected_ship =1;
  const float max_distance = 1000.0;
  /* for (int i =0; i<no_ships; ++i){ */
  /*   if(trips[i].trip == NULL)continue; */
  /*   float distance = nad_tpoint_tpoint((Temporal *) trips[i].trip, (Temporal *) trips[selected_ship].trip); */
  /*   printf("\n %d - %d) Distance to %ld: %f",distance< max_distance, i,trips[i].MMSI, distance); */
  /* } */

  /* Calculate the elapsed time */
  t = clock() - t;
  double time_taken = ((double) t) / CLOCKS_PER_SEC;
  printf("\nThe program took %f seconds to execute\n", time_taken);

  /* Call a function to query on trips */
  /* make_queries(trips, no_ships); */
  t = clock();
  no_ships_close_to_ship(trips, no_ships,selected_ship, max_distance, real_distances);
  t = clock()-t;
  printf("\nTo look for closest ship it took: %f seconds\n", ((double) t) / CLOCKS_PER_SEC);


  STBox** boxes = malloc(sizeof(STBox *) * no_ships );
  t = clock();
  struct rtree *tree = create_index(trips, no_ships, boxes);
  t = clock()-t;
  printf("\nTo create the index it took: %f seconds\n", ((double) t) / CLOCKS_PER_SEC);


  t = clock();
  query_index (selected_ship, max_distance * 1.5  , tree, boxes, trips, no_ships);
  t = clock()-t;
  printf("\nTo lookup in the index it took: %f seconds\n", ((double) t) / CLOCKS_PER_SEC);


  /* CONFUSION MATRIX! */
  int tp = 0, tn =0, fp=0,fn=0;
  for (int i = 0 ; i< no_ships; ++i){
    if(trips[i].trip == NULL){
      continue;
    }
    if (real_distances[i] == true && index_distances[i] == true){
      /* printf("\n True Positives %d", i); */
      ++tp;
    }
    if(real_distances[i] == true && index_distances[i] == false){
      /* printf("\n False negative %d", i); */
      fn++;
    }
    if(real_distances[i] == false && index_distances[i] == true){
      printf("\n False positive %d", i);
      fp++;
    }
    if(real_distances[i] == false && index_distances[i] == false){
      tn++;
    }
  }
  printf("\nCONFUSION MATRIX\n\t|pos\t|neg\t|\npos\t|%d\t|%d\t|\nneg\t|%d\t|%d\t|\n",tp,fp,fn,tn);

  /* int erroneous[] = {selected_ship,1640, 2235, 3346,4094,4353,4680,5441}; */
  /* printf("MAX DISTANCE: %f", max_distance); */
  /* for (int i = 0; i< 8; ++i){ */
  /*   if (!trips[erroneous[i]].trip) continue; */
  /*   double distance = nad_tpoint_tpoint( */
  /*     (Temporal *)tgeogpoint_to_tgeompoint(tpoint_transform((Temporal *)trips[erroneous[i]].trip, 3034)), */
  /*     (Temporal *)tgeogpoint_to_tgeompoint(tpoint_transform((Temporal *)trips[selected_ship].trip, 3034))); */
  /*   printf("\n--%d--%ld-- distance: %f",erroneous[i],trips[erroneous[i]].MMSI, distance); */
  /*   printf("\n STBox-- xmin: %f, xmax: %f, ymin: %f, ymax: %f", boxes[erroneous[i]]->xmin, boxes[erroneous[i]]->xmax, boxes[erroneous[i]]->ymin, boxes[erroneous[i]]->ymax); */
  /* } */
  /* printf("\n"); */

  /* State that the program executed successfully */
  exit_value = 0;
  FILE *fptr;

  // Open a file in append mode
  fptr = fopen("./data/linestring_aisdk.csv", "w");
    /* MAKE CSV  */
  fprintf(fptr, "MMSI|ID|GEOM");
  for (int i = 0 ; i<no_ships; ++i){
    if (trips[i].trip == NULL) continue;
    GSERIALIZED * foo = tpointseq_trajectory((TSequence *)tpoint_transform((Temporal*)trips[i].trip, 4326));
    /* GSERIALIZED * foo = tpointseq_trajectory((TSequence *)tpoint_transform((Temporal*)trips[i].trip, 3034)); */
    fprintf(fptr,"\n%ld|%d", trips[i].MMSI,i);
    fprintf(fptr,"|%s",geo_as_ewkt(foo, 6)+10);
  }
  fprintf(fptr,"\n");

/* Clean up */
cleanup:

 /* Free memory */
  for (i = 0; i < no_ships; i++)
  {
    free(trips[i].trip);
    free(trips[i].SOG);
  }

  /* Finalize MEOS */
  meos_finalize();

  return exit_value;
}
