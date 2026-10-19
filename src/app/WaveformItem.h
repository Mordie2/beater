#pragma once
#include <QColor>
#include <QImage>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

class DeckController;

// Waveform with beat grid, loop region and playhead.
//  Overview mode: whole track. Click = seek (beat snapped), drag = set loop
//  region snapped to bars (hold Alt/Option to snap to beats).
//  Zoom mode: a window of N beats scrolling around the playhead.
class WaveformItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(DeckController *deck READ deck WRITE setDeck NOTIFY deckChanged)
    Q_PROPERTY(bool zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(double zoomBeats READ zoomBeats WRITE setZoomBeats NOTIFY zoomChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)

public:
    explicit WaveformItem(QQuickItem *parent = nullptr);

    DeckController *deck() const { return m_deck; }
    void setDeck(DeckController *d);
    bool zoom() const { return m_zoom; }
    void setZoom(bool z);
    double zoomBeats() const { return m_zoomBeats; }
    void setZoomBeats(double b);
    QColor color() const { return m_color; }
    void setColor(const QColor &c);

    void paint(QPainter *p) override;

signals:
    void deckChanged();
    void zoomChanged();
    void colorChanged();

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void geometryChange(const QRectF &n, const QRectF &o) override;

private:
    void paintOverview(QPainter *p);
    void paintZoom(QPainter *p);
    void rebuildCache();
    double secAtX(double x) const;
    double snapBeat(double beat, bool fine) const;

    QPointer<DeckController> m_deck;
    bool m_zoom = false;
    double m_zoomBeats = 16.0;
    QColor m_color = QColor(255, 90, 54);

    QImage m_cache;
    int m_cacheRev = -1;
    QSizeF m_cacheSize;

    bool m_pressed = false;
    bool m_dragging = false;
    double m_pressX = 0.0;
    double m_dragA = 0.0, m_dragB = 0.0; // beats
};
