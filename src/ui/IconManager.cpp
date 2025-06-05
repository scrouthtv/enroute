#include <QString>

#include "IconManager.h"

using Qt::Literals::StringLiterals::operator""_s;

Ui::Icon::Icon(const QString path, const int size) : _size(size), qsvgr(path) {}

Ui::Icon::Icon(const QByteArray& bytes, const int size) : _size(size),
    qsvgr(bytes) {}

void Ui::Icon::draw(QPainter* painter, QPoint anchor,
  QFlags<Qt::AlignmentFlag> align, qreal degree) const {
  painter->save();

  // TODO scale? test alignment
  // TODO test whether we have to reset the rotation here as well?
  // TODO coloring / filling?

  anchor.setX(anchor.x() + _size / 2);
  if (align & Qt::AlignHCenter) anchor.setX(anchor.x() - _size / 2);
  else if (align & Qt::AlignRight) anchor.setX(anchor.x() - _size);

  anchor.setY(anchor.y() - _size / 2);
  if (align & Qt::AlignVCenter) anchor.setY(anchor.y() + _size / 2);
  else if (align & Qt::AlignBottom) anchor.setY(anchor.y() + _size);

  painter->translate(anchor);
  painter->scale(1, -1);

  if (degree != 0) {
    painter->rotate(degree);
  }

  qsvgr.render(painter, QRect(QPoint(-_size / 2, -_size/2), QSize(_size, _size)));

  painter->restore();
}

bool Ui::Icon::isValid() const {
  return qsvgr.isValid();
}

Ui::IconManager::IconManager() :
    fallback(QByteArray(R"(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<svg width="50mm" height="50mm" viewBox="0 0 50 50">
  <circle id="circle symbol"
      style="fill:#1000b0;stroke:#1000b0;stroke-width:6"
      cx="25" cy="25" r="16" />
</svg>
)"), 6) {
  const int importantSize = 31;
  const int secondarySize = 20;
  ad = new Icon(u":/icons/waypoints/AD.svg"_s, importantSize);
  adGld = new Icon(u":/icons/waypoints/AD-GLD.svg"_s, secondarySize);
  adGrass = new Icon(u":/icons/waypoints/AD-GRASS.svg"_s, importantSize);
  adInop = new Icon(u":/icons/waypoints/AD-INOP.svg"_s, importantSize);
  adMil = new Icon(u":/icons/waypoints/AD-MIL.svg"_s, importantSize);
  adMilGrass = new Icon(u":/icons/waypoints/AD-MIL-GRASS.svg"_s, importantSize);
  adMilPaved = new Icon(u":/icons/waypoints/AD-MIL-PAVED.svg"_s, importantSize);
  adUl = new Icon(u":/icons/waypoints/AD-UL.svg"_s, secondarySize);
  adWater = new Icon(u":/icons/waypoints/AD-WATER.svg"_s, importantSize);

  const int wpSize = 23;
  const int secwpSize = 14;
  wp = new Icon(u":/icons/waypoints/WP.svg"_s, wpSize);
  mrp = new Icon(u":/icons/waypoints/MRP.svg"_s, wpSize);
  rp = new Icon(u":/icons/waypoints/RP.svg"_s, wpSize);
  dme = new Icon(u":/icons/waypoints/DME.svg"_s, secwpSize);
  ndb = new Icon(u":/icons/waypoints/NDB.svg"_s, wpSize);
  vor = new Icon(u":/icons/waypoints/VOR.svg"_s, wpSize);
  vortac = new Icon(u":/icons/waypoints/VORTAC.svg"_s, wpSize);

  // this file is called VORDME.svg, but the Qt binding is named
  // VOR-DME.svg (see icons.qrc.in):
  vordme = new Icon(u":/icons/waypoints/VOR-DME.svg"_s, wpSize);

  warning = new Icon(u":/icons/NOTAM.svg"_s, 9); // TODO
}

Ui::IconManager::~IconManager() {
  // TODO
}

const Ui::Icon* Ui::IconManager::aerodrome() const {
  if (ad && ad->isValid()) return ad;
  else return &fallback;
}

const Ui::Icon* Ui::IconManager::aerodromeGlider() const {
  if (adGld && adGld->isValid()) return adGld;
  else return &fallback;
}

const Ui::Icon* Ui::IconManager::aerodromeGrass() const {
  if (adGrass && adGrass->isValid()) return adGrass;
  else return &fallback;
}

const Ui::Icon* Ui::IconManager::aerodromeInop() const {
  if (adInop && adInop->isValid()) return adInop;
  else return &fallback;
}

const Ui::Icon* Ui::IconManager::aerodromeMil() const {
  if (adMil && adMil->isValid()) return adMil;
  else return &fallback;
}

const Ui::Icon* Ui::IconManager::aerodromeMilGrass() const {
  if (adMilGrass && adMilGrass->isValid()) return adMilGrass;
  else return &fallback;
}

const Ui::Icon* Ui::IconManager::aerodromeMilPaved() const {
  if (adMilPaved && adMilPaved->isValid()) return adMilPaved;
  else return &fallback;
}

const Ui::Icon* Ui::IconManager::aerodromePaved() const {
  if (adPaved && adPaved->isValid()) return adPaved;
  else return &fallback;
}

const Ui::Icon* Ui::IconManager::aerodromeUl() const {
  if (adUl && adUl->isValid()) return adUl;
  else return &fallback;
}

const Ui::Icon* Ui::IconManager::aerodromeWater() const {
  if (adWater && adWater->isValid()) return adWater;
  else return &fallback;
}

const Ui::Icon* Ui::IconManager::waypoint() const {
  if (wp && wp->isValid()) return wp;
  else return &fallback;
}

const Ui::Icon* Ui::IconManager::waypointMRP() const {
  if (mrp && mrp->isValid()) return mrp;
  else return &fallback;
}

const Ui::Icon* Ui::IconManager::waypointRP() const {
  if (rp && rp->isValid()) return rp;
  else return &fallback;
}

const Ui::Icon* Ui::IconManager::waypointDME() const {
  if (dme && dme->isValid()) return dme;
  else return &fallback;
}

const Ui::Icon* Ui::IconManager::waypointNDB() const {
  if (ndb && ndb->isValid()) return ndb;
  else return &fallback;
}

const Ui::Icon* Ui::IconManager::waypointVOR() const {
  if (vor && vor->isValid()) return vor;
  else return &fallback;
}

const Ui::Icon* Ui::IconManager::waypointVORDME() const {
  if (vordme && vordme->isValid()) return vordme;
  else return &fallback;
}

const Ui::Icon* Ui::IconManager::waypointVORTAC() const {
  if (vortac && vortac->isValid()) return vortac;
  else return &fallback;
}

const Ui::Icon* Ui::IconManager::notam() const {
  if (warning && warning->isValid()) return warning;
  else return &fallback;
}
