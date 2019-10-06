#include "graphicsview.h"

#include "graphicsscene.h"

#include <QDebug>
#include <QMouseEvent>
#include <QScrollBar>
#include <QMimeData>
#include <QImageReader>

GraphicsView::GraphicsView(QWidget *parent)
    : QGraphicsView (parent)
{
    setDragMode(QGraphicsView::ScrollHandDrag);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setStyleSheet("background-color: rgba(0, 0, 0, 220);"
                  "border-radius: 3px;");
    setAcceptDrops(true);
    setCheckerboardEnabled(false);

    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, &GraphicsView::viewportRectChanged);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &GraphicsView::viewportRectChanged);
}

void GraphicsView::showFromUrlList(const QList<QUrl> &urlList)
{
    emit navigatorViewRequired(false, 0);
    if (urlList.isEmpty()) {
        // yeah, it's possible. dragging QQ's original sticker will trigger this, for example.
        showText(tr("File url list is empty"));
        return;
    }
    QUrl url(urlList.first());
    QString filePath(url.toLocalFile());

    if (filePath.endsWith(".svg")) {
        showSvg(filePath);
    } else if (filePath.endsWith(".gif")) {
        showGif(filePath);
    } else {
        QImageReader imageReader(filePath);
        QImage::Format imageFormat = imageReader.imageFormat();
        if (imageFormat == QImage::Format_Invalid) {
            showText(tr("File is not a valid image"));
        } else {
            showImage(QPixmap::fromImageReader(&imageReader));
        }
    }
}

void GraphicsView::showImage(const QPixmap &pixmap)
{
    resetTransform();
    scene()->showImage(pixmap);
    checkAndDoFitInView();
}

void GraphicsView::showText(const QString &text)
{
    resetTransform();
    scene()->showText(text);
    checkAndDoFitInView();
}

void GraphicsView::showSvg(const QString &filepath)
{
    resetTransform();
    scene()->showSvg(filepath);
    checkAndDoFitInView();
}

void GraphicsView::showGif(const QString &filepath)
{
    resetTransform();
    scene()->showGif(filepath);
    checkAndDoFitInView();
}

GraphicsScene *GraphicsView::scene() const
{
    return qobject_cast<GraphicsScene*>(QGraphicsView::scene());
}

void GraphicsView::setScene(GraphicsScene *scene)
{
    return QGraphicsView::setScene(scene);
}

qreal GraphicsView::scaleFactor() const
{
    int angle = static_cast<int>(m_rotateAngle);
    if (angle == 0 || angle == 180) {
        return qAbs(transform().m11());
    } else {
        return qAbs(transform().m12());
    }
}

void GraphicsView::resetTransform()
{
    m_scaleFactor = 1;
    m_rotateAngle = 0;
    QGraphicsView::resetTransform();
}

void GraphicsView::zoomView(qreal scaleFactor)
{
    m_enableFitInView = false;
    m_scaleFactor *= scaleFactor;
    reapplyViewTransform();
    emit navigatorViewRequired(!isThingSmallerThanWindowWith(transform()), m_rotateAngle);
}

void GraphicsView::resetScale()
{
    m_scaleFactor = 1;
    reapplyViewTransform();
    emit navigatorViewRequired(!isThingSmallerThanWindowWith(transform()), m_rotateAngle);
}

void GraphicsView::rotateView(qreal rotateAngel)
{
    m_rotateAngle += rotateAngel;
    m_rotateAngle = static_cast<int>(m_rotateAngle) % 360;
    reapplyViewTransform();
}

void GraphicsView::fitInView(const QRectF &rect, Qt::AspectRatioMode aspectRadioMode)
{
    QGraphicsView::fitInView(rect, aspectRadioMode);
    m_scaleFactor = scaleFactor();
}

void GraphicsView::checkAndDoFitInView()
{
    if (!isThingSmallerThanWindowWith(transform())) {
        m_enableFitInView = true;
        fitInView(sceneRect(), Qt::KeepAspectRatio);
    }
}

void GraphicsView::toggleCheckerboard()
{
    setCheckerboardEnabled(!m_checkerboardEnabled);
}

