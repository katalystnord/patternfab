#include "PreviewWidget.h"

#include <QPainter>
#include <QPaintEvent>

PreviewWidget::PreviewWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(380, 380);
}

void PreviewWidget::showSvg(const QString &path) {
    hasContent_ = renderer_.load(path) && renderer_.isValid();
    update();
}

void PreviewWidget::clear() {
    hasContent_ = false;
    update();
}

void PreviewWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);

    if (!hasContent_) {
        painter.setPen(QColor(0x99, 0x99, 0x99));
        painter.drawText(rect(), Qt::AlignCenter, tr("Load a pattern to preview"));
        return;
    }

    // Fit the SVG into the widget preserving aspect ratio, centered, with a
    // small margin so the specimen edge is visible against the ground.
    const qreal margin = 14.0;
    const QRectF area = QRectF(rect()).adjusted(margin, margin, -margin, -margin);
    const QSizeF svgSize = renderer_.defaultSize();
    if (svgSize.isEmpty() || area.isEmpty()) {
        return;
    }
    const QSizeF scaled = svgSize.scaled(area.size(), Qt::KeepAspectRatio);
    const QRectF target(area.left() + (area.width() - scaled.width()) / 2.0,
                        area.top() + (area.height() - scaled.height()) / 2.0,
                        scaled.width(), scaled.height());

    // A faint frame around the specimen bounds aids reading the extent.
    painter.setPen(QColor(0xcc, 0xcc, 0xcc));
    painter.drawRect(target);
    renderer_.render(&painter, target);
}
