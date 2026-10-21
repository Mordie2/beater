#include "app/WaveformItem.h"
#include "app/DeckController.h"

#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QQuickWindow>
#include <QSGSimpleRectNode>
#include <QSGSimpleTextureNode>
#include <QSGTexture>

#include <cmath>

namespace {

constexpr int kTileWidth = 2048; // device pixels per zoom tile
constexpr int kMaxTiles = 12;    // zoom tiles kept per waveform

// Plain rectangles for this frame; the nodes are reused from frame to frame.
class RectLayer : public QSGNode {
public:
    void begin() { m_used = 0; }
    void add(const QRectF &r, const QColor &c)
    {
        QSGSimpleRectNode *n;
        if (m_used < childCount()) {
            n = static_cast<QSGSimpleRectNode *>(childAtIndex(m_used));
        } else {
            n = new QSGSimpleRectNode;
            n->setFlag(QSGNode::OwnedByParent);
            appendChildNode(n);
        }
        if (n->rect() != r)
            n->setRect(r);
        if (n->color() != c)
            n->setColor(c);
        ++m_used;
    }
    void end()
    {
        while (childCount() > m_used) {
            QSGNode *n = lastChild();
            removeChildNode(n);
            delete n;
        }
    }

private:
    int m_used = 0;
};

// Texture quads for this frame (the overview image, or the visible zoom tiles).
class ImageLayer : public QSGNode {
public:
    void begin() { m_used = 0; }
    void add(QSGTexture *t, const QRectF &r)
    {
        QSGSimpleTextureNode *n;
        if (m_used < childCount()) {
            n = static_cast<QSGSimpleTextureNode *>(childAtIndex(m_used));
        } else {
            n = new QSGSimpleTextureNode;
            n->setFlag(QSGNode::OwnedByParent);
            n->setFiltering(QSGTexture::Linear);
            appendChildNode(n);
        }
        if (n->texture() != t)
            n->setTexture(t);
        if (n->rect() != r)
            n->setRect(r);
        ++m_used;
    }
    void end()
    {
        while (childCount() > m_used) {
            QSGNode *n = lastChild();
            removeChildNode(n);
            delete n;
        }
    }

private:
    int m_used = 0;
};

// What the cached textures were drawn for; any change redraws them.
struct CacheKey {
    const TrackBuffer *buffer = nullptr;
    int rev = -1;
    int width = 0, height = 0; // device pixels
    QRgb color = 0, core = 0;
    bool glow = false;
    double pixelsPerSec = 0.0; // zoom only
    bool operator==(const CacheKey &) const = default;
};

class WaveNode : public QSGNode {
public:
    WaveNode()
    {
        under->setFlag(OwnedByParent);
        glows->setFlag(OwnedByParent);
        images->setFlag(OwnedByParent);
        over->setFlag(OwnedByParent);
        appendChildNode(under);
        appendChildNode(glows);
        appendChildNode(images);
        appendChildNode(over);
    }
    ~WaveNode() override { dropTextures(); }

    void dropTextures()
    {
        // the nodes using them are rebuilt this frame
        images->begin();
        images->end();
        glows->begin();
        glows->end();
        delete overview;
        delete overviewGlow;
        overview = overviewGlow = nullptr;
        for (const Tile &t : std::as_const(tiles)) {
            delete t.texture;
            delete t.glow;
        }
        tiles.clear();
    }

    RectLayer *under = new RectLayer;
    ImageLayer *glows = new ImageLayer; // the halos, under the waveform
    ImageLayer *images = new ImageLayer;
    RectLayer *over = new RectLayer;

