/*****************************************************************************
 *
 * This MobilityDB code is provided under The PostgreSQL License.
 * Copyright (c) 2016-2023, Université libre de Bruxelles and MobilityDB
 * contributors
 *
 * MobilityDB includes portions of PostGIS version 3 source code released
 * under the GNU General Public License (GPLv2 or later).
 * Copyright (c) 2001-2023, PostGIS contributors
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
 * @file
 * @brief General functions for set values composed of an ordered list of
 * distinct values.
 */

#include "general/set.h"

/* C */
#include <assert.h>
/* PostgreSQL */
#include <postgres.h>
#include <utils/timestamp.h>
#if POSTGRESQL_VERSION_NUMBER >= 160000
  #include "varatt.h"
#endif
/* MEOS */
#include <meos.h>
#include <meos_internal.h>
#include "general/pg_types.h"
#include "general/tnumber_mathfuncs.h"
#include "general/type_out.h"
#include "general/type_parser.h"
#include "general/type_util.h"
#include "point/tpoint_out.h"
#include "point/tpoint_spatialfuncs.h"
#include "npoint/tnpoint_boxops.h"

/*****************************************************************************
 * General functions
 *****************************************************************************/

/**
 * @brief Ensure that a set value is of a set type
 */
void
ensure_set_has_type(const Set *s, meosType settype)
{
  if (s->settype != settype)
    elog(ERROR, "The set value must be of type %s", meostype_name(settype));
  return;
}

/**
 * @brief Ensure that the set arguments have the same type in order to be able
 * to apply operations to them
 */
void
ensure_same_set_type(const Set *s1, const Set *s2)
{
  if (s1->settype != s2->settype)
    elog(ERROR, "Operation on mixed set types: %s and %s",
      meostype_name(s1->settype), meostype_name(s2->settype));
  return;
}

/**
 * @brief Ensure that a set value has the same base type as the given one
 * @param[in] s Input value
 * @param[in] basetype Input base type
 */
void
ensure_same_set_basetype(const Set *s, meosType basetype)
{
  if (s->basetype != basetype)
    elog(ERROR, "Operation on mixed set and base types: %s and %s",
      meostype_name(s->settype), meostype_name(basetype));
  return;
}

/**
 * @brief Return the location of a value in a set using binary search.
 *
 * If the value is found, the index of the value is returned in the output
 * parameter. Otherwise, return a number encoding whether it is before, between
 * two values, or after the set.
 * For example, given a set composed of 3 values and a parameter
 * value, the result of the function is as follows:
 * @code
 *            0       1        2
 *            |       |        |
 * 1)    d^                            => loc = 0
 * 2)        d^                        => loc = 0
 * 3)            d^                    => loc = 1
 * 4)                    d^            => loc = 2
 * 5)                            d^    => loc = 3
 * @endcode
 *
 * @param[in] s Set
 * @param[in] d Value
 * @param[out] loc Location
 * @result Return true if the value is contained in the set
 */
bool
set_find_value(const Set *s, Datum d, int *loc)
{
  int first = 0;
  int last = s->count - 1;
  int middle = 0; /* make compiler quiet */
  while (first <= last)
  {
    middle = (first + last) / 2;
    Datum d1 = SET_VAL_N(s, middle);
    int cmp = datum_cmp(d, d1, s->basetype);
    if (cmp == 0)
    {
      *loc = middle;
      return true;
    }
    if (cmp < 0)
      last = middle - 1;
    else
      first = middle + 1;
  }
  *loc = middle;
  return false;
}

#if 0 /* not used */
/**
 * @ingroup libmeos_internal_setspan_accessor
 * @brief Return the location of a value in a ranked set (which is unordered)
 * using sequential search.
 * @note Contrary to function `set_find_value`, if the value is not found the
 * returned location is always 0.
 *
 * @param[in] s Set
 * @param[in] d Value
 * @param[out] loc Location of the value if found
 * @result Return true if the value is contained in the vecctor
 */
bool
rset_find_value(const Set *s, Datum d, int *loc)
{
  assert(s); assert(loc);
  for (int i = 0; i < s->count; i++)
  {
    Datum d1 = SET_VAL_N(s, i);
    if (datum_eq(d, d1, s->basetype))
    {
      *loc = i;
      return true;
    }
  }
  *loc = 0;
  return false;
}
#endif /* not used */

/*****************************************************************************
 * Input/output functions in string format
 *****************************************************************************/

/**
 * @ingroup libmeos_internal_setspan_inout
 * @brief Return a set from its Well-Known Text (WKT) representation.
 */
Set *
set_in(const char *str, meosType settype)
{
  assert(str);
  return set_parse(&str, settype);
}

#if MEOS
/**
 * @ingroup libmeos_setspan_inout
 * @brief Return a set from its Well-Known Text (WKT) representation.
 */
Set *
intset_in(const char *str)
{
  assert(str);
  return set_parse(&str, T_INTSET);
}

/**
 * @ingroup libmeos_setspan_inout
 * @brief Return a set from its Well-Known Text (WKT) representation.
 */
Set *
bigintset_in(const char *str)
{
  assert(str);
  return set_parse(&str, T_BIGINTSET);
}

/**
 * @ingroup libmeos_setspan_inout
 * @brief Return a set from its Well-Known Text (WKT) representation.
 */
Set *
floatset_in(const char *str)
{
  assert(str);
  return set_parse(&str, T_FLOATSET);
}

/**
 * @ingroup libmeos_setspan_inout
 * @brief Return a set from its Well-Known Text (WKT) representation.
 */
Set *
textset_in(const char *str)
{
  assert(str);
  return set_parse(&str, T_TEXTSET);
}

/**
 * @ingroup libmeos_setspan_inout
 * @brief Return a set from its Well-Known Text (WKT) representation.
 */
