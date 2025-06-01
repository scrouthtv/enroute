/***************************************************************************
 *   Copyright (C) 2019 by Stefan Kebekus                                  *
 *   stefan.kebekus@gmail.com                                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <QBrush>
#include <QDebug>
#include <QElapsedTimer>
#include <QFont>
#include <QGraphicsTextItem>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QPainterStateGuard>
#include <QPen>
#include <QPoint>
#include <QRect>
#include <QtConcurrent>
#include <QVector>
#include <set>

#include "GeoMapProvider.h"
#include "GlobalObject.h"
#include "GlobalSettings.h"
#include "Navigator.h"
#include "PositionProvider.h"
#include "PositionInfo.h"
#include "SideViewQuickItem.h"

// Qt does not even provide this function...
static bool pointInPolygon(const QGeoCoordinate& point, const QVector<QGeoCoordinate>& polygon) {
    int n = polygon.size();
    bool inside = false;

    if (n < 3) {
        return false; // A polygon must have at least 3 vertices
    }

    double x = point.longitude();
    double y = point.latitude();

    for (int i = 0, j = n - 1; i < n; j = i++) {
        double xi = polygon[i].longitude();
        double yi = polygon[i].latitude();
        double xj = polygon[j].longitude();
        double yj = polygon[j].latitude();

        // Check if the point is within the y-bounds of the edge
        bool intersect =
            ((yi > y) != (yj > y)) && // The edge crosses the horizontal line
            (x < (xj - xi) * (y - yi) / (yj - yi) + xi); // The intersection occurs to the right of 'point'

        if (intersect) {
            inside = !inside; // Flip inside status
        }
    }

    return inside;
}

Ui::SideViewQuickItem::SideViewQuickItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    // TODO why is this here? this breaks everything.
    // specifically, erasing and then redrawing does not work:
    // text objects aren't erased, and it is not possible to draw
    // any primitives after erasing.
    // TODO should I open a bug report for this???
    //setRenderTarget(QQuickPaintedItem::FramebufferObject);

    route = GlobalObject::navigator()->flightRoute();

    // We use this data:
    // - Base map (elevation)
    // - Route
    // - Airspaces
    // Whenever any of these change, we need to redraw.
    //connect(GlobalObject::geoMapProvider(),
        //&GeoMaps::GeoMapProvider::terrainMapTilesChanged,
        //this, &SideViewQuickItem::setDirty);

    connect(GlobalObject::globalSettings(),
        &GlobalSettings::airspaceAltitudeLimitChanged,
        this, &QQuickItem::update);
}

void Ui::SideViewQuickItem::getHScale() {
    // If possible, check which part of the route is visible (defined by
    // the start & end in track meters).
    int start, end;
    const int routeEnd = route->lengthM();

    switch (_mapBoundary.type()) {
        case QGeoShape::PathType: {
            const auto& path = static_cast<const QGeoPath>(_mapBoundary);
            const auto visible = visibleRouteSection(path.path());
            start = visible[0];
            end = visible[1]; }
            break;
        case QGeoShape::PolygonType: {
            const auto& polygon = static_cast<const QGeoPolygon>(_mapBoundary);
            const auto visible = visibleRouteSection(polygon.perimeter());
            start = visible[0];
            end = visible[1]; }
            break;
        case QGeoShape::RectangleType:
        case QGeoShape::CircleType:
        case QGeoShape::UnknownType:
        default:
            qWarning() << "Map boundary set from FlightMap is of unsupported type " << _mapBoundary.type();
            start = 0;
            end = routeEnd;
            break;
    }

    // Zoom in up to at most 10 m/px.
    const int minDistance = profileWidth() * 10;
    if (end - start < minDistance) {
        const int missing = minDistance - (end - start);
        const int spaceRight = routeEnd - end;
        const int spaceLeft = start;

        if (spaceLeft + spaceRight <= missing) {
            start = 0;
            end = routeEnd;
        } else if (spaceLeft < missing) {
            start = 0;
            end = minDistance;
        } else if (spaceRight < missing) {
            start = routeEnd - minDistance;
            end = routeEnd;
        } else {
            start -= missing/2;
            end += missing/2;
        }
    }

    // Apply the new viewport:
    if (hMeter0 != start || hMeterPerPx != (end - start)/profileWidth()) {
        hMeter0 = start;
        hMeterPerPx = (end - start)/profileWidth();
    }
}

void Ui::SideViewQuickItem::getVScale() {
    vFtPerPx = GlobalObject::globalSettings()->airspaceAltitudeLimit().toFeet() /
        (widgetHeight() - 2 * padding);
}

void Ui::SideViewQuickItem::paint(QPainter *painter)
{
    qDebug() << "Starting painting";
    QElapsedTimer timer;
    timer.start();

    // Get the current route:
    if (GlobalObject::navigator()->flightRoute() == nullptr) {
        // TODO Show error message
        qWarning() << "No route";
        return;
    } else if (GlobalObject::navigator()->flightRoute() != route) {
        route = GlobalObject::navigator()->flightRoute();
    }

    QPainterStateGuard guard(painter);

    QFont font("Roboto");
    font.setPixelSize(12);
    painter->setFont(font);

    getHScale();
    getVScale();

    // Clear the paint area:
    //painter->eraseRect(0, widgetHeight(), widgetWidth(), -widgetHeight());
    painter->fillRect(0, widgetHeight(), widgetWidth(), -widgetHeight(),
        QColor(0, 0, 0, 0xaa));

    // The Qt coordinate system starts at the top left corner, with
    // y pointing downwards.
    // Our world's coordinate system should start at the bottom left corner,
    // with y pointing upwards.
    // Furthermore, we shift our coordinate system to the right to create
    // some space for the vertical scale.
    painter->scale(1, -1);
    painter->translate(profileStart(), -widgetHeight() + padding);

    drawSky(painter);
    qDebug() << "Sky ok at " << timer.elapsed() << "ms";

    drawVScale(painter);

    drawTerrain(painter);
    qDebug() << "Terrain ok at " << timer.elapsed() << "ms";

    const auto borders = intersectAirspaces();
    drawAirspaces(painter, borders);

    // Test whether routes starting in an airspace work correctly.
    // Scale
    // Insert waypoints and waypoints along the way (?)
    // Plane symbol
    // Weather
    // Zoom + Move
    // Show related position on map
    // NOTAM

    qDebug() << "Drawing took" << timer.elapsed() << "milliseconds"; //TODO Remove
}

/*void Ui::SideViewQuickItem::drawNoTrackAvailable(QPainter *painter)
{
    // Draw a semi-transparent overlay
    painter->fillRect(0, 0, widgetWidth(), widgetHeight(), QColor(0, 0, 0, 50));

    // Set up the font
    QFont font = painter->font();
    font.setPixelSize(35); // Larger font size
    font.setBold(true);    // Bold font
    painter->setFont(font);

    // Set up text color
    painter->setPen(QPen(Qt::white));

    // Draw the text with shadow for better readability
    auto text = "Not sufficient data";
    painter->setPen(QPen(Qt::black));
    painter->drawText(1, 21, widgetWidth(), 40, Qt::AlignCenter, text);
    painter->setPen(QPen(Qt::white));
    painter->drawText(0, 20, widgetWidth(), 40, Qt::AlignCenter, text);
}*/

