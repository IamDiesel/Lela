#pragma once
// ==============================================================================
// MapProjection - Web-Mercator und Kachelnummern
// ==============================================================================
// Rein rechnerisch: kennt weder Netz noch LVGL noch Home Assistant.
//
// Koordinatensystem "Weltpixel" je Zoomstufe z:
//   x = 0 .. 256 * 2^z  von West (-180 Grad) nach Ost (+180 Grad)
//   y = 0 .. 256 * 2^z  von Nord (+85,05 Grad) nach Sued (-85,05 Grad)
// Kachel (z, x, y) deckt die Weltpixel [256*x, 256*x+256) x [256*y, 256*y+256)
// ab - das Schema aller gaengigen Raster-Kachelanbieter ("slippy map").
//
// Pruefwert: Berlin (52.52, 13.405) liegt bei z=10 in Kachel (550, 335).
// ==============================================================================

#include <Arduino.h>

// Kachel als ein Wert, z.B. als Schluessel in Caches und Warteschlangen.
// Aufbau: z (8 Bit) | x (24 Bit) | y (24 Bit). Reicht bis Zoomstufe 24.
typedef uint64_t MapTileKey;

inline MapTileKey MapTileKey_Make(uint8_t z, uint32_t x, uint32_t y) {
    return ((uint64_t)z << 48) | ((uint64_t)(x & 0xFFFFFF) << 24) | (uint64_t)(y & 0xFFFFFF);
}
inline uint8_t  MapTileKey_Z(MapTileKey k) { return (uint8_t)(k >> 48); }
inline uint32_t MapTileKey_X(MapTileKey k) { return (uint32_t)((k >> 24) & 0xFFFFFF); }
inline uint32_t MapTileKey_Y(MapTileKey k) { return (uint32_t)(k & 0xFFFFFF); }

// Punkt in Weltpixeln einer Zoomstufe.
struct MapPoint {
    double x;
    double y;
};

// Anzahl Kacheln je Achse bei Zoomstufe z (2^z).
uint32_t MapProjection_TilesPerAxis(uint8_t zoom);

// Geografische Koordinate -> Weltpixel. Breiten jenseits von +-85,05 Grad
// werden begrenzt (Mercator ist dort nicht definiert).
MapPoint MapProjection_LatLonToPixel(double lat, double lon, uint8_t zoom);

// Weltpixel -> geografische Koordinate.
void MapProjection_PixelToLatLon(const MapPoint& p, uint8_t zoom, double& lat, double& lon);

// Meter je Pixel an einer Breite (fuer Genauigkeitskreise und Mengenabschaetzung).
double MapProjection_MetersPerPixel(double lat, uint8_t zoom);

// Kachelbereich, der ein Rechteck aus Weltpixeln abdeckt (Grenzen inklusive,
// auf gueltige Kachelnummern begrenzt).
void MapProjection_TileRange(const MapPoint& topLeft, const MapPoint& bottomRight, uint8_t zoom,
                             uint32_t& x0, uint32_t& y0, uint32_t& x1, uint32_t& y1);