Set *
timestampset_in(const char *str)
{
  assert(str);
  return set_parse(&str, T_TSTZSET);
}

/**
 * @ingroup libmeos_setspan_inout
 * @brief Return a set from its Well-Known Text (WKT) representation.
 */
Set *
geomset_in(const char *str)
{
  assert(str);
  return set_parse(&str, T_GEOMSET);
}

/**
 * @ingroup libmeos_setspan_inout
 * @brief Return a set from its Well-Known Text (WKT) representation.
 */
Set *
geogset_in(const char *str)
{
  assert(str);
  return set_parse(&str, T_GEOGSET);
}
#endif /* MEOS */


/**
 * @brief Return true if the base type value is output enclosed into quotes.
 */
static bool
set_basetype_quotes(meosType type)
{
  /* Text values are output with quotes in the `basetype_out` function */
  if (type == T_TIMESTAMPTZ || spatial_basetype(type))
    return true;
  return false;
}

/**
 * @brief Return the output representation of a set given by a function.
 */
char *
set_out_fn(const Set *s, int maxdd, outfunc value_out)
{
  /* Ensure validity of the arguments */
  assert(s != NULL);
  ensure_non_negative(maxdd);

  char **strings = palloc(sizeof(char *) * s->count);
  size_t outlen = 0;
  for (int i = 0; i < s->count; i++)
  {
    Datum d = SET_VAL_N(s, i);
    strings[i] = value_out(d, s->basetype, maxdd);
    outlen += strlen(strings[i]) + 1;
  }
  bool quotes = set_basetype_quotes(s->basetype);
  return stringarr_to_string(strings, s->count, outlen, "", '{', '}', quotes,
    SPACES);
}

/**
 * @ingroup libmeos_internal_setspan_inout
 * @brief Return the Well-Known Text (WKT) representation of a s.
 */
char *
set_out(const Set *s, int maxdd)
{
  assert(s);
  return set_out_fn(s, maxdd, &basetype_out);
}

#if MEOS
/**
 * @ingroup libmeos_setspan_inout
 * @brief Output a set of integers.
*/
char *
intset_out(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_INTSET);
  return set_out(s, 0);
}

/**
 * @ingroup libmeos_setspan_inout
 * @brief Output a set of big integers.
*/
char *
bigintset_out(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_BIGINTSET);
  return set_out(s, 0);
}

/**
 * @ingroup libmeos_setspan_inout
 * @brief Output a set of floats.
*/
char *
floatset_out(const Set *s, int maxdd)
{
  assert(s);
  ensure_set_has_type(s, T_FLOATSET);
  return set_out(s, maxdd);
}

/**
 * @ingroup libmeos_setspan_inout
 * @brief Output a set of texts.
*/
char *
textset_out(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_TEXTSET);
  return set_out(s, 0);
}

/**
 * @ingroup libmeos_setspan_inout
 * @brief Output a set of timestamps.
*/
char *
timestampset_out(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_TSTZSET);
  return set_out(s, 0);
}

/**
 * @ingroup libmeos_setspan_inout
 * @brief Output a set of geometries.
*/
char *
geoset_out(const Set *s, int maxdd)
{
  assert(s);
  ensure_geoset_type(s->settype);
  return set_out(s, maxdd);
}
#endif /* MEOS */

/**
 * @ingroup libmeos_setspan_inout
 * @brief Return the Well-Known Text (WKT) representation of a geoset.
 * @sqlfunc asText()
 */
char *
geoset_as_text(const Set *s, int maxdd)
{
  assert(s);
  ensure_geoset_type(s->settype);
  return set_out_fn(s, maxdd, &wkt_out);
}

/**
 * @ingroup libmeos_setspan_inout
 * @brief Return the Extended Well-Known Text (EWKT) representation of a geoset.
 * @sqlfunc asEWKT()
 */
char *
geoset_as_ewkt(const Set *s, int maxdd)
{
  assert(s);
  ensure_geoset_type(s->settype);
  return set_out_fn(s, maxdd, &ewkt_out);
}

/*****************************************************************************
 * Constructor functions
 *****************************************************************************/

/**
 * @brief Return the size of a bounding box of a temporal type
 */
static size_t
set_bbox_size(meosType settype)
{
  if (alphanumset_type(settype))
    return 0;
  if (spatialset_type(settype))
    return sizeof(STBox);
  elog(ERROR, "unknown set_bbox_size function for set type: %d", settype);
  return 0; /* make compiler quiet */
}

/**
 * @brief Set a bounding box from an array of set values
 *
 * @param[in] values Values
 * @param[in] basetype Type of the values
 * @param[in] count Number of elements in the array
 * @param[out] box Bounding box
 */
void
valuearr_compute_bbox(const Datum *values, meosType basetype, int count,
  void *box)
{
  /* Currently, only geo set types have bounding box */
  assert(set_basetype(basetype));
  assert(! alphanum_basetype(basetype));
  if (geo_basetype(basetype))
    geoarr_set_stbox(values, count, (STBox *) box);
#if NPOINT
  else if (basetype == T_NPOINT)
   npointarr_set_stbox(values, count, (STBox *) box);
#endif
  else
    elog(ERROR, "unknown set type for computing bounding box: %d", basetype);
  return;
}

#ifdef DEBUG_BUILD
/**
 * @brief Return a pointer to the bounding box of a temporal sequence
 */
void *
SET_BBOX_PTR(const Set *s)
{
  return (void *)( ((char *) s) + DOUBLE_PAD(sizeof(Set)) );
}

/**
 * @brief Return a pointer to the offsets array of a set
 */
size_t *
SET_OFFSETS_PTR(const Set *s)
{
  return (size_t *)( ((char *) s) + DOUBLE_PAD(sizeof(Set)) +
    DOUBLE_PAD(s->bboxsize) );
}