void Ui::SideViewQuickItem::drawSky(QPainter *painter)
{
    // Fill the background with a solid color:
    QColor sky(144, 213, 255);
    painter->fillRect(0, 0, profileWidth(), widgetHeight() - 2 * padding, sky);
}

void Ui::SideViewQuickItem::drawVScale(QPainter *painter) {
    // Possible scale steps:
    const int scaleSteps[] = {100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000};
    const int textHeight = 16;

    painter->setPen(QColorConstants::White);

    // Choose the first scale where the spacing between labels is
    // larger than their size:
    int scale;
    for (int s : scaleSteps) {
        // Label spacing = scale / vFtPerPx.
        if (s / vFtPerPx >= textHeight) {
            scale = s;
            break;
        }
    }

    // Draw the labels:
    int y = 0, height = 0;
    while (y < widgetHeight() - 2 * padding) {
        y = height / vFtPerPx;
        QFlags<Qt::AlignmentFlag> align = Qt::AlignRight;
        /*if (y == 0) align |= Qt::AlignBottom;
        else if (y >= widgetHeight() - 2 * padding) align |= Qt::AlignTop;
        else*/ align |= Qt::AlignVCenter;

        drawText(painter, -padding, y, QString("%1 ft").arg(height), align);
        height += scale;
    }
}

