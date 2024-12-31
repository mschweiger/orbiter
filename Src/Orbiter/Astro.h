// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// =======================================================================
// Some general astronomical constants and functions

#ifndef __ASTRO_H
#define __ASTRO_H

#include <time.h>
#include "Vecmat.h"
#include "OrbiterAPI.h"

const double iAU = 1.0/AU;
// inverse of AU

const double parsec = AU/tan(1.0/3600.0*_RAD_);
// parsec (distance at which earth's orbital radius subtends 1")

const double iparsec = 1.0/parsec;
// inverse of parsec

const double Ggrav = 6.67259e-11;
// gravitational constant [m^3 kg^-1 s^-2]

const double MJD2000 = 51544.5;
// MJD date for epoch J2000

inline double E_grav (double M, double d2)
{ return Ggrav * M / d2; }
// magnitude of gravitation field strength caused by mass M at distance d,
// where d2 = d*d

const double day = (1.0/86400.0);
// Julian days/second

const double UTC_CT_diff = 66.184;
// Current difference between UTC (Coordinated Universal Time) and CT (Coordinate Time)
// This may change in the future if additional leap seconds are added to UTC.

inline double Day (double sec)
{ return sec * day; }
// Convert a time interval from seconds to days

inline double JD (time_t t)
{ return 2440587.5 + (double)t * day; }
// Return Julian Date (JD) for time "t" as returned by the "time"
// function (seconds since 00:00 January 1970 UTC)

inline double MJD (time_t t)
{ return 40587.0 + (double)t * day; }
// Return Modified Julian Date (MJD) for time "t" as returned
// by the "time" function (seconds since 00:00 January 1970 UTC)

inline double Jepoch2MJD (double J)
{ return (J-2000.0)*365.25 + MJD2000; }
// Convert a Julian epoch J into a Modified Julian Date

inline double MJD2Jepoch (double mjd)
{ return 2000.0 + (mjd-MJD2000)/365.25; }
// Convert a Modified Julian Date mjd to a Julian epoch

inline double JC2MJD (double jc)
{ return jc*36525.0+MJD2000; }
// Convert Julian Century for equinopticum 2000 into Modified Julian Date

inline double MJD2JC (double mjd)
{ return (mjd-MJD2000)/36525.0; }
// Convert Modified Julian Date mjd to Julian Century for equinopticum 2000

double date2mjd (struct tm *date);
// Convert a date and time (UT) into MJD format

struct tm *mjddate (double mjd);
// Converts MJD value into UT and returns in tm structure

char *DateStr (double mjd);

// *** Coordinate system transformations ***

double Obliquity (double jc);
// returns Earth's obliquity (rad) of ecliptic at Julian century jc

void Equ2Ecl (double cosob, double sinob, double ra, double dc, double &l, double &b);
// Convert equatorial coordinates ra,dc into ecliptic coordinates l,b
// given sin and cos of obliquity

void Ecl2Equ (double cosob, double sinob, double l, double b, double &ra, double &dc);
// Convert ecliptic coordinates l (longitude) and b (latitude) into
// equatorial coordinates ra (right ascension) and dc (declination)
// given sin and cos of obliquity

// *** Geographical routines ***

double Orthodome (double lng1, double lat1, double lng2, double lat2);
// given the equatorial coordinates (lng1,lat1) and (lng2,lat2) of
// two points 1 and 2 on the surface of a sphere this calculates the
// shortest (orthodome) distance between them (in rad)

void Orthodome (double lng1, double lat1, double lng2, double lat2,
				double &dist, double &dir);
// given the equatorial coordinates (lng1,lat1) and (lng2,lat2) of
// two points 1 and 2 on the surface of a sphere this calculates the
// shortest (orthodome) distance between them (in rad) and the compass
// direction of point 2 as seen from point 1

char *FloatStr (double f, int precision = 4);
// Formats floating point value f with given precision, using 'k', 'M' and 'G'
// suffix as required.

char *SciStr (double f, int precision = 4, char prefix = '\0');
// Formats floating point value f with given precision, using "*10^x" suffix
// notation as required

char *DistStr (double d, int precision = 4);
// as FloatStr, but uses 'AU' (astronomical units) instead of 'G'

#endif // !__ASTRO_H
