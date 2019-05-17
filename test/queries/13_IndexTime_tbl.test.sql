﻿-------------------------------------------------------------------------------

DROP INDEX IF EXISTS tbl_timestampset_big_gist_idx;
DROP INDEX IF EXISTS tbl_timestampset_big_spgist_idx;

DROP INDEX IF EXISTS tbl_period_big_gist_idx;
DROP INDEX IF EXISTS tbl_period_big_spgist_idx;

DROP INDEX IF EXISTS tbl_periodset_big_gist_idx;
DROP INDEX IF EXISTS tbl_periodset_big_spgist_idx;

-------------------------------------------------------------------------------

CREATE INDEX tbl_timestampset_big_gist_idx ON tbl_timestampset_big USING GIST(ts);
CREATE INDEX tbl_period_big_gist_idx ON tbl_period_big USING GIST(p);
CREATE INDEX tbl_periodset_big_gist_idx ON tbl_periodset_big USING GIST(ps);

SELECT count(*) FROM tbl_timestampset_big WHERE ts && period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_timestampset_big WHERE ts @> period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_timestampset_big WHERE ts <@ period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_timestampset_big WHERE ts <<# period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_timestampset_big WHERE ts &<# period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_timestampset_big WHERE ts #>> period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_timestampset_big WHERE ts #&> period '[2001-01-01, 2001-07-01]';

SELECT count(*) FROM tbl_period_big WHERE p && period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_period_big WHERE p @> period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_period_big WHERE p <@ period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_period_big WHERE p <<# period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_period_big WHERE p &<# period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_period_big WHERE p #>> period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_period_big WHERE p #&> period '[2001-01-01, 2001-07-01]';

SELECT count(*) FROM tbl_periodset_big WHERE ps && period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_periodset_big WHERE ps @> period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_periodset_big WHERE ps <@ period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_periodset_big WHERE ps <<# period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_periodset_big WHERE ps &<# period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_periodset_big WHERE ps #>> period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_periodset_big WHERE ps #&> period '[2001-01-01, 2001-07-01]';

DROP INDEX IF EXISTS tbl_timestampset_big_gist_idx;
DROP INDEX IF EXISTS tbl_period_big_gist_idx;
DROP INDEX IF EXISTS tbl_periodset_big_gist_idx;

-------------------------------------------------------------------------------

CREATE INDEX tbl_timestampset_big_spgist_idx ON tbl_timestampset_big USING SPGIST(ts);
CREATE INDEX tbl_period_big_spgist_idx ON tbl_period_big USING SPGIST(p);
CREATE INDEX tbl_periodset_big_spgist_idx ON tbl_periodset_big USING SPGIST(ps);

SELECT count(*) FROM tbl_timestampset_big WHERE ts && period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_timestampset_big WHERE ts @> period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_timestampset_big WHERE ts <@ period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_timestampset_big WHERE ts <<# period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_timestampset_big WHERE ts &<# period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_timestampset_big WHERE ts #>> period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_timestampset_big WHERE ts #&> period '[2001-01-01, 2001-07-01]';

SELECT count(*) FROM tbl_period_big WHERE p && period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_period_big WHERE p @> period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_period_big WHERE p <@ period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_period_big WHERE p <<# period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_period_big WHERE p &<# period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_period_big WHERE p #>> period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_period_big WHERE p #&> period '[2001-01-01, 2001-07-01]';

SELECT count(*) FROM tbl_periodset_big WHERE ps && period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_periodset_big WHERE ps @> period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_periodset_big WHERE ps <@ period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_periodset_big WHERE ps <<# period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_periodset_big WHERE ps &<# period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_periodset_big WHERE ps #>> period '[2001-01-01, 2001-07-01]';
SELECT count(*) FROM tbl_periodset_big WHERE ps #&> period '[2001-01-01, 2001-07-01]';

DROP INDEX IF EXISTS tbl_timestampset_big_spgist_idx;
DROP INDEX IF EXISTS tbl_period_big_spgist_idx;
DROP INDEX IF EXISTS tbl_periodset_big_spgist_idx;

-------------------------------------------------------------------------------