void Ui::SideViewQuickItem::drawTerrain(QPainter *painter) {
    elevations = std::vector<int>(widgetWidth());

    QList<QPoint> surface;
    for (int x = 0; x < profileWidth(); x++) {
        const auto coord = route->positionAtTrackM(hMeter0 + hMeterPerPx * x);
        const auto elevation = GlobalObject::geoMapProvider()->terrainElevationAMSL(coord).toFeet();
        elevations[x] = elevation;
        int y = elevation / vFtPerPx;
        if (y < 0) y = 0;  // TODO is this okay? Or are there significant points below 0 ft?
        surface.append(QPoint(x, y));
    }

    QPainterStateGuard guard(painter);

    // Draw the terrain surface with a dark color:
    painter->setPen(QColor(67, 54, 37));
    painter->drawPolyline(surface.data(), surface.size());

    // Fill the ground below with a light brown color:
    surface.insert(0, QPoint(0, 0));
    surface.append(QPoint(profileWidth(), 0));

    painter->setBrush(QColor(122, 98, 61));
    QPolygon poly(surface);
    painter->drawPolygon(poly);
}

std::vector<Ui::SideViewQuickItem::AirspaceVerticalBorders>
Ui::SideViewQuickItem::intersectAirspaces() {
    std::vector<Ui::SideViewQuickItem::AirspaceVerticalBorders> result;
    const auto path = route->geoPath();

    // Keep track of the track distance to each waypoint in the route
    // to avoid recalculating for every airspace:
    std::vector<int> trackMetersToWP;
    trackMetersToWP.push_back(0);

    // Get all relevant airspaces:
    const auto airspaces = GlobalObject::geoMapProvider()->airspaces(route->boundingRectangle());

    for (const auto& airspace : airspaces) {
        const auto& perimeter = airspace.polygon().perimeter();
        bool inside = pointInPolygon(path[0], perimeter);
        auto borders = Ui::SideViewQuickItem::AirspaceVerticalBorders(airspace);

        for (std::size_t i = 1; i < path.size(); i++) {
            const auto rtA = path[i - 1];
            const auto rtB = path[i];

            // Calculate and cache the track meters up to the current waypoint.
            if (trackMetersToWP.size() <= i) {
                int trackMeters = rtA.distanceTo(rtB);
                // If there were earlier legs, add their distance to the current leg:
                trackMeters += trackMetersToWP[i - 1];
                trackMetersToWP.push_back(trackMeters);
            }

            // Get distance to the start of the current leg, reusing the cached value:
            const int metersToRTA = trackMetersToWP[i - 1];

            // TODO Is the perimeter explicitely closed, i. e. is the first point the
            // same as the last point?
            // If this weren't the case, we should also check for intersection
            // with pA = perimeter[0] and pB = perimeter[end].

            // We need to first collect all intersections with this perimeter.
            // Afterwards, we sort these intersections by distance to the start.
            // Finally, we create a vertical border intersection for every one.
            QVector<QGeoCoordinate> intersections;
            for (std::size_t j = 1; j < perimeter.size(); j++) {
                const auto& pA = perimeter[j - 1];
                const auto& pB = perimeter[j];

                const auto intersection = intersect(rtA, rtB, pA, pB);
                if (intersection) intersections.push_back(*intersection);
            }

            // Sort intersections by distance to the start:
            std::sort(intersections.begin(), intersections.end(),
                      [this, rtA, rtB](const QGeoCoordinate& a, const QGeoCoordinate& b) {
                          return a.distanceTo(rtA) < b.distanceTo(rtB);
                      });

            // Create a vertical border for every intersection:
            for (const auto& intersection : intersections) {
                int trackmeters = metersToRTA + rtA.distanceTo(intersection);
                if (inside) {
                    borders._leavingBorder = Ui::SideViewQuickItem::AirspaceVerticalBorder(intersection, trackmeters);
                    result.push_back(borders);
                    borders = Ui::SideViewQuickItem::AirspaceVerticalBorders(airspace);
                    inside = false;
                } else {
                    borders._enteringBorder = Ui::SideViewQuickItem::AirspaceVerticalBorder(intersection, trackmeters);
                    inside = true;
                }
            }
        }

        // Add last border if we enter but do not leave the airspace at the end:
        if (inside && borders._enteringBorder) {
            result.push_back(borders);
        }
    }

    return result;
}