/**
 * @ingroup libmeos_internal_setspan_accessor
 * @brief Return the n-th value of a set
 * @pre The argument @p index is less than the number of values in the set
 */
Datum
SET_VAL_N(const Set *s, int index)
{
  assert(s);
  assert(index >= 0);
  /* For base types passed by value */
  if (MEOS_FLAGS_GET_BYVAL(s->flags))
    return (SET_OFFSETS_PTR(s))[index];
  /* For base types passed by reference */
  return PointerGetDatum(
    /* start of data : start address + size of struct + size of bbox + */
    ((char *) s) + DOUBLE_PAD(sizeof(Set)) + DOUBLE_PAD(s->bboxsize) +
      /* offset array + offset */
      (sizeof(size_t) * s->maxcount) + (SET_OFFSETS_PTR(s))[index] );
}
#endif /* DEBUG_BUILD */

/**
 * @ingroup libmeos_internal_setspan_constructor
 * @brief Construct a set from an array of values enabling the data structure
 * to expand.
 *
 * The memory structure depends on whether the value is passed by value or
 * by reference. For example, the memory structure of a set with 2 values
 * passed by value and passed by reference are, respectively, as follows
 *
 * @code
 * ------------------------------------------------------------
 * Header | count | bboxsize | ( bbox )_X | Value_0 | Value_1 |
 * ------------------------------------------------------------
 * @endcode
 *
 * @code
 * ---------------------------------------------------------------------
 * | Header | count | bboxsize | ( bbox )_X | offset_0 | offset_1 | ...
 * ---------------------------------------------------------------------
 * --------------------------
 *  ... | Value_0 | Value_1 |
 * --------------------------
 * @endcode
 * where
 * - `Header` contains internal information (size, type identifiers, flags)
 * - `count` is the number of values
 * - `bboxsize` is the size of the bounding box
 * - `bbox` is the bounding box and `X` are unused bytes added for double
 *   padding.
 * - `offset_i` are offsets from the begining of the struct for the values
 *
 * @param[in] values Array of values
 * @param[in] count Number of elements in the array
 * @param[in] maxcount Maximum number of elements in the array
 * @param[in] basetype Base type
 * @param[in] ordered True for ordered sets
 * @sqlfunc intset(), bigintset(), floatset(), textset(), tstzset()
 */
Set *
set_make_exp(const Datum *values, int count, int maxcount, meosType basetype,
  bool ordered)
{
  assert(values);
  assert(maxcount >= count);

  bool hasz = false;
  bool geodetic = false;
  if (geo_basetype(basetype))
  {
    /* Ensure the spatial validity of the elements */
    GSERIALIZED *gs1 = DatumGetGserializedP(values[0]);
    int srid = gserialized_get_srid(gs1);
    hasz = (bool) FLAGS_GET_Z(gs1->gflags);
    geodetic = FLAGS_GET_GEODETIC(gs1->gflags);
    /* Test the validity of the values */
    for (int i = 0; i < count; i++)
    {
      /* Test that the geometry is not empty */
      GSERIALIZED *gs2 = DatumGetGserializedP(values[i]);
      ensure_point_type(gs2);
      ensure_same_srid(srid, gserialized_get_srid(gs2));
      ensure_same_dimensionality_gs(gs1, gs2);
      ensure_non_empty(gs2);
    }
  }

  /* Sort the values and remove duplicates */
  Datum *newvalues;
  int newcount;
  if (ordered && count > 1)
  {
  /* Sort the values and remove duplicates */
    newvalues = palloc(sizeof(Datum) * count);
    memcpy(newvalues, values, sizeof(Datum) * count);
    datumarr_sort(newvalues, count, basetype);
    newcount = datumarr_remove_duplicates(newvalues, count, basetype);
  }
  else
  {
    newvalues = (Datum *) values;
    newcount = count;
  }

  /* Get the bounding box size */
  meosType settype = basetype_settype(basetype);
  size_t bboxsize = DOUBLE_PAD(set_bbox_size(settype));

  /* Determine whether the values are passed by value or by reference  */
  int16 typlen;
  bool typbyval = basetype_byvalue(basetype);
  if (typbyval)
    /* For base values passed by value */
    typlen = DOUBLE_PAD(sizeof(Datum));
  else
    /* For base values passed by reference */
    typlen = basetype_length(basetype);

  /* Compute the size of the set for values passed by reference */
  size_t values_size = 0;
  if (! typbyval)
  {
    if (typlen == -1)
    {
      for (int i = 0; i < newcount; i++)
        /* VARSIZE_ANY is used for oblivious data alignment, see postgres.h */
        values_size += DOUBLE_PAD(VARSIZE_ANY(DatumGetPointer(newvalues[i])));
    }
    else
      values_size = DOUBLE_PAD(typlen) * newcount;
  }

#if MEOS
  /* Compute the total size for maxcount elements as a proportion of the size
   * of the count elements provided. Note that this is only an INITIAL
   * ESTIMATION. The functions adding elements to a set must verify BOTH
   * the maximum number of elements AND the remaining space for adding an
   * additional variable-length element of arbitrary size */
  if (count != maxcount)
    values_size = (double) values_size * (double) maxcount / (double) count;
#endif /* MEOS */

  /* Total size of the struct */
  size_t memsize = DOUBLE_PAD(sizeof(Set)) + DOUBLE_PAD(bboxsize) +
    (sizeof(size_t) * maxcount) + values_size;

  /* Create the Set */
  Set *result = palloc0(memsize);
  SET_VARSIZE(result, memsize);
  MEOS_FLAGS_SET_BYVAL(result->flags, typbyval);
  MEOS_FLAGS_SET_ORDERED(result->flags, ordered);
  if (geo_basetype(basetype))
  {
    MEOS_FLAGS_SET_X(result->flags, true);
    MEOS_FLAGS_SET_Z(result->flags, hasz);
    MEOS_FLAGS_SET_GEODETIC(result->flags, geodetic);
  }
  result->count = newcount;
  result->maxcount = maxcount;
  result->settype = settype;
  result->basetype = basetype;
  result->bboxsize = (int16) bboxsize;
  /* Copy the array of values */
  if (typbyval)
  {
    for (int i = 0; i < newcount; i++)
      (SET_OFFSETS_PTR(result))[i] = newvalues[i];
  }
  else
  {
    /* Store the composing values */
    size_t pdata = DOUBLE_PAD(sizeof(Set)) + DOUBLE_PAD(bboxsize) +
      sizeof(size_t) * maxcount;
    size_t pos = 0;
    for (int i = 0; i < newcount; i++)
    {
      /* VARSIZE_ANY is used for oblivious data alignment, see postgres.h */
      size_t size_elem = (typlen == -1) ?
        VARSIZE_ANY(newvalues[i]) : (uint32) typlen;
      memcpy(((char *) result) + pdata + pos, DatumGetPointer(newvalues[i]),
        size_elem);
      (SET_OFFSETS_PTR(result))[i] = pos;
      pos += DOUBLE_PAD(size_elem);
    }
  }

  /* Compute the bounding box */
  if (bboxsize != 0)
    valuearr_compute_bbox(newvalues, basetype, newcount, SET_BBOX_PTR(result));

  if (ordered && count > 1)
    pfree(newvalues);
  return result;
}

