/***************************************************************************
 *   Copyright (C) 2024 by Stefan Kebekus                                  *
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

#pragma once

#include <optional>
#include <QQmlEngine>
#include <QtQuick/QQuickPaintedItem>
#include <set>

#include "AirspaceStyling.h"
#include "FlightRoute.h"
#include "GlobalObject.h"
#include "PositionInfo.h"
#include "GeoMapProvider.h"

namespace Ui {

/*! \brief QML Class implementing a SideView
 */

class SideViewQuickItem : public QQuickPaintedItem
{
  Q_OBJECT
  QML_NAMED_ELEMENT(SideView)

public:
  /*! \brief Standard constructor
   *
   *  @param parent The standard QObject parent pointer
   */
  explicit SideViewQuickItem(QQuickItem *parent = nullptr);

  /*! \brief Re-implemented from QQuickPaintedItem to implement painting
   *
   *  @param painter Pointer to the QPainter used for painting
   */
  void paint(QPainter *painter) override;

private:
  class AirspaceVerticalBorder {
   public:
    AirspaceVerticalBorder(QGeoCoordinate intersection, int trackM) : _intersection(intersection), _trackM(trackM) {}
    QGeoCoordinate _intersection;
    int _trackM;
  };

  class AirspaceHorizontalBorder {
   public:
    /**! \brief Parse a horizontal (lower / upper) border from a string description.
     *
     * Strings may be of the form:
     *  - "GND" -> height = 0 ft, isAGL = true
     *  - "500 AGL" -> height = 500 ft, isAGL = true
     *  - "1000" -> height = 1000 ft, isAGL = false
     *  - "FL 070" -> height = 7000 ft, isAGL = false
     *
     * If the string is of unknown format, a std::runtime_error is thrown.
     */
    explicit AirspaceHorizontalBorder(const QString& boundary);
    AirspaceHorizontalBorder(const int heightF, bool isAGL) :
      _heightF(heightF), _isAGL(isAGL) {}

    int _heightF;
    bool _isAGL;
  };

  class AirspaceVerticalBorders {
   public:
    explicit AirspaceVerticalBorders(GeoMaps::Airspace airspace)
      : _airspace(airspace) {}

    GeoMaps::Airspace _airspace;
    std::optional<AirspaceVerticalBorder> _enteringBorder;
    std::optional<AirspaceVerticalBorder> _leavingBorder;
  };

  const float hMeterPerPx = 100;
  const float vFtPerPx = 100;

  Navigation::FlightRoute* route;
  std::vector<int> elevations;

  void drawSky(QPainter *painter);

  /*! \brief Get a list of coordinates where each pixel of the route profile
   * should be drawn.
   */
  std::vector<QGeoCoordinate> getDrawpoints();

  /*! \brief Draw the terrain.
   *
   * The list of draw points is evaluated. Afterwards, we retrieve the
   * elevation at each draw point from the GeoMapProvider.
   * Elevations below 0 ft are drawn as 0 ft.
   *
   * Finally, the ground polyline is drawn using a dark color,
   * as well es a the area below filled with light brown.
   */
  void drawTerrain(QPainter *painter);

  /*! \brief Find any airspaces our route intersects.
   *
   * Sections where the route is identical to the airspace boundary
   * are ignored and will therefore likely introduce errors into the
   * route profile. Keep in mind, that due to floating point limitations,
   * sections where the route is almost identical to the airspace boundary
   * are also ignored.
   *
   * TODO?
   *
   * @return A list of airspace intersections.
   */
  std::vector<AirspaceVerticalBorders> intersectAirspaces();

  /*! \brief Get the horizontal border (bottom or top) of an airspace.
   *
   * The border is evaluated as a polyline connecting the left to the right
   * corner. If the airspace border is specified above ground, this
   * polyline follows the terrain countour.
   *
   * The left and right side must be given as screen coordinates (in px).
   * Remember to divide track meters by hMeterPerPx.
   *
   * If the border cannot be parsed, a safe fallback value is used:
   * When looking for the lower border, 0 ft AGL, else FL 600 is returned.
   *
   * @param boundary Specification of the airspace boundary. For allowed
   * formats, see AirspaceHorizontalBorder::AirspaceHorizontalBorder.
   *
   * @param lower If true, the caller wants the lower
   * @param xl Left screen coordinate in px.
   * @param xr Right screen coordinate in px.
   */
  QVector<QPoint> getHBorder(const QString& boundary, bool lower,
    const int xl, const int xr);

  /*! \brief Intersect two lines and return the intersection point.
   *
   * Two straight lines are defined by their ends a1 -- a2 and b1 -- b2. When
   * calculating the intersection, only the latitude and longitude are used,
   * intersections may be at any altitude. The intersection point is returned if
   * the lines intersect. If the lines do not intersect, std::nullopt is
   * returned. If the lines are identical, std::nullopt is returned.
   *
   * @param a1 First end of the first line
   * @param a2 Second end of the first line
   * @param b1 First end of the second line
   * @param b2 Second end of the second line
   *
   * @returns Intersection point if the lines intersect, std::nullopt otherwise.
   */
  std::optional<QGeoCoordinate> intersect(const QGeoCoordinate& a1,
    const QGeoCoordinate& a2, const QGeoCoordinate& b1, const QGeoCoordinate& b2);

  /*! \brief Draw the borders of an airspace.
   *
   * @param bottom Bottom border with points sorted from left to right.
   * @param top Top border with points sorted from left to right.
   * @param entering Left border (if it exists) with points sorted from bottom to top.
   * @param leaving Right border (if it exists) with points sorted from bottom to top.
   */
  void drawAirspaceBorders(QPainter *painter,
    const QColor& color, const int linewidth,
    const std::optional<QList<qreal>>& dashPattern,
    const QVector<QPoint>& bottom, const QVector<QPoint>& top,
    const std::optional<QVector<QPoint>> entering,
    const std::optional<QVector<QPoint>> leaving) const;

  /*! \brief Draw the airspace borders.
   *
   * For each crossed airspace, the lower and upper borders are evaluated.
   * If the airspace starts before / enters after the plot, these borders
   * are not drawn.
   * All other borders are drawn.
   */
  void drawAirspaces(QPainter *painter,
    const std::vector<AirspaceVerticalBorders>& borders);

  int widgetHeight();
  int widgetWidth();
  Units::Distance pressureAltitude();
  Q_DISABLE_COPY_MOVE(SideViewQuickItem)
};

} // namespace Ui