std::optional<QGeoCoordinate> Ui::SideViewQuickItem::intersect(const QGeoCoordinate& a1,
  const QGeoCoordinate& a2, const QGeoCoordinate& b1, const QGeoCoordinate& b2) const {
    // Implementation based on https://stackoverflow.com/a/1968345 for now:
    // latitude is y
    const double a_x = a2.longitude() - a1.longitude();
    const double a_y = a2.latitude() - a1.latitude();
    const double b_x = b2.longitude() - b1.longitude();
    const double b_y = b2.latitude() - b1.latitude();

    const double d = -b_x * a_y + a_x * b_y;

    if (d == 0) { // Lines are collinear
        return std::nullopt;
    }

    const double s = (-a_y * (a1.longitude() - b1.longitude()) + a_x * (a1.latitude() - b1.latitude())) / d;
    const double t = ( b_x * (a1.latitude() - b1.latitude()) - b_y * (a1.longitude() - b1.longitude())) / d;

    if (s >= 0 && s <= 1 && t >= 0 && t <= 1) {
        return QGeoCoordinate(a1.latitude() + (t * a_y), a1.longitude() + (t * a_x));
    }

    return std::nullopt; // No collision
}

Ui::SideViewQuickItem::AirspaceHorizontalBorder::AirspaceHorizontalBorder(const QString& boundary) {
    bool ok;
    if (boundary == "GND") {
        _heightF = 0;
        _isAGL = true;
        return;
    } else if (boundary.endsWith("AGL")) {
        _heightF = boundary.chopped(3).toInt(&ok);
        _isAGL = true;
    } else if (boundary.startsWith("FL")) {
        _heightF = boundary.sliced(2).toInt(&ok) * 100;
        _isAGL = false;
    } else {
        _heightF = boundary.toInt(&ok);
        _isAGL = false;
    }

    if (!ok) {
        throw std::runtime_error("Invalid airspace boundary '" +
            boundary.toStdString() + "'");
    }
}

void Ui::SideViewQuickItem::drawAirspaceBorders(QPainter *painter,
        const QColor& color, const int linewidth,
        const std::optional<QList<qreal>>& dashPattern,
        const QVector<QPoint>& bottom, const QVector<QPoint>& top,
        bool entering, bool leaving) const {
    QPen pen(color);
    pen.setWidth(linewidth);
    if (dashPattern) pen.setDashPattern(*dashPattern);
    painter->setPen(pen);

    if (!entering && !leaving) {
        // Only in this case, we need to draw to two separate polylines.
        painter->drawPolyline(bottom);
        painter->drawPolyline(top);
    } else if (leaving) {
        // Combine all borders into one polyline.
        QVector<QPoint> polyline;
        polyline.reserve(bottom.size() + top.size());

        // Append bottom (left to right):
        polyline.append(bottom);

        // Reverse append top (right to left):
        for (auto it = top.crbegin(); it != top.crend(); ++it)
            polyline.append(*it);

        // Close the polyline, if we also enter the airspace:
        if (entering) polyline.append(bottom.front());

        painter->drawPolyline(polyline);
    } else {
        // In this case, only entering exists.
        // Combine the three borders into one polyline.
        QVector<QPoint> polyline;
        polyline.reserve(top.size() + bottom.size());

        // Reverse append top (right to left):
        for (auto it = top.crbegin(); it != top.crend(); ++it)
            polyline.append(*it);

        // Don't need to append entering, because both points
        // are already in the bottom / top border.

        // Append bottom (left to right):
        polyline.append(bottom);

        painter->drawPolyline(polyline);
    }
}