/**
 * @ingroup libmeos_internal_setspan_constructor
 * @brief Construct a set from an array of values.
 * @param[in] values Array of values
 * @param[in] count Number of elements in the array
 * @param[in] basetype Base type
 * @param[in] ordered True for ordered sets
 * @sqlfunc intset(), bigintset(), floatset(), textset(), tstzset()
 */
Set *
set_make(const Datum *values, int count, meosType basetype, bool ordered)
{
  return set_make_exp(values, count, count, basetype, ordered);
}

#if MEOS
/**
 * @ingroup libmeos_setspan_constructor
 * @brief Construct an integer set from an array of values.
*/
Set *
intset_make(const int *values, int count)
{
  assert(values);
  assert(count > 0);
  Datum *datums = palloc(sizeof(Datum *) * count);
  for (int i = 0; i < count; ++i)
    datums[i] = Int32GetDatum(values[i]);
  return set_make(datums, count, T_INT4, ORDERED);
}

/**
 * @ingroup libmeos_setspan_constructor
 * @brief Construct a big integer set from an array of values.
*/
Set *
bigintset_make(const int64 *values, int count)
{
  assert(values);
  assert(count > 0);
  Datum *datums = palloc(sizeof(Datum *) * count);
  for (int i = 0; i < count; ++i)
    datums[i] = Int64GetDatum(values[i]);
  return set_make(datums, count, T_INT8, ORDERED);
}

/**
 * @ingroup libmeos_setspan_constructor
 * @brief Construct a float set from an array of values.
*/
Set *
floatset_make(const double *values, int count)
{
  assert(values);
  assert(count > 0);
  Datum *datums = palloc(sizeof(Datum *) * count);
  for (int i = 0; i < count; ++i)
    datums[i] = Float8GetDatum(values[i]);
  return set_make(datums, count, T_FLOAT8, ORDERED);
}

/**
 * @ingroup libmeos_setspan_constructor
 * @brief Construct a text set from an array of values.
*/
Set *
textset_make(const text **values, int count)
{
  assert(values);
  assert(count > 0);
  Datum *datums = palloc(sizeof(Datum *) * count);
  for (int i = 0; i < count; ++i)
    datums[i] = PointerGetDatum(values[i]);
  return set_make(datums, count, T_TEXT, ORDERED);
}

/**
 * @ingroup libmeos_setspan_constructor
 * @brief Construct a timestamp with time zone set from an array of values.
*/
Set *
timestampset_make(const TimestampTz *values, int count)
{
  assert(values);
  assert(count > 0);
  Datum *datums = palloc(sizeof(Datum *) * count);
  for (int i = 0; i < count; ++i)
    datums[i] = TimestampTzGetDatum(values[i]);
  return set_make(datums, count, T_TIMESTAMPTZ, ORDERED);
}

/**
 * @ingroup libmeos_setspan_constructor
 * @brief Construct a geometry set from an array of values.
*/
Set *
geomset_make(const GSERIALIZED **values, int count)
{
  assert(values);
  assert(count > 0);
  Datum *datums = palloc(sizeof(Datum *) * count);
  for (int i = 0; i < count; ++i)
    datums[i] = PointerGetDatum(values[i]);
  return set_make(datums, count, T_GEOMETRY, ORDERED);
}

/**
 * @ingroup libmeos_setspan_constructor
 * @brief Construct a geography set from an array of values.
*/
Set *
geogset_make(const GSERIALIZED **values, int count)
{
  assert(values);
  assert(count > 0);
  Datum *datums = palloc(sizeof(Datum *) * count);
  for (int i = 0; i < count; ++i)
    datums[i] = PointerGetDatum(values[i]);
  return set_make(datums, count, T_GEOGRAPHY, ORDERED);
}
#endif /* MEOS */

/**
 * @ingroup libmeos_internal_setspan_constructor
 * @brief Construct a set from the array of values and free the input array
 * after the creation.
 * @param[in] values Array of values
 * @param[in] count Number of elements in the array
 * @param[in] basetype Base type
 * @param[in] ordered True when the values are stored ordered
 */
Set *
set_make_free(Datum *values, int count, meosType basetype, bool ordered)
{
  assert(values);
  if (count == 0)
  {
    pfree(values);
    return NULL;
  }
  Set *result = set_make(values, count, basetype, ordered);
  pfree(values);
  return result;
}

