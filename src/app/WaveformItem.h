#pragma once
#include <QColor>
#include <QPointer>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

class DeckController;

// Waveform with beat grid, loop region and playhead.
//  Overview mode: whole track. Click or drag = move the playhead: it follows
//  the pointer and the deck jumps there on release (on the beat, or into
//  phase with the master when synced and playing). Shift-drag = draw an In / Out loop,
//  snapped to bars (add Alt/Option to snap to beats).
//  Zoom mode: a window of N beats scrolling around the playhead. Drag it to
//  move through the track the same way; the waveform follows the hand.
//
// Drawn with the scene graph: the waveform is rendered once into textures
// (the zoom one in tiles along the track) and each frame only moves them and
// redraws a few rectangles (grid, loop, playhead), so scrolling costs ~nothing.
class WaveformItem : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(DeckController *deck READ deck WRITE setDeck NOTIFY deckChanged)
    Q_PROPERTY(bool zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(double zoomBeats READ zoomBeats WRITE setZoomBeats NOTIFY zoomChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QColor gridColor MEMBER m_gridColor NOTIFY colorChanged)  // beat / phrase lines
    Q_PROPERTY(QColor loopColor MEMBER m_loopColor NOTIFY colorChanged)  // loop regions

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

    //! Milliseconds spent building waveform frames since the last call (BEATER_FPS).
    static double takeRenderMs();

signals:
    void deckChanged();
    void zoomChanged();
    void colorChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    double secAtX(double x) const;
    double snapBeat(double beat, bool fine) const;

    QPointer<DeckController> m_deck;
    bool m_zoom = false;
    double m_zoomBeats = 16.0;
    QColor m_color = QColor(255, 90, 54);
    QColor m_gridColor = QColor(255, 255, 255);
    QColor m_loopColor = QColor(255, 255, 255);
    int m_trackRev = 0; // bumped when the track changes, so cached textures go

    bool m_pressed = false;
    bool m_dragging = false;  // overview: shift-dragging a loop
    bool m_loopDrag = false;  // overview: this press sets a loop (Shift)
    double m_pressX = 0.0;
    double m_dragA = 0.0, m_dragB = 0.0; // beats

    // moving the playhead (the deck holds the preview)
    bool m_scrubbing = false;  // zoom: past the drag threshold
    double m_scrubFrom = 0.0;  // zoom: deck position at the press, s
};
