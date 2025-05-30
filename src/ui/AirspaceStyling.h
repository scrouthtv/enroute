#pragma once

#include <optional>
#include <QColor>
#include <QList>
#include <QVector>

#include "GlobalObject.h"

namespace Ui {

class AirspaceStyle {
  public:
  AirspaceStyle(QVector<QString> categories, QColor lineColor) :
    _categories(categories), _lineColor(lineColor) {}

  AirspaceStyle(QVector<QString> categories, QColor lineColor,
      QList<qreal> dashPattern) :
    _categories(categories), _lineColor(lineColor), _dashPattern(dashPattern) {}

  AirspaceStyle(QVector<QString> categories, QColor lineColor,
      std::optional<QList<qreal>> dashPattern,
      std::optional<QColor> offsetColor, float offsetOpacity,
      std::optional<QColor> fillColor, float fillOpacity) :
    _categories(categories), _lineColor(lineColor), _dashPattern(dashPattern),
    _offsetColor(offsetColor), _offsetOpacity(offsetOpacity),
    _fillColor(fillColor), _fillOpacity(fillOpacity) {}

  QVector<QString> _categories;

  QColor _lineColor;
  std::optional<QList<qreal>> _dashPattern;

  std::optional<QColor> _offsetColor;
  float _offsetOpacity;  // 0.0 - 1.0

  std::optional<QColor> _fillColor;
  float _fillOpacity;  // 0.0 - 1.0
};

class StyleManager : public GlobalObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

 public:
  StyleManager();
  const AirspaceStyle& getStyle(const QString& category) const;

 private:
  const QVector<AirspaceStyle> styles;
  const AirspaceStyle unk;
};

}  // namespace Ui