/**
 * @ingroup libmeos_setspan_constructor
 * @brief Return a copy of a set.
 */
Set *
set_copy(const Set *s)
{
  assert(s);
  Set *result = palloc(VARSIZE(s));
  memcpy(result, s, VARSIZE(s));
  return result;
}

/*****************************************************************************
 * Cast functions
 *****************************************************************************/

/**
 * @ingroup libmeos_internal_setspan_cast
 * @brief Cast a value as a set
 * @sqlop @p ::
 */
Set *
value_to_set(Datum d, meosType basetype)
{
  return set_make(&d, 1, basetype, ORDERED);
}

#if MEOS
/**
 * @ingroup libmeos_setspan_cast
 * @brief Cast an integer as a set
 * @sqlop @p ::
 */
Set *
int_to_intset(int i)
{
  Datum v = Int32GetDatum(i);
  return set_make(&v, 1, T_INT4, ORDERED);
}

/**
 * @ingroup libmeos_setspan_cast
 * @brief Cast a big integer as a set
 * @sqlop @p ::
 */
Set *
bigint_to_bigintset(int64 i)
{
  Datum v = Int64GetDatum(i);
  return set_make(&v, 1, T_INT8, ORDERED);
}

/**
 * @ingroup libmeos_setspan_cast
 * @brief Cast a float as a set
 * @sqlop @p ::
 */
Set *
float_to_floatset(double d)
{
  Datum v = Float8GetDatum(d);
  return set_make(&v, 1, T_FLOAT8, ORDERED);
}

/**
 * @ingroup libmeos_setspan_cast
 * @brief Cast a text as a set
 * @sqlop @p ::
 */
Set *
text_to_textset(text *txt)
{
  assert(txt);
  Datum v = PointerGetDatum(txt);
  return set_make(&v, 1, T_TEXT, ORDERED);
}

/**
 * @ingroup libmeos_setspan_cast
 * @brief Cast a timestamp as a set
 * @sqlop @p ::
 */
Set *
timestamp_to_tstzset(TimestampTz t)
{
  Datum v = TimestampTzGetDatum(t);
  return set_make(&v, 1, T_TIMESTAMPTZ, ORDERED);
}

/**
 * @ingroup libmeos_setspan_cast
 * @brief Cast a text as a set
 * @sqlop @p ::
 */
Set *
geom_to_geomset(GSERIALIZED *gs)
{
  assert(gs);
  Datum v = PointerGetDatum(gs);
  return set_make(&v, 1, T_GEOMETRY, ORDERED);
}

/**
 * @ingroup libmeos_setspan_cast
 * @brief Cast a text as a set
 * @sqlop @p ::
 */
Set *
geog_to_geogset(GSERIALIZED *gs)
{
  assert(gs);
  Datum v = PointerGetDatum(gs);
  return set_make(&v, 1, T_GEOGRAPHY, ORDERED);
}
#endif /* MEOS */

/**
 * @ingroup libmeos_internal_setspan_accessor
 * @brief Set the last argument to the bounding span of a set.
 */
void
set_set_span(const Set *set, Span *s)
{
  assert(set);
  assert(s);
  span_set(SET_VAL_N(set, MINIDX), SET_VAL_N(set, set->MAXIDX), true, true,
    set->basetype, s);
  return;
}

/*****************************************************************************/

#if MEOS
/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the bounding box of a spatial set.
 * @sqlfunc stbox()
 * @sqlop @p ::
 */
STBox *
spatialset_stbox(const Set *s)
{
  assert(s);
  ensure_spatialset_type(s->settype);
  STBox *result = palloc(sizeof(STBox));
  spatialset_set_stbox(s, result);
  return result;
}
#endif /* MEOS */

/**
 * @ingroup libmeos_internal_setspan_accessor
 * @brief Set the last argument to the bounding box of a spatial set.
 */
void
spatialset_set_stbox(const Set *s, STBox *box)
{
  assert(s);
  assert(spatialset_type(s->settype));
  memset(box, 0, sizeof(STBox));
  memcpy(box, SET_BBOX_PTR(s), sizeof(STBox));
  return;
}

/*****************************************************************************
 * Accessor functions
 *****************************************************************************/

#if MEOS
/**
 * @ingroup libmeos_internal_setspan_accessor
 * @brief Return the size in bytes of a set.
 * @sqlfunc memSize()
 */
int
set_mem_size(const Set *s)
{
  assert(s);
  return (int) VARSIZE(DatumGetPointer(s));
}
#endif /* MEOS */

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the bounding span of a set.
 * @sqlfunc span()
 * @sqlop @p ::
 */