QVector<QPoint>
Ui::SideViewQuickItem::getHBorder(const QString& boundary, bool lower,
const int xl, const int xr) {
    Ui::SideViewQuickItem::AirspaceHorizontalBorder border(0, false);
    try {
        border = Ui::SideViewQuickItem::AirspaceHorizontalBorder(boundary);
    } catch (const std::runtime_error& ex) {
        if (!lower) {
            // Move the upper border sufficiently (FL 600) upwards.
            border = Ui::SideViewQuickItem::AirspaceHorizontalBorder(600 * 100, false);
        }
        qWarning() << "Invalid airspace boundary: " << ex.what();
        qWarning() << "Defaulting to " << border._heightF << " ft.";
    }

    QVector<QPoint> points;

    if (border._isAGL) {
        auto elevation = elevations.cbegin() + xl;
        // TODO check whether we have elevation data for the whole track
        for (int x = xl; x <= xr; x++)
            points.push_back(QPoint(x, (border._heightF + *(elevation++)) / vFtPerPx));
    } else {
        points.push_back(QPoint(xl, border._heightF / vFtPerPx));
        points.push_back(QPoint(xr, border._heightF / vFtPerPx));
    }

    return points;
}

void Ui::SideViewQuickItem::drawAirspaces(QPainter *painter,
        const std::vector<Ui::SideViewQuickItem::AirspaceVerticalBorders>& borders) {
    const int offsetWidth = 6;
    const int linewidth = 2;

    const StyleManager styleManager;  // FIXME avoid costly initialisation of this by using singletons.
    for (auto border : borders) {
        const auto& style = styleManager.getStyle(border._airspace.CAT());

        bool entering = border._enteringBorder.has_value();
        bool leaving = border._leavingBorder.has_value();

        int xl = 0;
        if (entering)
            xl = (border._enteringBorder->_trackM - hMeter0) / hMeterPerPx;

        int xr = (route->lengthM() - hMeter0) / hMeterPerPx;
        if (leaving)
            xr = (border._leavingBorder->_trackM - hMeter0) / hMeterPerPx;

        if (xl < 0) {
            if (xr < 0) continue; // skip airspaces outside the drawing area.
            else {
                entering = false; // remove only this border.
                xl = 0;
            }
        }

        if (xr >= profileWidth()) {
            if (xl >= profileWidth()) continue; // skip airspaces outside the drawing area.
            else {
                leaving = false; // remove only this border.
                xr = profileWidth() - 1;
            }
        }

        const auto bottom = getHBorder(border._airspace.lowerBound(), true, xl, xr);
        const auto top = getHBorder(border._airspace.upperBound(), false, xl, xr);

        // Fill the airspace:
        if (style._fillColor) {
            // No line:
            painter->setPen(QColorConstants::Transparent);

            // Specify background color:
            QColor clr = *style._fillColor;
            clr.setAlphaF(style._fillOpacity);
            painter->setBrush(clr);

            // Merge the bottom and top to get the area defined by its borders:
            QVector<QPoint> area;
            area.reserve(bottom.size() + top.size());
            area.append(bottom);

            // Reverse append the top (right to left):
            for (auto it = top.crbegin(); it != top.crend(); ++it)
                area.append(*it);

            // Draw the area:
            painter->drawPolygon(area);

            // Reset the fill color:
            painter->setBrush(QColorConstants::Transparent);
        }

        // Draw the inset outline:
        if (style._offsetColor) {
            // Create offset borders:
            QVector<QPoint> bO, tO;

            bO.reserve(bottom.size());
            tO.reserve(top.size());

            // Offset them:
            for (const QPoint& pt : bottom)
                bO.append(QPoint(pt.x(), pt.y() + offsetWidth / 2));
            for (const QPoint& pt : top)
                tO.append(QPoint(pt.x(), pt.y() - offsetWidth / 2));

            QColor clr = *style._offsetColor;
            clr.setAlphaF(style._offsetOpacity);

            // Attempt to trim polyline sides according to entering, leaving
            // border.
            // If not possible, we simply fill the entire area with the offset
            // color
            if (trimBorder(&bO, entering, leaving, offsetWidth/2) &&
                trimBorder(&tO, entering, leaving, offsetWidth/2)) {
                // Draw the offset outline:
                drawAirspaceBorders(painter, clr, offsetWidth,
                    std::nullopt, bO, tO, entering, leaving);
            } else {
                // Fill the entire area with the offset color
                QPainterStateGuard guard(painter);
                painter->setPen(QColorConstants::Transparent);
                painter->setBrush(clr);

                // Merge the bottom and top to get the area defined by its borders:
                QVector<QPoint> area;
                area.reserve(bottom.size() + top.size());
                area.append(bottom);

                // Reverse append the top (right to left):
                for (auto it = top.crbegin(); it != top.crend(); ++it)
                    area.append(*it);

                // Draw the area:
                painter->drawPolygon(area);
            }
        }

        // Label the airspace.
        // For now, I will simply use the rectangle center,
        // as calculating the polygon centroid is too expensive.
        {
            const int cx = (xl + xr) / 2;
            const int cy = (bottom.front().y() + top.front().y() + bottom.back().y() + top.back().y()) / 4;
            const int w = xr - xl;

            // Height is the average height of the top and bottom borders:
            const int h = (top.front().y() - bottom.front().y() +
                top.back().y() - bottom.back().y()) / 2;

            painter->setPen(QColorConstants::Black);
            drawText(painter, cx, cy, border._airspace.CAT(), Qt::AlignCenter, w, h);
        }

        // Draw the borders:
        drawAirspaceBorders(painter, style._lineColor, linewidth,
            style._dashPattern, bottom, top, entering, leaving);
    }
}

