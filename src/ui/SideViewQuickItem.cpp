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
#include <QPainter>
#include <QElapsedTimer>
#include <QFont>
#include <QDebug>
#include <QPen>
#include <QPainterStateGuard>
#include <QPoint>
#include <QRect>
#include <QVector>
#include <QtConcurrent>
#include <QMutex>
#include <QMutexLocker>
#include <set>

#include "GlobalObject.h"
#include "Navigator.h"
#include "PositionProvider.h"
#include "PositionInfo.h"
#include "GeoMapProvider.h"
#include "SideViewQuickItem.h"

Ui::SideViewQuickItem::SideViewQuickItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    connect(GlobalObject::positionProvider(), &Positioning::PositionProvider::positionInfoChanged, this, &QQuickItem::update);
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
}

void Ui::SideViewQuickItem::paint(QPainter *painter)
{
    QElapsedTimer timer;
    timer.start();

    // The Qt coordinate system starts at the top left corner, with
    // y pointing downwards.
    // Our world's coordinate system should start at the bottom left corner,
    // with y pointing upwards.
    painter->scale(1, -1);
    painter->translate(0, -widgetHeight());

    // Collect information:
    if (GlobalObject::navigator()->flightRoute() == nullptr) {
        // TODO Show error message
        return;
    } else {
        route = GlobalObject::navigator()->flightRoute();
    }

    drawSky(painter);

    const auto navigator = GlobalObject::navigator();
    const auto route = navigator->flightRoute();

    drawTerrain(painter);

    const auto borders = intersectAirspaces();
    drawAirspaces(painter, borders);

    // Draw airspace lower and upper bounds
    // Airspace label
    // Insert waypoints and waypoints along the way (?)
    // Plane symbol
    // Weather
    // Zoom + Move
    // Show related position on map
    // NOTAM
    // Scala

    qDebug() << "Drawing took" << timer.elapsed() << "milliseconds"; //TODO Remove
}

std::vector<QGeoCoordinate> Ui::SideViewQuickItem::getDrawpoints() {
    std::vector<QGeoCoordinate> points;
    for (double m = 0; m < route->lengthM(); m += hMeterPerPx) {
        auto position = route->positionAtTrackM(m);
        points.push_back(position);
    }
    return points;
}

