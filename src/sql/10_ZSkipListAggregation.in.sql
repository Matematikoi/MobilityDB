CREATE FUNCTION sl_test(tfloat[], period, tfloat[])
	RETURNS integer
	AS 'MODULE_PATHNAME', 'sl_test'
	LANGUAGE C IMMUTABLE PARALLEL SAFE;

	CREATE FUNCTION tavg_transfn2(internal, tfloat)
    RETURNS internal
    AS 'MODULE_PATHNAME', 'temporal_tavg_transfn2'
    LANGUAGE C IMMUTABLE PARALLEL SAFE;

CREATE FUNCTION tavg_combinefn2(internal, internal)
    RETURNS internal
    AS 'MODULE_PATHNAME', 'temporal_tavg_combinefn2'
    LANGUAGE C IMMUTABLE PARALLEL SAFE;

CREATE FUNCTION tavg_finalfn2(internal)
    RETURNS tfloat
    AS 'MODULE_PATHNAME', 'temporal_tavg_finalfn2'
    LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;


CREATE AGGREGATE tavg2(tfloat) (
    SFUNC = tavg_transfn2,
    STYPE = internal,
    COMBINEFUNC = tavg_combinefn2,
    FINALFUNC = tavg_finalfn2,
    PARALLEL = SAFE
);