    CacheKey key;
    QSGTexture *overview = nullptr, *overviewGlow = nullptr;
    struct Tile {
        QSGTexture *texture = nullptr;
        QSGTexture *glow = nullptr;
        quint64 used = 0;
    };
    QHash<int, Tile> tiles;
    quint64 frame = 0;
};

using NodeTile = WaveNode::Tile;

double s_renderMs = 0.0;

// The halo round a drawn waveform: its shape, blurred (two box blurs each
// way, close to a gaussian), in the glow colour. Made once per tile.
QImage glowOf(const QImage &src, const QColor &color, int radius, double gain)
{
    const int w = src.width(), h = src.height();
    std::vector<float> a(size_t(w) * h), t(a.size());
    for (int y = 0; y < h; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        for (int x = 0; x < w; ++x)
            a[size_t(y) * w + x] = qAlpha(line[x]) / 255.0f;
    }
    const int r = std::max(1, radius);
    const float norm = 1.0f / float(2 * r + 1);
    auto blurH = [&](const std::vector<float> &in, std::vector<float> &out) {
        for (int y = 0; y < h; ++y) {
            const float *row = in.data() + size_t(y) * w;
            float *o = out.data() + size_t(y) * w;
            float sum = 0;
            for (int x = -r; x <= r; ++x)
                sum += row[std::clamp(x, 0, w - 1)];
            for (int x = 0; x < w; ++x) {
                o[x] = sum * norm;
                sum += row[std::min(x + r + 1, w - 1)] - row[std::max(x - r, 0)];
            }
        }
    };
    auto blurV = [&](const std::vector<float> &in, std::vector<float> &out) {
        for (int x = 0; x < w; ++x) {
            float sum = 0;
            for (int y = -r; y <= r; ++y)
                sum += in[size_t(std::clamp(y, 0, h - 1)) * w + x];
            for (int y = 0; y < h; ++y) {
                out[size_t(y) * w + x] = sum * norm;
                sum += in[size_t(std::min(y + r + 1, h - 1)) * w + x] - in[size_t(std::max(y - r, 0)) * w + x];
            }
        }
    };
    blurH(a, t);
    blurV(t, a);
    blurH(a, t);
    blurV(t, a);
    QImage out(w, h, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const int al = int(std::min(1.0f, a[size_t(y) * w + x] * float(gain)) * 255.0f);
            line[x] = qPremultiply(qRgba(color.red(), color.green(), color.blue(), al));
        }
    }
    return out;
}

// Whole track, peak (faint) and RMS (solid) per column.
QImage drawOverview(const TrackBuffer *tb, int w, int h, const QColor &color)
{
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    const int buckets = int(tb->peaks.size());
    const double mid = h / 2.0;
    QColor peakC = color;
    peakC.setAlpha(90);
    for (int x = 0; x < w; ++x) {
        const int b0 = int(double(x) / w * buckets), b1 = std::max(b0 + 1, int(double(x + 1) / w * buckets));
        float pk = 0, rm = 0;
        for (int b = b0; b < b1 && b < buckets; ++b) {
            pk = std::max(pk, tb->peaks[size_t(b)]);
            rm = std::max(rm, tb->rms[size_t(b)]);
        }
        p.setPen(peakC);
        p.drawLine(QPointF(x + 0.5, mid - pk * mid), QPointF(x + 0.5, mid + pk * mid));
        p.setPen(color);
        const double r = std::min(1.0, rm * 1.6);
        p.drawLine(QPointF(x + 0.5, mid - r * mid), QPointF(x + 0.5, mid + r * mid));
    }
    return img;
}

// One stretch of the zoomed waveform, from the fine peaks: tile i starts at
// i * kTileWidth / pixelsPerSec seconds. pad more columns either side (for
// a glow, so it blurs across the tiles' seams).
QImage drawTile(const TrackBuffer *tb, int i, int h, double pixelsPerSec, const QColor &color, int pad = 0)
{
    QImage img(kTileWidth + 2 * pad, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    const QRgb px = qPremultiply(color.rgba());
    const double mid = h / 2.0;
    const double bucketsPerSec = double(tb->sampleRate) / double(TrackBuffer::kDetailFrames);
    const int n = int(tb->detail.size());
    const double t0 = (double(i) * kTileWidth - pad) / pixelsPerSec;
    for (int c = 0; c < img.width(); ++c) {
        const double ta = t0 + c / pixelsPerSec, tb2 = t0 + (c + 1) / pixelsPerSec;
        const int b0 = int(std::floor(ta * bucketsPerSec)), b1 = std::max(b0 + 1, int(std::ceil(tb2 * bucketsPerSec)));
        float pk = 0;
        for (int b = std::max(0, b0); b < std::min(n, b1); ++b)
            pk = std::max(pk, tb->detail[size_t(b)]);
        if (pk <= 0)
            continue;
        const int y0 = std::max(0, int(std::floor(mid - pk * mid * 0.95)));
        const int y1 = std::min(h - 1, int(std::ceil(mid + pk * mid * 0.95)));
        for (int y = y0; y <= y1; ++y)
            reinterpret_cast<QRgb *>(img.scanLine(y))[c] = px;
    }
    return img;
}

} // namespace