/*void Ui::SideViewQuickItem::drawNoTrackAvailable(QPainter *painter)
{
    drawSky(painter);

    // Define gradient for the ground
    QLinearGradient groundGradient(0, widgetHeight() * 0.8, 0, widgetHeight());
    groundGradient.setColorAt(0.0, QColor(139, 69, 19));   // SaddleBrown at the top
    groundGradient.setColorAt(1.0, QColor(210, 180, 140)); // Tan at the bottom

    // Fill the ground with the ground gradient
    painter->fillRect(0, widgetHeight() * 0.8, widgetWidth(), widgetHeight(), groundGradient);

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
    painter->fillRect(0, 0, widgetWidth(), widgetHeight(), sky);
}

void Ui::SideViewQuickItem::drawTerrain(QPainter *painter) {
    const auto coords = getDrawpoints();
    elevations = std::vector<int>(coords.size());

    QList<QPoint> surface;
    int x = 0;
    for (const auto& coord : coords) {
        auto elevation = GlobalObject::geoMapProvider()->terrainElevationAMSL(coord).toFeet();
        elevations[x] = elevation;
        int y = elevation / vFtPerPx;
        if (y < 0) y = 0;  // TODO is this okay? Or are there significant points below 0 ft?
        surface.append(QPoint(x, y));
        x++;
    }

    QPainterStateGuard guard(painter);

    // Draw the terrain surface with a dark color:
    painter->setPen(QColor(67, 54, 37));
    painter->drawPolyline(surface.data(), surface.size());

    // Fill the ground below with a light brown color:
    surface.insert(0, QPoint(0, 0));
    surface.append(QPoint(x - 1, 0));

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
        bool inside = airspace.polygon().contains(path[0]);
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
            for (std::size_t j = 1; j < perimeter.size(); j++) {
                const auto& pA = perimeter[j - 1];
                const auto& pB = perimeter[j];

                const auto intersection = intersect(rtA, rtB, pA, pB);
                if (intersection) {
                    int trackmeters = metersToRTA + rtA.distanceTo(*intersection);

                    if (inside) {
                        borders._leavingBorder = Ui::SideViewQuickItem::AirspaceVerticalBorder(*intersection, trackmeters);
                        result.push_back(borders);
                        borders = Ui::SideViewQuickItem::AirspaceVerticalBorders(airspace);
                        inside = false;
                    } else {
                        borders._enteringBorder = Ui::SideViewQuickItem::AirspaceVerticalBorder(*intersection, trackmeters);
                        inside = true;
                    }
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
  const QGeoCoordinate& a2, const QGeoCoordinate& b1, const QGeoCoordinate& b2) {
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
        const std::optional<QVector<QPoint>> entering,
        const std::optional<QVector<QPoint>> leaving) const {
    QVector<QPolygon> lines; // Consider replacing this with pointers or refs,
                             // to avoid copying the polygons. We only use it
                             // inside this function to draw, anyways.

    if (!entering && !leaving) {
        // Only in this case, we need to draw to separate polylines.
        lines.push_back(bottom);
        lines.push_back(top);
    } else if (leaving) {
        // Combine all borders into one polyline.
        QVector<QPoint> polyline;
        polyline.reserve(bottom.size() + top.size() + leaving->size() +
            (entering ? entering->size() : 0));

        // Append bottom (left to right) and right (bottom to top):
        polyline.append(bottom);
        polyline.append(*leaving);

        // Reverse append top (right to left):
        for (auto it = top.crbegin(); it != top.crend(); ++it) {
            polyline.append(*it);
        }

        // Reverse append entering (top to bottom), if it exists:
        if (entering) {
            for (auto it = entering->crbegin(); it != entering->crend(); ++it) {
                polyline.append(*it);
            }
        }

        lines.push_back(polyline);
    } else {
        // In this case, only entering exists.
        // Combine the three borders into one polyline.
        QVector<QPoint> polyline;
        polyline.reserve(top.size() + entering->size() + bottom.size());

        // Reverse append top (right to left):
        for (auto it = top.crbegin(); it != top.crend(); ++it) {
            polyline.append(*it);
        }

        // Reverse append entering (top to bottom):
        for (auto it = entering->crbegin(); it != entering->crend(); ++it) {
            polyline.append(*it);
        }

        // Append bottom (left to right):
        polyline.append(bottom);

        lines.push_back(polyline);
    }

    // Finally, draw the polyline(s):
    QPen pen(color);
    pen.setWidth(linewidth);
    if (dashPattern) pen.setDashPattern(*dashPattern);
    painter->setPen(pen);

    for (const QPolygon& poly : lines)
        painter->drawPolygon(poly);
}

/*void Ui::SideViewQuickItem::drawAirspaceVBorder(QPainter *painter,
        const Ui::SideViewQuickItem::AirspaceVerticalBorder& border,
        bool entering, const Ui::AirspaceStyle& style) {
    const int x = border._trackM / hMeterPerPx;

    // 1. Draw the offset border:
    if (style._offsetColor) {
        QColor clr = *style._offsetColor;
        clr.setAlphaF(style._offsetOpacity);
        QPen pen(clr);
        pen.setWidth(offsetWidth);
        painter->setPen(pen);
        int tempx = x;
        if (entering) tempx += offsetWidth/2;
        else tempx -= offsetWidth/2;
        painter->drawLine(tempx, 0, tempx, widgetHeight());
    }

    // 2. Draw the line:
    QPen pen(style._lineColor);
    if (style._dashPattern) {
        pen.setDashPattern(*style._dashPattern);
    }
    pen.setWidth(linewidth);
    painter->setPen(pen);
    painter->drawLine(x, 0, x, widgetHeight());
}*/

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
    const int offsetWidth = 6;  // FIXME have this somewhere configurable.
    const int linewidth = 2;

    const StyleManager styleManager;  // FIXME avoid costly relocation of this by using singletons.
    for (const auto& border : borders) {
        const auto& style = styleManager.getStyle(border._airspace.CAT());
        QPainterStateGuard guard(painter);

        std::optional<QVector<QPoint>> entering;
        std::optional<QVector<QPoint>> leaving;

        int xl = 0;
        if (border._enteringBorder)
            xl = border._enteringBorder->_trackM / hMeterPerPx;

        int xr = route->lengthM() / hMeterPerPx;
        if (border._leavingBorder)
            xr = border._leavingBorder->_trackM / hMeterPerPx;

        const auto bottom = getHBorder(border._airspace.lowerBound(), true, xl, xr);
        const auto top = getHBorder(border._airspace.upperBound(), false, xl, xr);

        if (border._enteringBorder)
            entering = QVector<QPoint>({bottom.front(), top.front()});

        if (border._leavingBorder)
            leaving = QVector<QPoint>({bottom.back(), top.back()});

        // Fill the airspace:
        if (style._fillColor) {
            // No line:
            painter->setPen(QColor("transparent"));

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
        }

        // Draw the inset outline:
        if (style._offsetColor) {
            // Create offset borders:
            QVector<QPoint> bO, tO;
            std::optional<QVector<QPoint>> eO, lO;

            bO.reserve(bottom.size());
            tO.reserve(top.size());

            // Offset them:
            for (const QPoint& pt : bottom)
                bO.append(QPoint(pt.x(), pt.y() + offsetWidth / 2));
            for (const QPoint& pt : top)
                tO.append(QPoint(pt.x(), pt.y() - offsetWidth / 2));

            if (entering) {
                eO.emplace();
                eO->reserve(entering->size());
                for (const QPoint& pt : *entering)
                    eO->append(QPoint(pt.x() + offsetWidth / 2, pt.y()));
            }

            if (leaving) {
                lO.emplace();
                lO->reserve(leaving->size());
                for (const QPoint& pt : *leaving)
                    lO->append(QPoint(pt.x() - offsetWidth / 2, pt.y()));
            }

            QColor clr = *style._offsetColor;
            clr.setAlphaF(style._offsetOpacity);
            drawAirspaceBorders(painter, clr, offsetWidth,
                std::nullopt, bO, tO, eO, lO);
        }

        // TODO label the airspace

        // Draw the borders:
        drawAirspaceBorders(painter, style._lineColor, linewidth,
            style._dashPattern, bottom, top, entering, leaving);
    }
}

