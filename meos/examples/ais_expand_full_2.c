#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* For memset */
#include <time.h>
#include <meos.h>
/* The expandable functions are in the internal MEOS API */
#include <meos_internal.h>

/*
 * IMPORTANT !!!
 * Please fix the values MAX_NO_RECORDS and MAX_SHIPS according to the
 * available memory in your computer
 */
/* Maximum number of records read in the CSV file */
#define MAX_NO_RECORDS 20000000
/* Maximum number of trips */
#define MAX_SHIPS 6500
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

/* Main program */
int main(void)
{
  /* Input buffers to read the CSV file */
  char line_buffer[MAX_LENGTH_LINE];
  char point_buffer[MAX_LENGTH_POINT];
  /* Allocate space to build the trips */
  trip_record trips[MAX_SHIPS] = {0};
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

  /* Construct the trips */
  printf("\n-----------------------------------------------------------------------------\n");
  printf("|   MMSI    |   #Rec  | #TrInst |  #SInst |     Distance    |     Speed     |\n");
  printf("-----------------------------------------------------------------------------\n");
  for (i = 0; i < no_ships; i++)
  {
    printf("| %.9ld |   %5d |   %5d |   %5d |", trips[i].MMSI,
      trips[i].no_records, trips[i].no_trip_instants, trips[i].no_SOG_instants);
    if (trips[i].trip != NULL)
    {
      printf(" %15.6lf |", tpointseq_length(trips[i].trip));
    }
    else
      printf("        ---      |");

    if (trips[i].SOG != NULL)
    {
      printf(" %13.6lf |\n", tnumberseq_twavg(trips[i].SOG));
    }
    else
      printf("       ---     |\n");
  }
  printf("-----------------------------------------------------------------------------\n");
  printf("\n%d records read.\n%d erroneous records ignored.\n", no_records,
    no_err_records);
  printf("%d trips read.\n", no_ships);

  /* Calculate the elapsed time */
  t = clock() - t;
  double time_taken = ((double) t) / CLOCKS_PER_SEC;
  printf("The program took %f seconds to execute\n", time_taken);

  /* State that the program executed successfully */
  exit_value = 0;

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
