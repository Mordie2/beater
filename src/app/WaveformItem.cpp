#include "app/WaveformItem.h"
#include "app/DeckController.h"

#include <QMouseEvent>
#include <QPainter>
#include <QQuickWindow>

#include <cmath>

WaveformItem::WaveformItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setCursor(Qt::ArrowCursor);
    setAntialiasing(false);
    setOpaquePainting(false);
}

void WaveformItem::setDeck(DeckController *d)
{
    if (d == m_deck)
        return;
    if (m_deck)
        disconnect(m_deck, nullptr, this, nullptr);
    m_deck = d;
    if (m_deck) {
        auto repaint = [this] { update(); };
        connect(m_deck, &DeckController::positionChanged, this, repaint);
        connect(m_deck, &DeckController::gridChanged, this, repaint);
        connect(m_deck, &DeckController::loopChanged, this, repaint);
        connect(m_deck, &DeckController::trackChanged, this, [this] {
            m_cacheRev = -1;
            update();
        });
    }
    m_cacheRev = -1;
    emit deckChanged();
    update();
}

void WaveformItem::setZoom(bool z)
{
    if (z == m_zoom)
        return;
    m_zoom = z;
    setCursor(z ? Qt::OpenHandCursor : Qt::ArrowCursor);
    emit zoomChanged();
    update();
}

void WaveformItem::setZoomBeats(double b)
{
    b = std::clamp(b, 2.0, 128.0);
    if (b == m_zoomBeats)
        return;
    m_zoomBeats = b;
    emit zoomChanged();
    update();
}

void WaveformItem::setColor(const QColor &c)
{
    if (c == m_color)
        return;
    m_color = c;
    m_cacheRev = -1;
    emit colorChanged();
    update();
}

void WaveformItem::geometryChange(const QRectF &n, const QRectF &o)
{
    QQuickPaintedItem::geometryChange(n, o);
    m_cacheRev = -1;
    update();
}

void WaveformItem::paint(QPainter *p)
{
    if (!m_deck || !m_deck->buffer()) {
        p->setPen(QColor(255, 255, 255, 30));
        p->drawLine(QPointF(0, height() / 2), QPointF(width(), height() / 2));
        return;
    }
    if (m_zoom)
        paintZoom(p);
    else
        paintOverview(p);
}

// ---------------------------------------------------------------- overview

void WaveformItem::rebuildCache()
{
    const TrackBuffer *tb = m_deck->buffer();
    const qreal dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
    const int w = std::max(1, int(width() * dpr)), h = std::max(1, int(height() * dpr));
    m_cache = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    m_cache.fill(Qt::transparent);
    QPainter p(&m_cache);
    const int buckets = int(tb->peaks.size());
    const double mid = h / 2.0;
    QColor peakC = m_color;
    peakC.setAlpha(90);
    QColor rmsC = m_color;
    for (int x = 0; x < w; ++x) {
        const int b0 = int(double(x) / w * buckets), b1 = std::max(b0 + 1, int(double(x + 1) / w * buckets));
        float pk = 0, rm = 0;
        for (int b = b0; b < b1 && b < buckets; ++b) {
            pk = std::max(pk, tb->peaks[size_t(b)]);
            rm = std::max(rm, tb->rms[size_t(b)]);
        }
        p.setPen(peakC);
        p.drawLine(QPointF(x + 0.5, mid - pk * mid), QPointF(x + 0.5, mid + pk * mid));
        p.setPen(rmsC);
        const double r = std::min(1.0, rm * 1.6);
        p.drawLine(QPointF(x + 0.5, mid - r * mid), QPointF(x + 0.5, mid + r * mid));
    }
    m_cache.setDevicePixelRatio(dpr);
    m_cacheRev = m_deck->waveformRevision();
    m_cacheSize = size();
}

double WaveformItem::secAtX(double x) const
{
    return std::clamp(x / std::max(1.0, width()), 0.0, 1.0) * m_deck->duration();
}

double WaveformItem::snapBeat(double beat, bool fine) const
{
    const double q = fine ? 1.0 : 4.0;
    return std::round(beat / q) * q;
}

void WaveformItem::paintOverview(QPainter *p)
{
    if (m_cacheRev != m_deck->waveformRevision() || m_cacheSize != size())
        rebuildCache();
    const double w = width(), h = height();
    const double dur = std::max(0.001, m_deck->duration());
    auto xAt = [&](double sec) { return sec / dur * w; };

    p->drawImage(QRectF(0, 0, w, h), m_cache);

    const BeatGrid g = m_deck->grid();
    // phrase markers every 8 bars
    if (g.valid()) {
        p->setPen(QColor(255, 255, 255, 28));
        const double first = std::ceil(g.beatAtSec(0) / 32.0) * 32.0;
        for (double b = first; g.secAtBeat(b) < dur; b += 32.0) {
            const double x = std::round(xAt(g.secAtBeat(b))) + 0.5;
            p->drawLine(QPointF(x, 0), QPointF(x, h));
        }
    }

    // loop region (or the one being dragged)
    double la = m_deck->loopStartBeat(), lb = m_deck->loopEndBeat();
    if (m_dragging) {
        la = std::min(m_dragA, m_dragB);
        lb = std::max(m_dragA, m_dragB);
    }
    if (g.valid() && lb > la) {
        const double x0 = xAt(g.secAtBeat(la)), x1 = xAt(g.secAtBeat(lb));
        const bool on = m_deck->loopEnabled() || m_dragging;
        QColor fill(255, 255, 255, on ? 34 : 12);
        p->fillRect(QRectF(x0, 0, x1 - x0, h), fill);
        QColor edge(255, 255, 255, on ? 170 : 60);
        p->fillRect(QRectF(x0, 0, 1.5, h), edge);
        p->fillRect(QRectF(x1 - 1.5, 0, 1.5, h), edge);
        p->fillRect(QRectF(x0, 0, x1 - x0, 3), edge);
    }

    // played part dimmed, playhead
    const double px = xAt(m_deck->shownPosition());
    p->fillRect(QRectF(0, 0, px, h), QColor(0, 0, 0, 110));
    p->fillRect(QRectF(px - 1, 0, 2, h), QColor(255, 255, 255));
}