WaveformItem::WaveformItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    setClip(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setCursor(Qt::ArrowCursor);
    connect(this, &WaveformItem::colorChanged, this, [this] { update(); }); // gridColor / loopColor
    connect(this, &WaveformItem::gridEditChanged, this, [this] {
        setCursor(m_zoom ? (m_gridEdit ? Qt::SizeHorCursor : Qt::OpenHandCursor) : Qt::ArrowCursor);
        update();
    });
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
            ++m_trackRev;
            update();
        });
    }
    ++m_trackRev;
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
    emit colorChanged();
    update();
}

double WaveformItem::takeRenderMs()
{
    const double ms = s_renderMs;
    s_renderMs = 0.0;
    return ms;
}

// Runs on the render thread while the GUI thread waits, so reading the deck is safe.
QSGNode *WaveformItem::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    QElapsedTimer timer;
    timer.start();
    auto *node = static_cast<WaveNode *>(old);
    if (!node)
        node = new WaveNode;
    ++node->frame;

    const double w = width(), h = height();
    const qreal dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
    const int wpx = std::max(1, int(std::round(w * dpr))), hpx = std::max(1, int(std::round(h * dpr)));
    const TrackBuffer *tb = m_deck ? m_deck->buffer() : nullptr;

    node->under->begin();
    node->glows->begin();
    node->images->begin();
    node->over->begin();

    if (!tb || tb->frames() == 0 || w < 1 || h < 1) {
        node->dropTextures();
        node->key = {};
        node->over->add(QRectF(0, std::floor(h / 2), w, 1), QColor(255, 255, 255, 30));
    } else if (!m_zoom) {
        // ---------------------------------------------------------- overview
        const QColor core = m_glow && m_coreColor.isValid() ? m_coreColor : m_color;
        const CacheKey key{tb, m_trackRev, wpx, hpx, m_color.rgba(), core.rgba(), m_glow, 0.0};
        if (!(key == node->key) || !node->overview) {
            node->dropTextures();
            node->key = key;
            const QImage img = drawOverview(tb, wpx, hpx, core);
            node->overview = window()->createTextureFromImage(img);
            if (m_glow)
                node->overviewGlow = window()->createTextureFromImage(glowOf(img, m_color, int(3 * dpr), 1.4));
        }
        if (node->overviewGlow)
            node->glows->add(node->overviewGlow, QRectF(0, 0, w, h));
        node->images->add(node->overview, QRectF(0, 0, w, h));

        const double dur = std::max(0.001, m_deck->duration());
        auto xAt = [&](double sec) { return sec / dur * w; };
        const BeatGrid g = m_deck->grid();
        // phrase markers every 8 bars
        if (g.valid()) {
            const double first = std::ceil(g.beatAtSec(0) / 32.0) * 32.0;
            QColor c = m_gridColor;
            c.setAlpha(45);
            for (double b = first; g.secAtBeat(b) < dur; b += 32.0)
                node->over->add(QRectF(std::round(xAt(g.secAtBeat(b))), 0, 1, h), c);
        }
        // the loops not in use right now, faint
        if (g.valid()) {
            const QString active = m_deck->activeLoop();
            const struct { const char *kind; double a, b; } loops[] = {
                {"intro", m_deck->introStartBeat(), m_deck->introEndBeat()},
                {"custom", m_deck->customStartBeat(), m_deck->customEndBeat()},
                {"end", m_deck->endStartBeat(), m_deck->endEndBeat()},
            };
            QColor fill = m_loopColor, edge = m_loopColor;
            fill.setAlpha(22);
            edge.setAlpha(110);
            for (const auto &l : loops) {
                if (l.b <= l.a || active == QLatin1String(l.kind))
                    continue;
                const double x0 = xAt(g.secAtBeat(l.a)), x1 = xAt(g.secAtBeat(l.b));
                node->over->add(QRectF(x0, 0, x1 - x0, h), fill);
                node->over->add(QRectF(x0, 0, 1, h), edge);
                node->over->add(QRectF(x1 - 1, 0, 1, h), edge);
            }
            // an In placed, waiting for its Out: the loop so far, growing with the playhead
            if (m_deck->customPending()) {
                const double x0 = xAt(g.secAtBeat(m_deck->customStartBeat()));
                const double x1 = std::max(x0, xAt(m_deck->shownPosition()));
                QColor grow = m_loopColor;
                grow.setAlpha(70);
                QColor solid = m_loopColor;
                node->over->add(QRectF(x0, 0, x1 - x0, h), grow);
                node->over->add(QRectF(x0, 0, x1 - x0, 4), solid);
                node->over->add(QRectF(x0 - 1, 0, 2, h), solid);
            }
        }
        // the loop in use (or the one being drawn with Shift)
        double la = m_deck->activeLoopStartBeat(), lb = m_deck->activeLoopEndBeat();
        if (m_dragging) {
            la = std::min(m_dragA, m_dragB);
            lb = std::max(m_dragA, m_dragB);
        }
        if (g.valid() && lb > la) {
            const double x0 = xAt(g.secAtBeat(la)), x1 = xAt(g.secAtBeat(lb));
            const bool on = !m_deck->activeLoop().isEmpty() || m_dragging;
            QColor fill = m_loopColor, edge = m_loopColor;
            fill.setAlpha(on ? 75 : 22);
            edge.setAlpha(on ? 255 : 110);
            node->over->add(QRectF(x0, 0, x1 - x0, h), fill);
            node->over->add(QRectF(x0, 0, 2, h), edge);
            node->over->add(QRectF(x1 - 2, 0, 2, h), edge);
            node->over->add(QRectF(x0, 0, x1 - x0, 4), edge);
        }
        // played part dimmed, playhead
        const double px = xAt(m_deck->shownPosition());
        node->over->add(QRectF(0, 0, px, h), QColor(0, 0, 0, 110));
        node->over->add(QRectF(px - 1, 0, 2, h), QColor(255, 255, 255));
    } else {
        // ---------------------------------------------------------- zoom
        BeatGrid g = m_deck->grid();
        g.firstBeatSec += m_gridShift; // the grid being dragged
        const double pos = m_deck->shownPosition();
        const double spb = g.valid() ? g.secPerBeat() : 0.5;
        const double span = m_zoomBeats * spb;
        const double t0 = pos - span / 2.0;
        const double pixelsPerSec = wpx / span;
        auto xAt = [&](double sec) { return (sec - t0) / span * w; };

        const QColor core = m_glow && m_coreColor.isValid() ? m_coreColor : m_color;
        const CacheKey key{tb, m_trackRev, wpx, hpx, m_color.rgba(), core.rgba(), m_glow, pixelsPerSec};
        if (!(key == node->key)) {
            node->dropTextures();
            node->key = key;
        }

        // the loop in use: tinted underneath, a band along the top and its edges over
        const double la = m_deck->activeLoopStartBeat(), lb = m_deck->activeLoopEndBeat();
        if (g.valid() && lb > la) {
            const double x0 = xAt(g.secAtBeat(la)), x1 = xAt(g.secAtBeat(lb));
            QColor fill = m_loopColor;
            fill.setAlpha(60);
            node->under->add(QRectF(x0, 0, x1 - x0, h), fill);
            node->over->add(QRectF(x0, 0, x1 - x0, 5), m_loopColor);
            node->over->add(QRectF(x0 - 1, 0, 2, h), m_loopColor);
            node->over->add(QRectF(x1 - 1, 0, 2, h), m_loopColor);
        }
        // an In placed, waiting for its Out: the loop so far, up to the playhead
        if (g.valid() && m_deck->customPending()) {
            const double x0 = xAt(g.secAtBeat(m_deck->customStartBeat()));
            const double x1 = std::max(x0, w / 2);
            QColor fill = m_loopColor;
            fill.setAlpha(60);
            node->under->add(QRectF(x0, 0, x1 - x0, h), fill);
            node->over->add(QRectF(x0, 0, x1 - x0, 5), m_loopColor);
            node->over->add(QRectF(x0 - 1, 0, 2, h), m_loopColor);
        }

        // the tiles in view
        const double tileSec = kTileWidth / pixelsPerSec;
        const double dur = m_deck->duration();
        const int iFirst = std::max(0, int(std::floor(t0 / tileSec)));
        const int iLast = int(std::floor(std::min(t0 + span, dur) / tileSec));
        for (int i = iFirst; i <= iLast; ++i) {
            auto it = node->tiles.find(i);
            if (it == node->tiles.end()) {
                if (node->tiles.size() >= kMaxTiles) {
                    // drop the least recently shown
                    auto oldest = node->tiles.begin();
                    for (auto t = node->tiles.begin(); t != node->tiles.end(); ++t)
                        if (t->used < oldest->used)
                            oldest = t;
                    delete oldest->texture;
                    delete oldest->glow;
                    node->tiles.erase(oldest);
                }
                NodeTile t;
                if (m_glow) {
                    // drawn wider than the tile, so the blur sees across the seams
                    const int radius = int(5 * dpr), pad = 4 * radius;
                    const QImage wide = drawTile(tb, i, hpx, pixelsPerSec, core, pad);
                    t.texture = window()->createTextureFromImage(wide.copy(pad, 0, kTileWidth, hpx));
                    t.glow = window()->createTextureFromImage(glowOf(wide, m_color, radius, 1.6).copy(pad, 0, kTileWidth, hpx));
                } else {
                    t.texture = window()->createTextureFromImage(drawTile(tb, i, hpx, pixelsPerSec, core));
                }
                it = node->tiles.insert(i, t);
            }
            it->used = node->frame;
            const QRectF at(xAt(i * tileSec), 0, tileSec / span * w, h);
            if (it->glow)
                node->glows->add(it->glow, at);
            node->images->add(it->texture, at);
        }

        // what has played, darker
        node->over->add(QRectF(0, 0, w / 2, h), QColor(0, 0, 0, 100));
        // beat grid
        if (g.valid()) {
            const double bFirst = std::ceil(g.beatAtSec(t0));
            for (double b = bFirst; g.secAtBeat(b) < t0 + span; b += 1.0) {
                const double x = xAt(g.secAtBeat(b));
                const bool bar = std::fmod(std::fmod(b, 4.0) + 4.0, 4.0) < 0.5;
                QColor c = m_gridColor;
                c.setAlpha(bar ? 170 : 60);
                node->over->add(QRectF(x - (bar ? 1.0 : 0.5), 0, bar ? 2.0 : 1.0, h), c);
            }
        }
        // fixed playhead in the middle
        node->over->add(QRectF(w / 2 - 1, 0, 2, h), QColor(255, 255, 255));
        // grid edit: a frame in the grid's colour
        if (m_gridEdit) {
            const double t = 2.0;
            node->over->add(QRectF(0, 0, w, t), m_gridColor);
            node->over->add(QRectF(0, h - t, w, t), m_gridColor);
            node->over->add(QRectF(0, 0, t, h), m_gridColor);
            node->over->add(QRectF(w - t, 0, t, h), m_gridColor);
        }
    }

    node->under->end();
    node->glows->end();
    node->images->end();
    node->over->end();
    s_renderMs += timer.nsecsElapsed() / 1e6;
    return node;
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
        if (!m_gridEdit)
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
            const double span = m_zoomBeats * (g.valid() ? g.secPerBeat() : 0.5);
            const double dsec = (x - m_pressX) / std::max(1.0, width()) * span;
            if (m_gridEdit) {
                // the grid moves with the hand, the music stays
                if (g.valid()) {
                    m_gridShift = dsec;
                    update();
                }
            } else {
                // the track moves with the hand: drag left to go forward
                m_deck->setPreview(m_scrubFrom - dsec);
            }
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
        setCursor(m_gridEdit ? Qt::SizeHorCursor : Qt::OpenHandCursor);
        if (m_scrubbing) {
            m_scrubbing = false;
            if (m_gridEdit) {
                const double shift = m_gridShift;
                m_gridShift = 0.0;
                if (shift != 0.0)
                    m_deck->nudgeGrid(shift * 1000.0);
                update();
            } else {
                m_deck->commitPreview();
            }
        }
        return;
    }
    if (!m_loopDrag) {
        m_deck->commitPreview();
    } else if (m_dragging) {
        m_dragging = false;
        if (std::abs(m_dragB - m_dragA) >= 1.0)
            m_deck->setCustomBeats(std::min(m_dragA, m_dragB), std::max(m_dragA, m_dragB)); // an In / Out loop
        update();
    } else {
        m_deck->seek(secAtX(e->position().x()));
    }
}

void WaveformItem::mouseDoubleClickEvent(QMouseEvent *e)
{
    // grid edit: the beat nearest the click becomes a downbeat
    if (!m_zoom || !m_gridEdit || !m_deck || !m_deck->buffer()) {
        QQuickItem::mouseDoubleClickEvent(e);
        return;
    }
    const BeatGrid g = m_deck->grid();
    if (!g.valid())
        return;
    const double span = m_zoomBeats * g.secPerBeat();
    const double sec = m_deck->position() + (e->position().x() / std::max(1.0, width()) - 0.5) * span;
    m_deck->setDownbeatAt(g.secAtBeat(std::round(g.beatAtSec(sec))));
    e->accept();
}
