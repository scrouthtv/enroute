#include "AirspaceStyling.h"

namespace Ui {

StyleManager::StyleManager() :
  styles({
    Ui::AirspaceStyle(QVector<QString>{"UNK"}, QColor("black")),
    Ui::AirspaceStyle(QVector<QString>{"FIR", "FIS"}, QColor("green")),
    Ui::AirspaceStyle(QVector<QString>{"SUA"}, QColor("red"), QList<qreal>({4.0, 3.0})),

    Ui::AirspaceStyle(QVector<QString>{"GLD"},
        QColor("yellow"), std::nullopt,
        std::nullopt, 0, QColor("yellow"), 0.8),

    Ui::AirspaceStyle(QVector<QString>{"RMZ", "ATZ", "TIZ", "TIA"},
        QColor("blue"), QList<qreal>({3.0, 3.0}),
        std::nullopt, 0, QColor("blue"), 0.2),

    Ui::AirspaceStyle(QVector<QString>{"TMZ"}, QColor("black"), QList<qreal>({4.0, 3.0, 0.5, 3.0})),
    Ui::AirspaceStyle(QVector<QString>{"PJE"}, QColor("red"), QList<qreal>({4.0, 3.0})),

    Ui::AirspaceStyle(QVector<QString>{"A", "B", "C", "D"},
        QColor("blue"), std::nullopt,
        QColor("blue"), 0.2, std::nullopt, 0),

    Ui::AirspaceStyle(QVector<QString>{"E", "F", "G"}, QColor("blue")),

    Ui::AirspaceStyle(QVector<QString>{"CTR"},
        QColor("blue"), QList<qreal>({4.0, 3.0}),
        std::nullopt, 0, QColor("red"), 0.2),

    Ui::AirspaceStyle(QVector<QString>{"NRA"},
        QColor("green"), std::nullopt,
        QColor("green"), 0.2, std::nullopt, 0),

    Ui::AirspaceStyle(QVector<QString>{"DNG", "R", "P"},
        QColor("red"), QList<qreal>({4.0, 3.0}),
        QColor("red"), 0.2, std::nullopt, 0),
  }), unk(QVector<QString>{"UNK"}, QColor("black")) {}

// Taken from flightMap/osm-liberty.json, starting at line 1240.
// In the future, we should propably read that file at compile time.

const AirspaceStyle& StyleManager::getStyle(const QString& name) const {
  for (const auto& style : styles) {
      if (style._categories.contains(name)) {
          return style;
      }
  }

  qWarning() << "Cannot find airspace style for category" << name;
  return unk;
}

}  // namespace Ui