bool Ui::SideViewQuickItem::trimBorder(QVector<QPoint>* border, bool left, bool right, int trimWidth) const {
    if (border->size() < 2)
        return false;

    // Remove elements that would protrude over the entering / leaving border.
    // We have to test first, whether there are enough elements to remove.
    // If there aren't enough elements, but the space between the first and
    // last element is large enough (straight line), we move the first and
    // last point over.
    // If not, the polyline degrades to a single vertical line, the
    // draw width of which needs to be adjusted accordingly.
    if (left && right) {
        if (border->size() > 2 * trimWidth) {
            for (int i = 0; i < trimWidth; i++) {
                border->pop_front();
                border->pop_back();
            }
            return true;
        } else if (border->front().x() + 2 * trimWidth < border->back().x()) {
            border->front().setX(border->front().x() + trimWidth);
            border->back().setX(border->back().x() - trimWidth);
            return true;
        } else return false;  // not enough space to trim.
    } else if (left) {
        if (border->size() > trimWidth) {
            for (int i = 0; i < trimWidth; i++)
                border->pop_front();
            return true;
        } else if (border->front().x() + trimWidth < border->back().x()) {
            border->front().setX(border->front().x() + trimWidth);
            return true;
        } else return false;  // not enough space to trim.
    } else if (right) {
        if (border->size() > trimWidth) {
            for (int i = 0; i < trimWidth; i++)
                border->pop_back();
            return true;
        } else if (border->front().x() + trimWidth < border->back().x()) {
            border->back().setX(border->back().x() - trimWidth);
            return true;
        } else return false;  // not enough space to trim.
    } else return true; // nothing to trim.
}

