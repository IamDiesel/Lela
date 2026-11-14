// ==============================================================================
// MapProjection - Implementierung (siehe Header)
// ==============================================================================

#include "MapProjection.h"
#include "MapTileConfig.h"
#include <math.h>

// Grenze der Web-Mercator-Projektion: atan(sinh(pi)) in Grad.
static const double MERCATOR_MAX_LAT = 85.05112878;

// Erdumfang am Aequator in Metern (WGS84).
static const double EARTH_CIRCUMFERENCE_M = 40075016.686;

static double clampLat(double lat) {
    if (lat >  MERCATOR_MAX_LAT) return  MERCATOR_MAX_LAT;
    if (lat < -MERCATOR_MAX_LAT) return -MERCATOR_MAX_LAT;
    return lat;
}

// Weltbreite in Pixeln bei Zoomstufe z.
static double worldSizePx(uint8_t zoom) {
    return (double)MAP_TILE_PX * (double)MapProjection_TilesPerAxis(zoom);
}

uint32_t MapProjection_TilesPerAxis(uint8_t zoom) {
    return 1UL << zoom;
}

MapPoint MapProjection_LatLonToPixel(double lat, double lon, uint8_t zoom) {
    const double size   = worldSizePx(zoom);
    const double latRad = clampLat(lat) * M_PI / 180.0;

    MapPoint p;
    p.x = (lon + 180.0) / 360.0 * size;
    p.y = (1.0 - log(tan(latRad) + 1.0 / cos(latRad)) / M_PI) / 2.0 * size;
    return p;
}

void MapProjection_PixelToLatLon(const MapPoint& p, uint8_t zoom, double& lat, double& lon) {
    const double size = worldSizePx(zoom);
    lon = p.x / size * 360.0 - 180.0;
    const double n = M_PI * (1.0 - 2.0 * p.y / size);
    lat = atan(sinh(n)) * 180.0 / M_PI;
}

double MapProjection_MetersPerPixel(double lat, uint8_t zoom) {
    return EARTH_CIRCUMFERENCE_M * cos(clampLat(lat) * M_PI / 180.0) / worldSizePx(zoom);
}

void MapProjection_TileRange(const MapPoint& topLeft, const MapPoint& bottomRight, uint8_t zoom,
                             uint32_t& x0, uint32_t& y0, uint32_t& x1, uint32_t& y1) {
    const double maxTile = (double)MapProjection_TilesPerAxis(zoom) - 1.0;

    auto toTile = [maxTile](double px) -> uint32_t {
        double t = floor(px / (double)MAP_TILE_PX);
        if (t < 0.0) t = 0.0;
        if (t > maxTile) t = maxTile;
        return (uint32_t)t;
    };

    x0 = toTile(topLeft.x);
    y0 = toTile(topLeft.y);
    x1 = toTile(bottomRight.x);
    y1 = toTile(bottomRight.y);
}