void GraphicsView::mousePressEvent(QMouseEvent *event)
{
    if (shouldIgnoreMousePressMoveEvent(event)) {
        event->ignore();
        // blumia: return here, or the QMouseEvent event transparency won't
        //         work if we set a QGraphicsView::ScrollHandDrag drag mode.
        return;
    }

    return QGraphicsView::mousePressEvent(event);
}

void GraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    if (shouldIgnoreMousePressMoveEvent(event)) {
        event->ignore();
    }

    return QGraphicsView::mouseMoveEvent(event);
}

void GraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    QGraphicsItem *item = itemAt(event->pos());
    if (!item) {
        event->ignore();
    }

    return QGraphicsView::mouseReleaseEvent(event);
}

void GraphicsView::wheelEvent(QWheelEvent *event)
{
    event->ignore();

    return QGraphicsView::wheelEvent(event);
}

void GraphicsView::resizeEvent(QResizeEvent *event)
{
    if (m_enableFitInView) {
        QTransform tf;
        tf.rotate(m_rotateAngle);
        if (isThingSmallerThanWindowWith(tf) && scaleFactor() >= 1) {
            // no longer need to do fitInView()
            // but we leave the m_enableFitInView value unchanged in case
            // user resize down the window again.
        } else {
            fitInView(sceneRect(), Qt::KeepAspectRatio);
        }
    } else {
        emit navigatorViewRequired(!isThingSmallerThanWindowWith(transform()), m_rotateAngle);
    }
    return QGraphicsView::resizeEvent(event);
}

void GraphicsView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasImage() || event->mimeData()->hasText()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
    qDebug() << event->mimeData() << "Drag Enter Event"
             << event->mimeData()->hasUrls() << event->mimeData()->hasImage()
             << event->mimeData()->formats() << event->mimeData()->hasFormat("text/uri-list");

    return QGraphicsView::dragEnterEvent(event);
}

void GraphicsView::dragMoveEvent(QDragMoveEvent *event)
{
    Q_UNUSED(event)
    // by default, QGraphicsView/Scene will ignore the action if there are no QGraphicsItem under cursor.
    // We actually doesn't care and would like to keep the drag event as-is, so just do nothing here.
}

void GraphicsView::dropEvent(QDropEvent *event)
{
    event->acceptProposedAction();

    const QMimeData * mimeData = event->mimeData();

    if (mimeData->hasUrls()) {
        showFromUrlList(mimeData->urls());
    } else if (mimeData->hasImage()) {
        QImage img = qvariant_cast<QImage>(mimeData->imageData());
        QPixmap pixmap = QPixmap::fromImage(img);
        if (pixmap.isNull()) {
            showText(tr("Image data is invalid"));
        } else {
            showImage(pixmap);
        }
    } else if (mimeData->hasText()) {
        showText(mimeData->text());
    } else {
        showText(tr("Not supported mimedata: %1").arg(mimeData->formats().first()));
    }
}

bool GraphicsView::isThingSmallerThanWindowWith(const QTransform &transform) const
{
    return rect().size().expandedTo(transform.mapRect(sceneRect()).size().toSize())
            == rect().size();
}

bool GraphicsView::shouldIgnoreMousePressMoveEvent(const QMouseEvent *event) const
{
    if (event->buttons() == Qt::NoButton) {
        return true;
    }

    QGraphicsItem *item = itemAt(event->pos());
    if (!item) {
        return true;
    }

    if (isThingSmallerThanWindowWith(transform())) {
        return true;
    }

    return false;
}

void GraphicsView::setCheckerboardEnabled(bool enabled)
{
    m_checkerboardEnabled = enabled;
    if (m_checkerboardEnabled) {
        // Prepare background check-board pattern
        QPixmap tilePixmap(0x20, 0x20);
        tilePixmap.fill(QColor(35, 35, 35, 110));
        QPainter tilePainter(&tilePixmap);
        QColor color(40, 40, 40, 110);
        tilePainter.fillRect(0, 0, 0x10, 0x10, color);
        tilePainter.fillRect(0x10, 0x10, 0x10, 0x10, color);
        tilePainter.end();

        setBackgroundBrush(tilePixmap);
    } else {
        setBackgroundBrush(Qt::transparent);
    }
}

void GraphicsView::reapplyViewTransform()
{
    QGraphicsView::resetTransform();
    scale(m_scaleFactor, m_scaleFactor);
    rotate(m_rotateAngle);
}
