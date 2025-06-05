#pragma once

#include <QByteArray>
#include <QPainter>
#include <QSvgRenderer>

namespace Ui {

/**
 * \brief Helper functionality for icons based on svg pictures.
 *
 * Animated svgs are not supported!
 */
class Icon {
 public:
  Icon(const QString path, const int size);
  Icon(const QByteArray& bytes, const int size);

  void draw(QPainter* painter, QPoint anchor,
    QFlags<Qt::AlignmentFlag> align, qreal degree = 0) const;

  bool isValid() const;

 private:
  const int _size;
  mutable QSvgRenderer qsvgr;  // when calling QSvgRenderer::render(),
                               // actually, the SVG animation frame is
                               // increased. Therefore, also changing
                               // the state. However, we don't use
                               // animated svgs, so we don't care.
};

class IconManager {
 public:
  IconManager();
  ~IconManager();

  const Icon* aerodrome() const;
  const Icon* aerodromeGlider() const;
  const Icon* aerodromeGrass() const;
  const Icon* aerodromeInop() const;
  const Icon* aerodromeMil() const;
  const Icon* aerodromeMilGrass() const;
  const Icon* aerodromeMilPaved() const;
  const Icon* aerodromePaved() const;
  const Icon* aerodromeUl() const;
  const Icon* aerodromeWater() const;

  const Icon* waypoint() const;
  const Icon* waypointMRP() const;
  const Icon* waypointRP() const;
  const Icon* waypointDME() const;
  const Icon* waypointNDB() const;
  const Icon* waypointVOR() const;
  const Icon* waypointVORDME() const;
  const Icon* waypointVORTAC() const;

  const Icon* notam() const;

 private:
  const Icon fallback;

  const Icon* ad;
  const Icon* adGld;
  const Icon* adGrass;
  const Icon* adInop;
  const Icon* adMil;
  const Icon* adMilGrass;
  const Icon* adMilPaved;
  const Icon* adPaved;
  const Icon* adUl;
  const Icon* adWater;

  const Icon* wp;
  const Icon* mrp;
  const Icon* rp;
  const Icon* dme;
  const Icon* ndb;
  const Icon* vor;
  const Icon* vordme;
  const Icon* vortac;

  const Icon* warning;
};

}