int Ui::SideViewQuickItem::widgetHeight()
{
    return static_cast<int>(height());
}

int Ui::SideViewQuickItem::widgetWidth()
{
    return static_cast<int>(width());
}

Units::Distance Ui::SideViewQuickItem::pressureAltitude() {
    //return GlobalObject::positionProvider()->pressureAltitude();
    return GlobalObject::positionProvider()->positionInfo().trueAltitudeAMSL();
}


QPointF Ui::SideViewQuickItem::getPolygonCentroid(const QPolygonF &polygon)
{
    qreal centroid_x = 0.0, centroid_y = 0.0;
    qreal signedArea = 0.0;
    qreal x0 = 0.0, y0 = 0.0;  // Current vertex coordinates
    qreal x1 = 0.0, y1 = 0.0;  // Next vertex coordinates
    qreal a = 0.0;  // Partial signed area

    // For all vertices except last
    int i;
    for (i = 0; i < polygon.size() - 1; ++i)
    {
        x0 = polygon[i].x();
        y0 = polygon[i].y();
        x1 = polygon[i + 1].x();
        y1 = polygon[i + 1].y();
        a = x0 * y1 - x1 * y0;
        signedArea += a;
        centroid_x += (x0 + x1) * a;
        centroid_y += (y0 + y1) * a;
    }

    // Do last vertex separately to avoid performing an expensive modulo operation in each iteration
    x0 = polygon[i].x();
    y0 = polygon[i].y();
    x1 = polygon[0].x();
    y1 = polygon[0].y();
    a = x0 * y1 - x1 * y0;
    signedArea += a;
    centroid_x += (x0 + x1) * a;
    centroid_y += (y0 + y1) * a;

    signedArea *= 0.5;
    centroid_x /= (6 * signedArea);
    centroid_y /= (6 * signedArea);

    return QPointF(centroid_x, centroid_y);
}
