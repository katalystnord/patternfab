#include "NoiseFloorWidget.h"

#include <QFontMetrics>
#include <algorithm>
#include <QPainter>
#include <QPaintEvent>

NoiseFloorWidget::NoiseFloorWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(380, 380);
}

void NoiseFloorWidget::showMap(const patternfab::NoiseFloorMap &map,
                               const patternfab::NoiseFloorScale &scale) {
    scale_ = scale;
    subsetRadiusPx_ = map.subsetRadiusPx;

    const patternfab::NoiseFloorPicture picture = patternfab::drawNoiseFloor(map, scale);
    image_ = QImage(picture.widthPx, picture.heightPx, QImage::Format_RGB888);
    for (int y = 0; y < picture.heightPx; ++y) {
        auto *row = reinterpret_cast<uchar *>(image_.scanLine(y));
        for (int x = 0; x < picture.widthPx; ++x) {
            const patternfab::Rgb8 &c =
                picture.pixels[static_cast<std::size_t>(y) * picture.widthPx + x];
            row[3 * x + 0] = c.r;
            row[3 * x + 1] = c.g;
            row[3 * x + 2] = c.b;
        }
    }
    hasContent_ = !image_.isNull() && picture.widthPx > 0 && picture.heightPx > 0;
    update();
}

void NoiseFloorWidget::clear() {
    hasContent_ = false;
    image_ = QImage();
    update();
}

void NoiseFloorWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0x20, 0x22, 0x26));

    if (!hasContent_) {
        painter.setPen(QColor(0x99, 0x99, 0x99));
        painter.drawText(rect(), Qt::AlignCenter, tr("Load a pattern to see where it can measure"));
        return;
    }

    const QFontMetrics metrics(painter.font());
    const int barHeight = 22;
    const int labelHeight = metrics.height() + 4;
    const int margin = 14;
    const int footer = barHeight + labelHeight + margin;

    const QRectF area = QRectF(rect()).adjusted(margin, margin, -margin, -(footer + margin));
    if (area.isEmpty()) {
        return;
    }

    // ⚑ Nearest neighbour, not smooth scaling. Interpolating blends the colour
    // that means "nothing was established here" into the ramp, so the border of
    // every hole would acquire a run of perfectly plausible readings that the
    // pattern does not support.
    const QSize scaled = image_.size().scaled(area.size().toSize(), Qt::KeepAspectRatio);
    const QRectF target(area.left() + (area.width() - scaled.width()) / 2.0,
                        area.top() + (area.height() - scaled.height()) / 2.0,
                        scaled.width(), scaled.height());
    painter.drawImage(target, image_.scaled(scaled, Qt::KeepAspectRatio, Qt::FastTransformation));
    painter.setPen(QColor(0x55, 0x59, 0x5f));
    painter.drawRect(target);

    // --- the scale bar -------------------------------------------------
    const int barTop = height() - margin - barHeight - labelHeight;
    const QRect bar(margin, barTop, width() - 2 * margin, barHeight);

    if (!scale_.usable) {
        painter.setPen(QColor(0xdd, 0xdd, 0xdd));
        painter.drawText(bar, Qt::AlignLeft | Qt::AlignVCenter,
                         tr("No floor established anywhere (see the panel)."));
        return;
    }

    // The absent colour sits ON the bar rather than only in the prose, because
    // grey over a quarter of the picture is the first thing a reader asks
    // about and the answer must be where they are looking.
    //
    // ⚑ It is given a width of its own, wide enough for the word under it, and
    // the ramp is shortened to make room. Sized to the swatch alone, the word
    // "none" and the scale's top label were drawn over one another -- found by
    // looking at the window, not by a test.
    const int swatch = std::max(barHeight, metrics.horizontalAdvance(tr("none")) + 6);
    const QRect absent(bar.right() - swatch, bar.top(), swatch, barHeight);
    painter.fillRect(absent, QColor(patternfab::kNoFloorColour.r, patternfab::kNoFloorColour.g,
                                    patternfab::kNoFloorColour.b));

    const int gap = 10;
    const int rampWidth = std::max(1, bar.width() - swatch - gap);
    for (int x = 0; x < rampWidth; ++x) {
        const double position = rampWidth > 1
                                    ? static_cast<double>(x) / (rampWidth - 1)
                                    : 0.0;
        const patternfab::Rgb8 c = patternfab::noiseFloorColour(position);
        painter.setPen(QColor(c.r, c.g, c.b));
        painter.drawLine(bar.left() + x, bar.top(), bar.left() + x, bar.bottom());
    }

    painter.setPen(QColor(0x55, 0x59, 0x5f));
    painter.drawRect(QRect(bar.left(), bar.top(), rampWidth, barHeight));
    painter.drawRect(absent);

    const std::vector<std::string> labels = patternfab::noiseFloorScaleLabels(scale_, 4);
    painter.setPen(QColor(0xdd, 0xdd, 0xdd));
    const QRect labelRow(bar.left(), bar.bottom() + 2, rampWidth, labelHeight);
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const QString text = QString::fromStdString(labels[i]);
        const int textWidth = metrics.horizontalAdvance(text);
        const double fraction = labels.size() > 1
                                    ? static_cast<double>(i) / (labels.size() - 1)
                                    : 0.0;
        int x = labelRow.left() + static_cast<int>(fraction * labelRow.width());
        // The end labels are pulled inside the bar rather than centred on its
        // ends: centred, the first loses its leading digits off the left edge
        // and the last runs off the right, which SurView's comparison window
        // paid for once already.
        if (i == 0) {
            x = labelRow.left();
        } else if (i + 1 == labels.size()) {
            x = labelRow.right() - textWidth;
        } else {
            x -= textWidth / 2;
        }
        painter.drawText(x, labelRow.bottom(), text);
    }
    const QString noneText = tr("none");
    painter.drawText(absent.center().x() - metrics.horizontalAdvance(noneText) / 2,
                     labelRow.bottom(), noneText);
}