// ---------------------------------------------------------------- zoom

void WaveformItem::paintZoom(QPainter *p)
{
    const TrackBuffer *tb = m_deck->buffer();
    const BeatGrid g = m_deck->grid();
    const double w = width(), h = height(), mid = h / 2.0;
    const double pos = m_deck->shownPosition();
    const double spb = g.valid() ? g.secPerBeat() : 0.5;
    const double span = m_zoomBeats * spb;
    const double t0 = pos - span / 2.0;
    auto xAt = [&](double sec) { return (sec - t0) / span * w; };

    // loop region underneath
    const double la = m_deck->loopStartBeat(), lb = m_deck->loopEndBeat();
    if (g.valid() && lb > la) {
        const double x0 = xAt(g.secAtBeat(la)), x1 = xAt(g.secAtBeat(lb));
        p->fillRect(QRectF(x0, 0, x1 - x0, h), QColor(255, 255, 255, m_deck->loopEnabled() ? 22 : 8));
    }

    // waveform from fine peaks
    const double sr = tb->sampleRate;
    const double bucketsPerSec = sr / double(TrackBuffer::kDetailFrames);
    const int n = int(tb->detail.size());
    const qreal dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
    const int cols = int(w * dpr);
    QColor past = m_color.darker(170);
    for (int c = 0; c < cols; ++c) {
        const double ta = t0 + span * c / cols, tb2 = t0 + span * (c + 1) / cols;
        int b0 = int(std::floor(ta * bucketsPerSec)), b1 = int(std::ceil(tb2 * bucketsPerSec));
        float pk = 0;
        for (int b = std::max(0, b0); b < std::min(n, std::max(b1, b0 + 1)); ++b)
            pk = std::max(pk, tb->detail[size_t(b)]);
        if (pk <= 0)
            continue;
        const double x = (c + 0.5) / dpr;
        p->setPen(QPen(ta < pos ? past : m_color, 1.0 / dpr));
        p->drawLine(QPointF(x, mid - pk * mid * 0.95), QPointF(x, mid + pk * mid * 0.95));
    }

    // beat grid
    if (g.valid()) {
        const double bFirst = std::ceil(g.beatAtSec(t0));
        for (double b = bFirst; g.secAtBeat(b) < t0 + span; b += 1.0) {
            const double x = xAt(g.secAtBeat(b));
            const bool bar = std::fmod(std::fmod(b, 4.0) + 4.0, 4.0) < 0.5;
            p->fillRect(QRectF(x - (bar ? 1.0 : 0.5), 0, bar ? 2.0 : 1.0, h),
                        QColor(255, 255, 255, bar ? 150 : 50));
        }
    }

    // fixed playhead in the middle
    p->fillRect(QRectF(w / 2 - 1, 0, 2, h), QColor(255, 255, 255));
}

// ---------------------------------------------------------------- mouse

void WaveformItem::mousePressEvent(QMouseEvent *e)
{
    if (!m_deck || !m_deck->buffer()) {
        e->ignore();
        return;
    }
    m_pressed = true;
    m_pressX = e->position().x();
    setKeepMouseGrab(true);
    e->accept();
    if (m_zoom) {
        m_scrubbing = false;
        m_scrubFrom = m_deck->position();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    m_dragging = false;
    m_loopDrag = e->modifiers() & Qt::ShiftModifier;
    if (!m_loopDrag)
        m_deck->setPreview(secAtX(m_pressX));
}

void WaveformItem::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_pressed)
        return;
    const BeatGrid g = m_deck->grid();
    const double x = e->position().x();
    if (m_zoom) {
        if (!m_scrubbing && std::abs(x - m_pressX) > 3.0)
            m_scrubbing = true;
        if (m_scrubbing) {
            // the track moves with the hand: drag left to go forward
            const double span = m_zoomBeats * (g.valid() ? g.secPerBeat() : 0.5);
            m_deck->setPreview(m_scrubFrom - (x - m_pressX) / std::max(1.0, width()) * span);
        }
        return;
    }
    if (!m_loopDrag) {
        m_deck->setPreview(secAtX(x));
        return;
    }
    if (!g.valid())
        return;
    if (!m_dragging && std::abs(x - m_pressX) > 4.0)
        m_dragging = true;
    if (m_dragging) {
        const bool fine = e->modifiers() & Qt::AltModifier;
        m_dragA = snapBeat(g.beatAtSec(secAtX(m_pressX)), fine);
        m_dragB = snapBeat(g.beatAtSec(secAtX(x)), fine);
        update();
    }
}

void WaveformItem::mouseReleaseEvent(QMouseEvent *e)
{
    if (!m_pressed)
        return;
    m_pressed = false;
    setKeepMouseGrab(false);
    if (m_zoom) {
        setCursor(Qt::OpenHandCursor);
        if (m_scrubbing) {
            m_scrubbing = false;
            m_deck->commitPreview();
        }
        return;
    }
    if (!m_loopDrag) {
        m_deck->commitPreview();
    } else if (m_dragging) {
        m_dragging = false;
        if (std::abs(m_dragB - m_dragA) >= 1.0)
            m_deck->setLoopBeats(std::min(m_dragA, m_dragB), std::max(m_dragA, m_dragB));
        update();
    } else {
        m_deck->seek(secAtX(e->position().x()));
    }
}
