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
  std::vector<QGeoCoordinate> getDrawpoints();
  void drawTerrain(QPainter *painter);
  std::vector<AirspaceVerticalBorders> intersectAirspaces();
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

  /**! \brief Draw evaluated borders of the airspace.
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
  void drawAirspaces(QPainter *painter,
    const std::vector<AirspaceVerticalBorders>& borders);

  /*void drawNoTrackAvailable(QPainter *painter);
  int getHighestElevation(std::vector<int> &elevations, const Positioning::PositionInfo &info, float defaultUpperLimit);
  std::vector<Airspace2D> get2dAirspaces(double track, float steps, float stepsBackwards, float stepSizeInMeter);
  std::vector<MergedAirspace2D> mergedAirspaces2D(std::vector<Airspace2D> airspaces, std::vector<int> &elevations, float steps, int highestElevation);
  std::vector<MergedAirspace2D> mergeAirspaces(std::vector<Airspace2D> mergedAirspaces);
  void drawAirspacesOutline(QPainter *painter, const MergedAirspace2D &mergedAirspaces2D);
  void drawAirspacesArea(QPainter *painter, const MergedAirspace2D &mergedAirspaces2D);
  void drawAirspacesLabel(QPainter *painter, const MergedAirspace2D &mergedAirspaces2D);
  void drawTerrain(QPainter *painter, const std::vector<int> &elevations, int highestElevation, float steps);
  QStringList airspaceSortedCategories();
  void drawAircraft(QPainter *painter, const Positioning::PositionInfo &info, int highestElevation, float steps, float stepsOffset);
  void drawCurrentHorizontalPosition(QPainter *painter, const Positioning::PositionInfo &info, float steps, float stepsBackwards);
  void drawFlightPath(QPainter *painter, const Positioning::PositionInfo &info, int highestElevation, float steps, float stepOffset);
  int yCoordinate(int altitude, int maxHeight, int objectHeight);*/

  int widgetHeight();
  int widgetWidth();
  Units::Distance pressureAltitude();
  QPointF getPolygonCentroid(const QPolygonF &polygon);
  Q_DISABLE_COPY_MOVE(SideViewQuickItem)

};

} // namespace Ui