Span *
set_span(const Set *s)
{
  assert(s);
  ensure_set_spantype(s->settype);
  Span *result = palloc(sizeof(Span));
  set_set_span(s, result);
  return result;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the number of values of a set.
 * @sqlfunc numTimestamps()
 */
int
set_num_values(const Set *s)
{
  assert(s);
  return s->count;
}

/**
 * @ingroup libmeos_internal_setspan_accessor
 * @brief Return the start value of a set.
 * @sqlfunc startTimestamp()
 */
Datum
set_start_value(const Set *s)
{
  assert(s);
  return SET_VAL_N(s, 0);
}

#if MEOS
/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the start value of an integer set.
 * @sqlfunc startValue()
 */
int
intset_start_value(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_INTSET);
  int result = DatumGetInt32(SET_VAL_N(s, 0));
  return result;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the start value of a big integer set.
 * @sqlfunc startValue()
 */
int64
bigintset_start_value(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_BIGINTSET);
  int64 result = DatumGetInt64(SET_VAL_N(s, 0));
  return result;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the start value of a float set.
 * @sqlfunc startValue()
 */
double
floatset_start_value(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_FLOATSET);
  double result = DatumGetFloat8(SET_VAL_N(s, 0));
  return result;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the start value of a text set.
 * @sqlfunc startValue()
 */
text *
textset_start_value(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_TEXTSET);
  text *result = DatumGetTextP(SET_VAL_N(s, 0));
  return result;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the start value of a timestamp set.
 * @sqlfunc startTimestamp()
 */
TimestampTz
timestampset_start_timestamp(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_TSTZSET);
  TimestampTz result = DatumGetTimestampTz(SET_VAL_N(s, 0));
  return result;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the start value of a geo set.
 * @sqlfunc startValue()
 */
GSERIALIZED *
geoset_start_value(const Set *s)
{
  assert(s);
  ensure_geoset_type(s->settype);
  GSERIALIZED *result = DatumGetGserializedP(SET_VAL_N(s, 0));
  return result;
}
#endif /* MEOS */

/**
 * @ingroup libmeos_internal_setspan_accessor
 * @brief Return the end value of a set.
 * @sqlfunc endTimestamp()
 */
Datum
set_end_value(const Set *s)
{
  assert(s);
  return SET_VAL_N(s, s->count - 1);
}

#if MEOS
/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the end value of an integer set.
 * @sqlfunc endValue()
 */
int
intset_end_value(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_INTSET);
  int result = DatumGetInt32(SET_VAL_N(s, 0));
  return result;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the end value of a big integer set.
 * @sqlfunc endValue()
 */
int64
bigintset_end_value(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_BIGINTSET);
  int64 result = DatumGetInt64(SET_VAL_N(s, s->count - 1));
  return result;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the end value of a float set.
 * @sqlfunc endValue()
 */
double
floatset_end_value(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_FLOATSET);
  double result = DatumGetFloat8(SET_VAL_N(s, s->count - 1));
  return result;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the end value of a text set.
 * @sqlfunc startValue()
 */
text *
textset_end_value(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_TEXTSET);
  text *result = DatumGetTextP(SET_VAL_N(s, s->count - 1));
  return result;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the end value of a timestamp set.
 * @sqlfunc endTimestamp()
 */
TimestampTz
timestampset_end_timestamp(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_TSTZSET);
  TimestampTz result = DatumGetTimestampTz(SET_VAL_N(s, s->count - 1));
  return result;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the end value of a geo set.
 * @sqlfunc startValue()
 */
GSERIALIZED *
geoset_end_value(const Set *s)
{
  assert(s);
  ensure_geoset_type(s->settype);
  GSERIALIZED *result = DatumGetGserializedP(SET_VAL_N(s, s->count - 1));
  return result;
}
#endif /* MEOS */

/**
 * @ingroup libmeos_internal_setspan_accessor
 * @brief Compute the n-th value of a set
 * @param[in] s Set
 * @param[in] n Number
 * @param[out] result Timestamp
 * @result Return true if the value is found
 * @note It is assumed that n is 1-based
 * @sqlfunc valueN(), timestampN()
 */
bool
set_value_n(const Set *s, int n, Datum *result)
{
  assert(s); assert(result);
  if (n < 1 || n > s->count)
    return false;
  *result = SET_VAL_N(s, n - 1);
  return true;
}

#if MEOS
/**
 * @ingroup libmeos_setspan_accessor
 * @brief Compute the n-th value of an integer set
 * @param[in] s Integer set
 * @param[in] n Number
 * @param[out] result Value
 * @result Return true if the value is found
 * @note It is assumed that n is 1-based
 * @sqlfunc valueN()
 */
bool
intset_value_n(const Set *s, int n, int *result)
{
  assert(s); assert(result);
  ensure_set_has_type(s, T_INTSET);
  if (n < 1 || n > s->count)
    return false;
  *result = DatumGetInt32(SET_VAL_N(s, n - 1));
  return true;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Compute the n-th value of a big integer set
 * @param[in] s Integer set
 * @param[in] n Number
 * @param[out] result Value
 * @result Return true if the value is found
 * @note It is assumed that n is 1-based
 * @sqlfunc valueN()
 */
bool
bigintset_value_n(const Set *s, int n, int64 *result)
{
  assert(s); assert(result);
  ensure_set_has_type(s, T_BIGINTSET);
  if (n < 1 || n > s->count)
    return false;
  *result = DatumGetInt64(SET_VAL_N(s, n - 1));
  return true;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Compute the n-th value of a float set
 * @param[in] s Float set
 * @param[in] n Number
 * @param[out] result Value
 * @result Return true if the value is found
 * @note It is assumed that n is 1-based
 * @sqlfunc valueN()
 */
bool
floatset_value_n(const Set *s, int n, double *result)
{
  assert(s); assert(result);
  ensure_set_has_type(s, T_FLOATSET);
  if (n < 1 || n > s->count)
    return false;
  *result = DatumGetFloat8(SET_VAL_N(s, n - 1));
  return true;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Compute the n-th value of a text set
 * @param[in] s Float set
 * @param[in] n Number
 * @param[out] result Value
 * @result Return true if the value is found
 * @note It is assumed that n is 1-based
 * @sqlfunc valueN()
 */
bool
textset_value_n(const Set *s, int n, text **result)
{
  assert(s); assert(result);
  ensure_set_has_type(s, T_TEXTSET);
  if (n < 1 || n > s->count)
    return false;
  *result = DatumGetTextP(SET_VAL_N(s, n - 1));
  return true;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Compute the n-th value of a timestamp set
 * @param[in] s Timestamp set
 * @param[in] n Number
 * @param[out] result Timestamp
 * @result Return true if the timestamp is found
 * @note It is assumed that n is 1-based
 * @sqlfunc timestampN()
 */
bool
timestampset_timestamp_n(const Set *s, int n, TimestampTz *result)
{
  assert(s); assert(result);
  ensure_set_has_type(s, T_TSTZSET);
  if (n < 1 || n > s->count)
    return false;
  *result = DatumGetTimestampTz(SET_VAL_N(s, n - 1));
  return true;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Compute the n-th value of a geo set
 * @param[in] s Float set
 * @param[in] n Number
 * @param[out] result Value
 * @result Return true if the value is found
 * @note It is assumed that n is 1-based
 * @sqlfunc valueN()
 */
bool
geoset_value_n(const Set *s, int n, GSERIALIZED **result)
{
  assert(s); assert(result);
  ensure_geoset_type(s->settype);
  if (n < 1 || n > s->count)
    return false;
  *result = DatumGetGserializedP(SET_VAL_N(s, n - 1));
  return true;
}
#endif /* MEOS */

/**
 * @ingroup libmeos_internal_setspan_accessor
 * @brief Return the array of values of a set.
 * @sqlfunc values(), timestamps()
 */
Datum *
set_values(const Set *s)
{
  assert(s);
  Datum *result = palloc(sizeof(Datum) * s->count);
  for (int i = 0; i < s->count; i++)
    result[i] = SET_VAL_N(s, i);
  return result;
}

#if MEOS
/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the array of values of an integer set.
 * @sqlfunc values()
 */
int *
intset_values(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_INTSET);
  int *result = palloc(sizeof(int) * s->count);
  for (int i = 0; i < s->count; i++)
    result[i] = DatumGetInt32(SET_VAL_N(s, i));
  return result;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the array of values of a big integer set.
 * @sqlfunc values()
 */
int64 *
bigintset_values(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_BIGINTSET);
  int64 *result = palloc(sizeof(int64) * s->count);
  for (int i = 0; i < s->count; i++)
    result[i] = DatumGetInt64(SET_VAL_N(s, i));
  return result;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the array of values of a float set.
 * @sqlfunc values()
 */
double *
floatset_values(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_FLOATSET);
  double *result = palloc(sizeof(double) * s->count);
  for (int i = 0; i < s->count; i++)
    result[i] = DatumGetFloat8(SET_VAL_N(s, i));
  return result;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the array of values of a text set.
 * @sqlfunc values()
 */
text **
textset_values(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_TEXTSET);
  text **result = palloc(sizeof(text *) * s->count);
  for (int i = 0; i < s->count; i++)
    result[i] = DatumGetTextP(SET_VAL_N(s, i));
  return result;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the array of values of a timestamp set.
 * @sqlfunc timestamps()
 */
TimestampTz *
timestampset_values(const Set *s)
{
  assert(s);
  ensure_set_has_type(s, T_TSTZSET);
  TimestampTz *result = palloc(sizeof(TimestampTz) * s->count);
  for (int i = 0; i < s->count; i++)
    result[i] = DatumGetTimestampTz(SET_VAL_N(s, i));
  return result;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the array of values of a geo set.
 * @sqlfunc values()
 */
GSERIALIZED **
geoset_values(const Set *s)
{
  assert(s);
  ensure_geoset_type(s->settype);
  GSERIALIZED **result = palloc(sizeof(GSERIALIZED *) * s->count);
  for (int i = 0; i < s->count; i++)
    result[i] = DatumGetGserializedP(SET_VAL_N(s, i));
  return result;
}
#endif /* MEOS */

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the SRID of a geoset point.
 * @sqlfunc SRID()
 */
int
geoset_srid(const Set *s)
{
  assert(s);
  ensure_geoset_type(s->settype);
  GSERIALIZED *gs = DatumGetGserializedP(SET_VAL_N(s, 0));
  return gserialized_get_srid(gs);
}

/*****************************************************************************
 * Transformation functions
 *****************************************************************************/

/**
 * @ingroup libmeos_setspan_transf
 * @brief Set the precision of the float set to the number of decimal places.
 */
Set *
floatset_round(const Set *s, int maxdd)
{
  /* Ensure validity of the arguments */
  assert(s);
  ensure_set_has_type(s, T_FLOATSET);
  ensure_non_negative(maxdd);

  Set *result = set_copy(s);
  Datum size = Int32GetDatum(maxdd);
  for (int i = 0; i < s->count; i++)
    (SET_OFFSETS_PTR(result))[i] = datum_round_float(SET_VAL_N(s, i), size);
  return result;
}

/**
 * @ingroup libmeos_setspan_transf
 * @brief Set the precision of the coordinates to the number of decimal places.
 */
Set *
geoset_round(const Set *s, int maxdd)
{
  /* Ensure validity of the arguments */
  assert(s);
  ensure_geoset_type(s->settype);
  ensure_non_negative(maxdd);

  Datum *values = palloc(sizeof(Datum) * s->count);
  Datum size = Int32GetDatum(maxdd);
  for (int i = 0; i < s->count; i++)
  {
    Datum value = SET_VAL_N(s, i);
    values[i] = datum_round_geo(value, size);
  }
  Set *result = set_make(values, s->count, s->basetype, ORDERED);
  pfree(values);
  return result;
}

/*****************************************************************************
 * Modification functions
 *****************************************************************************/

/**
 * @ingroup libmeos_internal_setspan_transf
 * @brief Shift the values of set.
 */
Set *
set_shift(const Set *s, Datum shift)
{
  assert(s);
  assert(MEOS_FLAGS_GET_BYVAL(s->flags));
  Set *result = set_copy(s);
  for (int i = 0; i < s->count; i++)
    (SET_OFFSETS_PTR(result))[i] =
      datum_add(SET_VAL_N(s, i), shift, s->basetype);
  return result;
}

/**
 * @ingroup libmeos_setspan_transf
 * @brief Return a timestamp set shifted and/or scaled by the intervals
 * @sqlfunc shift(), tscale(), shiftTscale()
 */
Set *
timestampset_shift_tscale(const Set *s, const Interval *shift,
  const Interval *duration)
{
  assert(shift || duration);
  if (duration)
    ensure_valid_duration(duration);
  ensure_set_has_type(s, T_TSTZSET);
  Set *result = set_copy(s);

  /* Set the first and last instants */
  TimestampTz lower, lower1, upper, upper1;
  lower = lower1 = DatumGetTimestampTz(SET_VAL_N(s, 0));
  upper = upper1 = DatumGetTimestampTz(SET_VAL_N(s, s->count - 1));
  lower_upper_shift_tscale(shift, duration, &lower1, &upper1);
  (SET_OFFSETS_PTR(result))[0] = TimestampTzGetDatum(lower1);
  (SET_OFFSETS_PTR(result))[s->count - 1] = TimestampTzGetDatum(upper1);
  if (s->count > 1)
  {
    /* Shift and/or scale from the second to the penultimate instant */
    TimestampTz delta = 0; /* make compiler quiet */
    double scale = 0; /* make compiler quiet */
    if (shift != NULL)
      delta = lower1 - lower;
    if (duration != NULL)
      scale = (double) (upper1 - lower1) / (double) (upper - lower);
    for (int i = 1; i < s->count - 1; i++)
    {
      if (shift != NULL)
        (SET_OFFSETS_PTR(result))[i] += delta;
      if (duration != NULL)
        (SET_OFFSETS_PTR(result))[i] = lower1 +
          (TimestampTz) ((SET_VAL_N(result, i) - lower1) * scale);
    }
  }
  return result;
}

/*****************************************************************************
 * Functions for defining B-tree index
 *****************************************************************************/

/**
 * @ingroup libmeos_setspan_comp
 * @brief Return true if the first set is equal to the second one.
 * @note The internal B-tree comparator is not used to increase efficiency
 * @sqlop @p =
 */
bool
set_eq(const Set *s1, const Set *s2)
{
  assert(s1); assert(s2);
  ensure_same_set_type(s1, s2);
  if (s1->count != s2->count)
    return false;
  /* s1 and s2 have the same number of values */
  for (int i = 0; i < s1->count; i++)
  {
    Datum v1 = SET_VAL_N(s1, i);
    Datum v2 = SET_VAL_N(s2, i);
    if (datum_ne(v1, v2, s1->basetype))
      return false;
  }
  /* All values of the two sets are equal */
  return true;
}

/**
 * @ingroup libmeos_setspan_comp
 * @brief Return true if the first set is different from the
 * second one.
 * @note The internal B-tree comparator is not used to increase efficiency
 * @sqlop @p <>
 */
bool
set_ne(const Set *s1, const Set *s2)
{
  return ! set_eq(s1, s2);
}

/**
 * @ingroup libmeos_setspan_comp
 * @brief Return -1, 0, or 1 depending on whether the first set is less
 * than, equal, or greater than the second one.
 * @note Function used for B-tree comparison
 * @sqlfunc set_cmp()
 */
int
set_cmp(const Set *s1, const Set *s2)
{
  assert(s1); assert(s2);
  ensure_same_set_type(s1, s2);
  int count = Min(s1->count, s2->count);
  int result = 0;
  for (int i = 0; i < count; i++)
  {
    Datum v1 = SET_VAL_N(s1, i);
    Datum v2 = SET_VAL_N(s2, i);
    result = datum_cmp(v1, v2, s1->basetype);
    if (result)
      break;
  }
  /* The first count times of the two Set are equal */
  if (! result)
  {
    if (count < s1->count) /* s1 has more values than s2 */
      result = 1;
    else if (count < s2->count) /* s2 has more values than s1 */
      result = -1;
    else
      result = 0;
  }
  return result;
}

/**
 * @ingroup libmeos_setspan_comp
 * @brief Return true if the first set is less than the second one
 * @sqlop @p <
 */
bool
set_lt(const Set *s1, const Set *s2)
{
  return set_cmp(s1, s2) < 0;
}

/**
 * @ingroup libmeos_setspan_comp
 * @brief Return true if the first set is less than or equal to the
 * second one
 * @sqlop @p <=
 */
bool
set_le(const Set *s1, const Set *s2)
{
  return set_cmp(s1, s2) <= 0;
}

/**
 * @ingroup libmeos_setspan_comp
 * @brief Return true if the first set is greater than or equal to
 * the second one
 * @sqlop @p >=
 */
bool
set_ge(const Set *s1, const Set *s2)
{
  return set_cmp(s1, s2) >= 0;
}

/**
 * @ingroup libmeos_setspan_comp
 * @brief Return true if the first set is greater than the second one
 * @sqlop @p >
 */
bool
set_gt(const Set *s1, const Set *s2)
{
  return set_cmp(s1, s2) > 0;
}

/*****************************************************************************
 * Functions for defining hash index
 * The function reuses PostgreSQL approach for array types for combining the
 * hash of the elements.
 *****************************************************************************/

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the 32-bit hash of a set.
 * @sqlfunc hash()
 */
uint32
set_hash(const Set *s)
{
  assert(s);
  uint32 result = 1;
  for (int i = 0; i < s->count; i++)
  {
    Datum d = SET_VAL_N(s, i);
    uint32 value_hash = datum_hash(d, s->basetype);
    result = (result << 5) - result + value_hash;
  }
  return result;
}

/**
 * @ingroup libmeos_setspan_accessor
 * @brief Return the 64-bit hash of a set using a seed.
 * @sqlfunc hash_extended()
 */
uint64
set_hash_extended(const Set *s, uint64 seed)
{
  assert(s);
  uint64 result = 1;
  for (int i = 0; i < s->count; i++)
  {
    Datum d = SET_VAL_N(s, i);
    uint64 value_hash = datum_hash_extended(d, s->basetype, seed);
    result = (result << 5) - result + value_hash;
  }
  return result;
}

/*****************************************************************************/