template <typename Iterator>
std::optional<int> Ui::SideViewQuickItem::intersect(Iterator route,
        const Iterator& routeEnd, const QVector<QGeoCoordinate>& mapBoundary) const {
    static_assert(std::is_same<typename std::iterator_traits<Iterator>::value_type, QGeoCoordinate>::value,
                  "Iterator's value type must be QGeoCoordinate");

    int trackM = 0;
    std::optional<int> result;
    for (; (route + 1) != routeEnd; ++route) {
        for (auto bound = mapBoundary.cbegin() + 1; bound != mapBoundary.cend(); ++bound) {
            const auto intersection = intersect(*route, *(route + 1), *(bound - 1), *bound);
            if (intersection) {
                // If there is an intersection, we only want it, if it is closer, than
                // the previous one.
                if (!result || intersection->distanceTo(*route) < *result)
                    result = intersection->distanceTo(*route);
            }
        }

        if (result) return *result + trackM;

        trackM += route->distanceTo(*(route + 1));
    }

    return std::nullopt;
}

std::array<int, 2>
Ui::SideViewQuickItem::visibleRouteSection(const QVector<QGeoCoordinate>& mapBoundary) {
    std::array<int, 2> result;

    if (pointInPolygon(route->geoPath().front(), mapBoundary)) {
        result[0] = 0;
    } else {
        const auto intersection = intersect(route->geoPath().cbegin(), route->geoPath().cend(), mapBoundary);
        if (intersection) result[0] = *intersection;
        else result[0] = 0;
    }

    if (pointInPolygon(route->geoPath().back(), mapBoundary)) {
        result[1] = route->lengthM();
    } else {
        const auto intersection = intersect(route->geoPath().crbegin(), route->geoPath().crend(), mapBoundary);
        if (intersection) result[1] = route->lengthM() - *intersection;
        else result[1] = route->lengthM();
    }

    return result;
}

void Ui::SideViewQuickItem::drawText(QPainter *painter, int x, int y,
        const QString& text, QFlags<Qt::AlignmentFlag> align, int w, int h) const {
    // We need to reset the scale to (1, 1) or the text itself will be
    // scaled ( = flipped) as well. However, first we need to move the
    // origin to the center of the text.
    painter->save();  // FIXME this saves a whole lot of uninteresting
                        // information, maybe we can save on performance??
                        // transform() does not give the original translation...
    painter->translate(x, y);
    painter->scale(1, -1);

    // TODO draw a white halo / border around the text.
    // This does not seem to be trivial, e.g.
    // https://stackoverflow.com/a/17517453

    // painter->drawText takes the top left corner, no matter which
    // orientation we use.
    // Therefore we have to calculate the top left corner:
    // The top left corner is calculated relative to (x, y) as we already translated
    // to that point.
    int left = 0;
    if (align & Qt::AlignHCenter) left -= w/2;
    else if (align & Qt::AlignRight) left -= w;

    int top = 0;
    if (align & Qt::AlignVCenter) top -= h/2;
    else if (align & Qt::AlignBottom) top -= h;

    painter->drawText(left, top, w, h, align, text);

    // Restore the earlier transformation:
    painter->restore();
}

int Ui::SideViewQuickItem::widgetHeight() const
{
    return static_cast<int>(height());
}

int Ui::SideViewQuickItem::widgetWidth() const
{
    return static_cast<int>(width());
}

int Ui::SideViewQuickItem::profileWidth() const
{
    return widgetWidth() - padding * 3 - scaleWidth;
}

int Ui::SideViewQuickItem::profileStart() const
{
    return 2 * padding + scaleWidth;
}

Units::Distance Ui::SideViewQuickItem::pressureAltitude() {
    //return GlobalObject::positionProvider()->pressureAltitude();
    return GlobalObject::positionProvider()->positionInfo().trueAltitudeAMSL();
}

void Ui::SideViewQuickItem::setMapBoundary(const QGeoShape& mapBoundary) {
    _mapBoundary = mapBoundary;

    update();
}
