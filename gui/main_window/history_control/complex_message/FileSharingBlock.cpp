#include "stdafx.h"

#include "../../../../corelib/enumerations.h"

#include "../../../core_dispatcher.h"
#include "../../../controls/CommonStyle.h"
#include "../../../controls/TextEmojiWidget.h"
#include "../../../gui_settings.h"
#include "../../../main_window/MainWindow.h"
#include "../../../previewer/GalleryWidget.h"
#include "../../../previewer/Previewer.h"
#include "../../../themes/ResourceIds.h"
#include "../../../themes/ThemePixmap.h"
#include "../../../utils/InterConnector.h"
#include "../../../utils/LoadMovieFromFileTask.h"
#include "../../../utils/LoadPixmapFromFileTask.h"
#include "../../../utils/log/log.h"
#include "../../../utils/PainterPath.h"
#include "../../../utils/utils.h"
#include "../../contact_list/ContactListModel.h"

#include "../ActionButtonWidget.h"
#include "../FileSizeFormatter.h"
#include "../KnownFileTypes.h"
#include "../MessageStyle.h"

#include "ComplexMessageItem.h"
#include "FileSharingImagePreviewBlockLayout.h"
#include "FileSharingPlainBlockLayout.h"
#include "FileSharingUtils.h"
#include "Style.h"



#include "FileSharingBlock.h"

#ifdef __APPLE__
#include "../../../utils/macos/mac_support.h"
#endif

UI_COMPLEX_MESSAGE_NS_BEGIN

namespace
{
    QPen getFileSizePen();
}

FileSharingBlock::FileSharingBlock(
    ComplexMessageItem *parent,
    const QString &link,
    const core::file_sharing_content_type type)
    : FileSharingBlockBase(parent, link, type)
    , PreviewRequestId_(-1)
    , CtrlButton_(nullptr)
    , IsBodyPressed_(false)
    , IsShowInDirLinkPressed_(false)
    , OpenPreviewer_(false)
    , SnapId_(0)
    , SnapMetainfoRequestId_(-1)
    , IsFailedSnap_(false)
    , SnapAuthorNickCtrl_(nullptr)
    , IsPausedByUser_(false)
{
    parseLink();

    if (isPreviewable())
    {
        initPreview();
    }
    else
    {
        initPlainFile();
    }
}

FileSharingBlock::~FileSharingBlock()
{
}

QFont FileSharingBlock::getAuthorNickFont() const
{
    return Fonts::appFontScaled(15);
}

QSize FileSharingBlock::getCtrlButtonSize() const
{
    assert(CtrlButton_);

    if (CtrlButton_)
    {
        return CtrlButton_->sizeHint();
    }

    return QSize();
}

QSize FileSharingBlock::getFailedSnapSizeMax() const
{
    return QSize(Utils::scale_value(240), Utils::scale_value(240));
}

QSize FileSharingBlock::getOriginalPreviewSize() const
{
    assert(isPreviewable());
    assert(!isFailedSnap());
    assert(!OriginalPreviewSize_.isEmpty());

    return OriginalPreviewSize_;
}

QString FileSharingBlock::getShowInDirLinkText() const
{
    return QT_TRANSLATE_NOOP("chat_page","Show in folder");
}

bool FileSharingBlock::isAuthorVisible() const
{
    return (isSnap() && !isFailedSnap());
}

bool FileSharingBlock::isFailedSnap() const
{
    if (IsFailedSnap_)
    {
        assert(isPreviewable());
        return true;
    }

    return false;
}

bool FileSharingBlock::isPreviewReady() const
{
    return !Preview_.isNull();
}

bool FileSharingBlock::isSharingEnabled() const
{
    return !IsFailedSnap_;
}

void FileSharingBlock::onVisibilityChanged(const bool isVisible)
{
    FileSharingBlockBase::onVisibilityChanged(isVisible);

    if (isGifImage())
    {
        onGifImageVisibilityChanged(isVisible);
    }
}

void FileSharingBlock::setAuthorNickGeometry(const QRect &rect)
{
    assert(!rect.isEmpty());
    assert(isAuthorVisible());

    if (!SnapAuthorNickCtrl_)
    {
        return;
    }

    SnapAuthorNickCtrl_->setGeometry(rect);
    SnapAuthorNickCtrl_->setVisible(true);
}

void FileSharingBlock::setCtrlButtonGeometry(const QRect &rect)
{
    assert(!rect.isEmpty());
    assert(CtrlButton_);
    assert(!isFailedSnap());

    if (!CtrlButton_)
    {
        return;
    }

    const auto bias = CtrlButton_->getCenterBias();
    const auto fixedRect = rect.translated(-bias);
    CtrlButton_->setGeometry(fixedRect);

    const auto isButtonVisible = (!isGifImage() || !isGifPlaying());
    CtrlButton_->setVisible(isButtonVisible);
}

void FileSharingBlock::drawBlock(QPainter &p)
{
    const auto &contentRect = getFileSharingLayout()->getContentRect();
    assert(!contentRect.isEmpty());

    if (isPreviewable())
    {
        QRect authorAvatarRect;
        QRect authorNickRect;

        if (isSnap())
        {
            authorAvatarRect = getFileSharingLayout()->getAuthorAvatarRect();
            authorNickRect = getFileSharingLayout()->getAuthorNickRect();
        }

        drawPreviewableBlock(p, contentRect, authorAvatarRect, authorNickRect);
    }
    else
    {
        drawPlainFileBlock(p, contentRect);
    }

    LastContentRect_ = contentRect;
}

void FileSharingBlock::enterEvent(QEvent* event)
{
    IsBodyPressed_ = false;
    IsShowInDirLinkPressed_ = false;

    GenericBlock::enterEvent(event);
}

void FileSharingBlock::initializeFileSharingBlock()
{
    setCursor(Qt::PointingHandCursor);

    connectGeneralSignals(true);

    if (isSnap())
    {
        requestSnapMetainfo();

        return;
    }

    sendGenericMetainfoRequests();
}

void FileSharingBlock::leaveEvent(QEvent*)
{
    IsBodyPressed_ = false;
    IsShowInDirLinkPressed_ = false;
}

void FileSharingBlock::mousePressEvent(QMouseEvent *event)
{
    event->ignore();

    IsShowInDirLinkPressed_ = false;
    IsBodyPressed_ = false;

    if (IsFailedSnap_)
    {
        return;
    }

    const auto isLeftButton = (event->button() == Qt::LeftButton);
    if (!isLeftButton)
    {
        return;
    }

    if (isPreviewable())
    {
        IsBodyPressed_ = true;
        return;
    }

    const auto &showInDirLinkRect = getFileSharingLayout()->getShowInDirLinkRect();
    const auto isOverLink = showInDirLinkRect.contains(event->pos());
    if (isOverLink && isFileDownloaded())
    {
        IsShowInDirLinkPressed_ = true;
    }
    else
    {
        IsBodyPressed_ = true;
    }
}

void FileSharingBlock::mouseReleaseEvent(QMouseEvent *event)
{
    event->ignore();

    const auto &showInDirLinkRect = getFileSharingLayout()->getShowInDirLinkRect();
    const auto isOverLink = showInDirLinkRect.contains(event->pos());
    if (isOverLink && IsShowInDirLinkPressed_)
    {
        showFileInDir();
    }

    if (IsBodyPressed_)
    {
        onLeftMouseClick(event->globalPos());
    }

    IsBodyPressed_ = false;
    IsShowInDirLinkPressed_ = false;
}

void FileSharingBlock::applyClippingPath(QPainter &p, const QRect &previewRect)
{
    assert(isPreviewable());
    assert(!previewRect.isEmpty());

    const auto isPreviewRectChanged = (LastContentRect_ != previewRect);
    const auto shouldResetClippingPath = (PreviewClippingPath_.isEmpty() || isPreviewRectChanged);
    if (shouldResetClippingPath)
    {
        PreviewClippingPath_ = Utils::renderMessageBubble(previewRect, MessageStyle::getBorderRadius(), isOutgoing());
    }

    p.setClipPath(PreviewClippingPath_);
}

void FileSharingBlock::changeGifPlaybackStatus(const bool isPlaying)
{
    assert(isGifImage());

    if (CtrlButton_)
    {
        CtrlButton_->setVisible(!isPlaying);
    }
}

void FileSharingBlock::connectGeneralSignals(const bool isConnected)
{
    if (isConnected)
    {
        QMetaObject::Connection connection;

        connection = QObject::connect(
            GetDispatcher(),
            &core_dispatcher::snapMetainfoDownloaded,
            this,
            &FileSharingBlock::onSnapMetainfoDownloaded,
            (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));
        assert(connection);

        if (isSnap())
        {
            connection = QObject::connect(
                Logic::GetAvatarStorage(),
                &Logic::AvatarStorage::avatarChanged,
                this,
                &FileSharingBlock::onSnapAuthorAvatarChanged,
                (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));
            assert(connection);
        }

        return;
    }

    QObject::disconnect(
        GetDispatcher(),
        &core_dispatcher::snapMetainfoDownloaded,
        this,
        &FileSharingBlock::onSnapMetainfoDownloaded);

    if (isSnap())
    {
        QObject::disconnect(
            Logic::GetAvatarStorage(),
            &Logic::AvatarStorage::avatarChanged,
            this,
            &FileSharingBlock::onSnapAuthorAvatarChanged);
    }
}

void FileSharingBlock::connectImageSignals(const bool isConnected)
{
    if (isConnected)
    {
        QMetaObject::Connection connection;

        connection = QObject::connect(
            GetDispatcher(),
            &core_dispatcher::imageDownloaded,
            this,
            &FileSharingBlock::onImageDownloaded,
            (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));
        assert(connection);

        connection = QObject::connect(
            GetDispatcher(),
            &core_dispatcher::imageDownloadingProgress,
            this,
            &FileSharingBlock::onImageDownloadingProgress,
            (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));
        assert(connection);

        connection = QObject::connect(
            GetDispatcher(),
            &core_dispatcher::imageMetaDownloaded,
            this,
            &FileSharingBlock::onImageMetaDownloaded,
            (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));
        assert(connection);

        connection = QObject::connect(
            GetDispatcher(),
            &core_dispatcher::imageDownloadError,
            this,
            &FileSharingBlock::onImageDownloadError,
            (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));
        assert(connection);

        return;
    }

    QObject::disconnect(
        GetDispatcher(),
        &core_dispatcher::imageDownloaded,
        this,
        &FileSharingBlock::onImageDownloaded);

    QObject::disconnect(
        GetDispatcher(),
        &core_dispatcher::imageDownloadingProgress,
        this,
        &FileSharingBlock::onImageDownloadingProgress);

    QObject::disconnect(
        GetDispatcher(),
        &core_dispatcher::imageMetaDownloaded,
        this,
        &FileSharingBlock::onImageMetaDownloaded);

    QObject::disconnect(
        GetDispatcher(),
        &core_dispatcher::imageDownloadError,
        this,
        &FileSharingBlock::onImageDownloadError);
}

void FileSharingBlock::createAuthorNickControl(const QString &_authorName)
{
    assert(!SnapAuthorNickCtrl_);
    assert(!_authorName.isEmpty());

    const auto font = getAuthorNickFont();

    QColor color(0x28, 0x28, 0x28);
    SnapAuthorNickCtrl_ = new TextEmojiWidget(
        this,
        Fonts::defaultAppFontFamily(),
        Fonts::defaultAppFontStyle(),
        font.pixelSize(),
        color);

    SnapAuthorNickCtrl_->setText(_authorName, color);
}

void FileSharingBlock::drawFailedSnap(QPainter &p, const QRect &previewRect)
{
    assert(isFailedSnap());
    assert(SnapExpiredImage_);

    // draw bubble

    p.save();

    const auto fillWithDefaultBrush = (isStandalone() || isSelected());
    if (fillWithDefaultBrush)
    {
        const auto &bodyBrush = MessageStyle::getBodyBrush(isOutgoing(), isSelected(), getThemeId());
        p.setBrush(bodyBrush);
    }
    else
    {
        const auto bgAlpha = ((7 * 255) / 100);
        QBrush bgBrush(QColor(0x00, 0x00, 0x00, bgAlpha));
        p.setBrush(bgBrush);
    }

    p.drawRect(previewRect);

    p.restore();

    // draw icon

    auto iconRect = SnapExpiredImage_->GetRect();
    iconRect.moveCenter(previewRect.center());
    SnapExpiredImage_->Draw(p, iconRect);

    // draw text

    const auto labelFont = Fonts::appFontScaled(18);
    const QFontMetrics labelFontMetrics(labelFont);

    const auto textLeft = previewRect.left();
    const auto textWidth = previewRect.width();
    const auto textHeight = labelFontMetrics.height();

    const auto LABEL_BASELINE_OFFSET = Utils::scale_value(32);
    auto textTop = (
        iconRect.bottom() + 1 +
        LABEL_BASELINE_OFFSET -
        textHeight);

    const QRect textRect(textLeft, textTop, textWidth, textHeight);

    const auto textAlpha = ((50 * 255) / 100);
    const QColor textColor(0x28, 0x28, 0x28, textAlpha);
    const QPen textPen(textColor);

    p.save();

    p.setPen(textPen);
    p.setFont(labelFont);

    const auto labelText = QT_TRANSLATE_NOOP("chat_page","Snap expired");

    p.drawText(textRect, Qt::AlignHCenter, labelText);

    p.restore();
}

void FileSharingBlock::drawPlainFileBlock(QPainter &p, const QRect &frameRect)
{
    assert(isPlainFile());

    if (isStandalone())
    {
        drawPlainFileBubble(p, frameRect);
    }
    else
    {
        drawPlainFileFrame(p, frameRect);
    }

    const auto layout = getFileSharingLayout();

    const auto &filenameRect = layout->getFilenameRect();
    drawPlainFileName(p, filenameRect);

    const auto text = getProgressText();
    if (!text.isEmpty())
    {
        const auto &fileSizeRect = layout->getFileSizeRect();
        drawPlainFileSizeAndProgress(p, text, fileSizeRect);
    }

    const auto &showInDirLinkRect = layout->getShowInDirLinkRect();
    drawPlainFileShowInDirLink(p, showInDirLinkRect);
}

void FileSharingBlock::drawPlainFileBubble(QPainter &p, const QRect &bubbleRect)
{
    assert(isStandalone());

    p.save();

    const auto &bodyBrush = MessageStyle::getBodyBrush(isOutgoing(), isSelected(), getThemeId());
    p.setBrush(bodyBrush);

    p.drawRoundedRect(
        bubbleRect,
        MessageStyle::getBorderRadius(),
        MessageStyle::getBorderRadius());

    p.restore();
}


void FileSharingBlock::drawPlainFileFrame(QPainter &p, const QRect &frameRect)
{
    assert(!isStandalone());

    p.save();

    p.setPen(Style::getFileSharingFramePen());

    p.drawRoundedRect(frameRect, MessageStyle::getBorderRadius(), MessageStyle::getBorderRadius());

    p.restore();
}

void FileSharingBlock::drawPlainFileName(QPainter &p, const QRect &filenameRect)
{
    assert(isPlainFile());

    const auto font = getFileSharingLayout()->getFilenameFont();

    QString text;

    if (!getFilename().isEmpty())
    {
        QFontMetrics fontMetrics(font);
        text = fontMetrics.elidedText(getFilename(), Qt::ElideRight, filenameRect.width());
    }
    else
    {
        text = QT_TRANSLATE_NOOP("contact_list", "File");
    }

    p.save();

    p.setFont(font);
    p.setPen(Ui::CommonStyle::getTextCommonColor());

    p.drawText(filenameRect, text);

    p.restore();
}

void FileSharingBlock::drawPlainFileShowInDirLink(QPainter &p, const QRect &linkRect)
{
    if (linkRect.isEmpty())
    {
        return;
    }

    assert(!linkRect.isEmpty());

    if (!isFileDownloaded())
    {
        return;
    }

    p.save();

    const auto &btnText = getShowInDirLinkText();

    p.setFont(getFileSharingLayout()->getShowInDirLinkFont());
    p.setPen(QPen(Ui::CommonStyle::getLinkColor()));
    p.drawText(linkRect, btnText);

    p.restore();
}

void FileSharingBlock::drawPlainFileSizeAndProgress(QPainter &p, const QString &text, const QRect &fileSizeRect)
{
    assert(!fileSizeRect.isEmpty());
    assert(!text.isEmpty());

    if (getFileSize() <= 0)
    {
        return;
    }

    const auto font = getFileSharingLayout()->getFileSizeFont();

    p.save();

    p.setFont(font);
    p.setPen(getFileSizePen());

    p.drawText(fileSizeRect, text);

    p.restore();
}

void FileSharingBlock::drawPreview(QPainter &p, const QRect &previewRect)
{
    assert(isPreviewable());
    assert(!previewRect.isEmpty());

    if (isPreviewReady())
    {
        p.drawPixmap(previewRect, Preview_);
    }
    else
    {
        p.fillRect(
            previewRect,
            MessageStyle::getImagePlaceholderBrush());
    }

    if (isSelected())
    {
        const QBrush brush(Utils::getSelectionColor());
        p.fillRect(previewRect, brush);
    }
}

void FileSharingBlock::drawPreviewableBlock(QPainter &p, const QRect &previewRect, const QRect &authorAvatarRect, const QRect &authorNickRect)
{
    if (isAuthorVisible())
    {
        drawSnapAuthor(p, authorAvatarRect, authorNickRect, previewRect);
    }

    applyClippingPath(p, previewRect);

    if (isFailedSnap())
    {
        drawFailedSnap(p, previewRect);
        return;
    }

    drawPreview(p, previewRect);

    if (isFileDownloading())
    {
        p.fillRect(previewRect, MessageStyle::getImageShadeBrush());
    }
}

void FileSharingBlock::drawSnapAuthor(QPainter &p, const QRect &authorAvatarRect, const QRect &authorNameRect, const QRect &previewRect)
{
    assert(isSnap() && !isFailedSnap());
    assert(authorAvatarRect.isValid());
    assert(authorNameRect.isValid());
    assert(previewRect.isValid());

    if (isStandalone())
    {
        p.save();

        const auto brush = MessageStyle::getBodyBrush(isOutgoing(), false, getThemeId());
        p.setBrush(brush);

        const QRect authorNameBackground(
            0,
            0,
            previewRect.width() - 1,
            previewRect.height() / 2);
        p.drawRoundedRect(authorNameBackground, MessageStyle::getBorderRadius(), MessageStyle::getBorderRadius());

        p.restore();
    }

    if (SnapAuthorAvatar_)
    {
        assert(!SnapAuthorAvatar_->isNull());
        p.drawPixmap(authorAvatarRect, *SnapAuthorAvatar_);
    }
}

void FileSharingBlock::initPlainFile()
{
    auto layout = new FileSharingPlainBlockLayout();
    setBlockLayout(layout);
    setLayout(layout);

    auto resourceSet = ActionButtonWidget::ResourceSet::DownloadPlainFile_;

    assert(!CtrlButton_);
    CtrlButton_ = new ActionButtonWidget(resourceSet, this);

    QMetaObject::Connection connection;

    connection = QObject::connect(
        CtrlButton_,
        &ActionButtonWidget::startClickedSignal,
        this,
        [this]
        (QPoint globalClickCoords)
        {
            onLeftMouseClick(globalClickCoords);
        });
    assert(connection);

    connection = QObject::connect(
        CtrlButton_,
        &ActionButtonWidget::stopClickedSignal,
        this,
        [this]
        (QPoint globalClickCoords)
        {
            Q_UNUSED(globalClickCoords);

            if (isFileDownloading())
            {
                stopDownloading();
            }

            CtrlButton_->stopAnimation();

            notifyBlockContentsChanged();
        });
    assert(connection);

    auto connectionDrag = QObject::connect(
        CtrlButton_,
        &ActionButtonWidget::dragSignal,
        this,
        [this]
    ()
    {
        drag();
    });
}

void FileSharingBlock::initPreview()
{
    auto layout = new FileSharingImagePreviewBlockLayout();
    setBlockLayout(layout);
    setLayout(layout);

    auto resourceSet = &ActionButtonWidget::ResourceSet::DownloadMediaFile_;

    if (isGifImage())
    {
        resourceSet = &ActionButtonWidget::ResourceSet::Gif_;
    }
    else if (isVideo())
    {
        resourceSet = &ActionButtonWidget::ResourceSet::Play_;
    }

    CtrlButton_ = new ActionButtonWidget(*resourceSet, this);

    QMetaObject::Connection connection;

    connection = QObject::connect(
        CtrlButton_,
        &ActionButtonWidget::startClickedSignal,
        this,
        [this]
        (QPoint globalClickCoords)
        {
            onLeftMouseClick(globalClickCoords);
        });
    assert(connection);

    connection = QObject::connect(
        CtrlButton_,
        &ActionButtonWidget::stopClickedSignal,
        this,
        [this]
        (QPoint globalClickCoords)
        {
            Q_UNUSED(globalClickCoords);

            if (isFileDownloading())
            {
                stopDownloading();
            }

            CtrlButton_->stopAnimation();

            notifyBlockContentsChanged();
        });
        assert(connection);

    connection = QObject::connect(
        CtrlButton_,
        &ActionButtonWidget::dragSignal,
        this,
        [this]
        {
            drag();
        });
    assert(connection);
}

bool FileSharingBlock::isGifPlaying() const
{
    assert(isGifImage());

    return (GifImage_ && (GifImage_->state() == QMovie::Running));
}

bool FileSharingBlock::drag()
{
    auto fileTypeIcon = History::GetIconByFilename(getFilename());
    return Utils::dragUrl(this, isPreviewable() ? Preview_ : fileTypeIcon->GetPixmap(), getLink());
}

void FileSharingBlock::onDownloadingFailed()
{
    if (!isSnap())
    {
        getParentComplexMessage()->replaceBlockWithSourceText(this);
        return;
    }

    if (IsFailedSnap_)
    {
        // the temporary workaround
        return;
    }

    assert(isPreviewable());
    assert(!SnapExpiredImage_);
    assert(!IsFailedSnap_);

    IsFailedSnap_ = true;
    SnapExpiredImage_ = Themes::GetPixmap(Themes::PixmapResourceId::FileSharingSnapExpired);

    if (CtrlButton_)
    {
        CtrlButton_->setVisible(false);
    }

    if (SnapAuthorNickCtrl_)
    {
        SnapAuthorNickCtrl_->setVisible(false);
    }

    notifyBlockContentsChanged();
}

void FileSharingBlock::loadSnapAuthorAvatar(const QString &_uin, const QString &_nick)
{
    assert(isSnap());
    assert(!_uin.isEmpty());
    assert(!_nick.isEmpty());

    auto isDefault = false;

    SnapAuthorAvatar_ = Logic::GetAvatarStorage()->GetRounded(
        _uin,
        _nick,
        getFileSharingLayout()->getAuthorAvatarSize(),
        QString(),
        true,
        Out isDefault,
        false,
        false);
}

void FileSharingBlock::onDownloadingStarted()
{
    assert(CtrlButton_);

    if (shouldDisplayProgressAnimation())
    {
        CtrlButton_->startAnimation();
    }

    notifyBlockContentsChanged();
}

void FileSharingBlock::onDownloadingStopped()
{
    assert(CtrlButton_);

    if (CtrlButton_)
    {
        CtrlButton_->stopAnimation();
    }

    notifyBlockContentsChanged();
}

void FileSharingBlock::onDownloaded()
{
    CtrlButton_->stopAnimation();

    notifyBlockContentsChanged();

    if (getChatAimid() != Logic::getContactListModel()->selectedContact())
        return;

    if (OpenPreviewer_ && !isVideo())
    {
        OpenPreviewer_ = false;
        showImagePreviewer(getFileLocalPath());
    }

    if (isVideo() && !ffmpeg::is_enable_streaming())
    {
        Utils::InterConnector::instance().getMainWindow()->playVideo(getFileLocalPath());
    }

    if (isGifImage())
    {
        playGif(getFileLocalPath());
    }
}

void FileSharingBlock::onLeftMouseClick(const QPoint &globalPos)
{
    Q_UNUSED(globalPos);

    assert(!IsFailedSnap_);

    const auto clickOnSnapAuthorAvatar = (
        isSnap() &&
        getFileSharingLayout()->
            getAuthorAvatarRect().contains(
                mapFromGlobal(globalPos)));
    if (clickOnSnapAuthorAvatar)
    {
        emit Utils::InterConnector::instance().profileSettingsShow(SnapAuthorUin_);
        GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::profile_avatar);
        return;
    }

    const auto hasSnapId = (SnapId_ > 0);
    if (hasSnapId)
    {
        GetDispatcher()->read_snap(getSenderAimid(), SnapId_, false);
    }

    const auto isGifImageLoaded = (isGifImage() && isFileDownloaded());
    if (isGifImageLoaded)
    {
        IsPausedByUser_ = false;

        if (isGifPlaying())
        {
            IsPausedByUser_ = true;
            pauseGif();
        }
        else
        {
            playGif(getFileLocalPath());
        }

        return;
    }

    if (isImage())
    {
        if (isFileDownloading())
        {
            stopDownloading();
        }
        else
        {
            OpenPreviewer_ = true;
            startDownloading(true);
        }

        return;
    }

    if (isVideo())
    {
        if (ffmpeg::is_enable_streaming())
        {
            requestDirectUri(this, [](bool _res, const QString& _uri)
            {
                if (_res)
                {
                    Utils::InterConnector::instance().getMainWindow()->playVideo(_uri);
                }
            });
        }
        else
        {
            startDownloading(true);
        }
    }

    if (isPlainFile())
    {
        if (isFileDownloaded())
        {
            showFileInDir();
        }
        else
        {
            startDownloading(true);
        }
    }
}

void FileSharingBlock::onRestoreResources()
{
    if (isGifImage() && isFileDownloaded())
    {
        __TRACE(
            "resman",
            "restoring gif fs block\n"
            __LOGP(local, getFileLocalPath())
            __LOGP(uri, getLink()));

        playGif(getFileLocalPath());
    }

    FileSharingBlockBase::onRestoreResources();
}

void FileSharingBlock::onUnloadResources()
{
    if (GifImage_)
    {
        __TRACE(
            "resman",
            "unloading gif fs block\n"
            __LOGP(local, getFileLocalPath())
            __LOGP(uri, getLink()));

        GifImage_.reset();
    }

    FileSharingBlockBase::onUnloadResources();
}

void FileSharingBlock::parseLink()
{
    Id_ = extractIdFromFileSharingUri(getLink());
    assert(!Id_.isEmpty());

    if (isPreviewable())
    {
        OriginalPreviewSize_ = extractSizeFromFileSharingId(Id_);

        if (OriginalPreviewSize_.isEmpty())
        {
            OriginalPreviewSize_ = Utils::scale_value(QSize(64, 64));
        }
    }
}

void FileSharingBlock::pauseGif()
{
    assert(isGifImage());
    assert(GifImage_);

    GifImage_->setPaused(true);

    changeGifPlaybackStatus(false);
}

void FileSharingBlock::playGif(const QString &localPath)
{
    const auto isFileExists = QFile::exists(localPath);
    assert(isFileExists);
    assert(!IsPausedByUser_);

    if (isGifPlaying() || !isFileExists)
    {
        return;
    }

    if (GifImage_)
    {
        const auto &previewRect = getFileSharingLayout()->getContentRect();
        if (previewRect.isEmpty())
        {
            return;
        }

        changeGifPlaybackStatus(true);

        const auto gifSize = Utils::unscale_value(previewRect.size());
        GifImage_->setScaledSize(gifSize);

        GifImage_->start();

        return;
    }

    std::unique_ptr<Utils::LoadMovieFromFileTask> task(
        new Utils::LoadMovieFromFileTask(localPath));

    QObject::connect(
        task.get(),
        &Utils::LoadMovieFromFileTask::loadedSignal,
        this,
        [this, localPath]
        (QSharedPointer<QMovie> movie)
        {
            assert(movie);
            assert(!GifImage_);

            GifImage_ = movie;

            QObject::connect(
                GifImage_.data(),
                &QMovie::frameChanged,
                this,
                &FileSharingBlock::onGifFrameUpdated);

            playGif(localPath);
        }
    );

    task->run();
}

void FileSharingBlock::requestPreview(const QString &uri)
{
    assert(isPreviewable());
    assert(PreviewRequestId_ == -1);

    PreviewRequestId_ = GetDispatcher()->downloadImage(uri, getChatAimid(), QString(), false, 0, 0);
}

void FileSharingBlock::sendGenericMetainfoRequests()
{
    assert(!isSnap() || (SnapId_ > 0));

    if (isPreviewable())
    {
        connectImageSignals(true);

        requestMetainfo(true);
    }

    requestMetainfo(false);

    if (isPlainFile())
    {
        checkExistingLocalCopy();
    }
}

void FileSharingBlock::showFileInDir()
{
#ifdef _WIN32
    assert(QFile::exists(getFileLocalPath()));

    const auto param = ("/select," + QDir::toNativeSeparators(getFileLocalPath()));
    const auto command = "explorer " + param;
    QProcess::startDetached(command);
#else
#ifdef __APPLE__
    MacSupport::openFinder(getFileLocalPath());
#else
    QDir dir(getFileLocalPath());
    dir.cdUp();
    QDesktopServices::openUrl(dir.absolutePath());
#endif // __APPLE__
#endif //_WIN32
}

void FileSharingBlock::showImagePreviewer(const QString &localPath)
{
    assert(isImage());
    assert(!localPath.isEmpty());

    Utils::InterConnector::instance().getMainWindow()->openGallery(
        getChatAimid(), Data::Image(getParentComplexMessage()->getId(), getLink(), true), localPath);
}

void FileSharingBlock::requestSnapMetainfo()
{
    assert(SnapMetainfoRequestId_ == -1);
    assert(isSnap());

    __TRACE(
        "snippets",
        "requested snap metainfo\n"
            __LOGP(fsid, getFileSharingId())
            __LOGP(type, getType())
            __LOGP(contact, getChatAimid()));

    SnapMetainfoRequestId_ = GetDispatcher()->download_snap_metainfo(getChatAimid(), getFileSharingId());
}

bool FileSharingBlock::shouldDisplayProgressAnimation() const
{
    return (
        isPlainFile() ||
        (getFileSize() > Style::getMinFileSize4ProgressBar()));
}

void FileSharingBlock::unloadGif()
{
    assert(isGifImage());
    assert(GifImage_);

    GifImage_.reset();
}

void FileSharingBlock::updateFileTypeIcon()
{
    assert(isPlainFile());

    const auto &filename = getFilename();

    assert(!filename.isEmpty());
    if (filename.isEmpty())
    {
        return;
    }

    const auto iconId = History::GetIconIdByFilename(filename);

    auto resourceSet = CtrlButton_->getResourceSet();

    resourceSet.StartButtonImage_ = iconId;
    resourceSet.StartButtonImageActive_ = iconId;
    resourceSet.StartButtonImageHover_ = iconId;

    CtrlButton_->setResourceSet(resourceSet);
}

void FileSharingBlock::onDownloading(const int64_t _bytesTransferred, const int64_t _bytesTotal)
{
    assert(_bytesTotal > 0);
    if (_bytesTotal <= 0)
    {
        return;
    }

    if (!shouldDisplayProgressAnimation())
    {
        return;
    }

    const auto progress = ((double)_bytesTransferred / (double)_bytesTotal);
    assert(progress >= 0);

    CtrlButton_->setProgress(progress);

    if (isPreviewable())
    {
        const auto text = HistoryControl::formatProgressText(_bytesTotal, _bytesTransferred);
        assert(!text.isEmpty());

        CtrlButton_->setProgressText(text);
    }

    notifyBlockContentsChanged();
}

void FileSharingBlock::onMetainfoDownloaded()
{
    if (isGifImage())
    {
        startDownloading(false);
    }

    if (isPlainFile())
    {
        updateFileTypeIcon();

        notifyBlockContentsChanged();
    }
}

void FileSharingBlock::onGifFrameUpdated(int frameNumber)
{
    Q_UNUSED(frameNumber);

    const auto frame = GifImage_->currentPixmap();
    Preview_ = frame;

    update();
}

void FileSharingBlock::onImageDownloadError(qint64 seq, QString rawUri)
{
    rawUri;

    assert(seq > 0);

    if (PreviewRequestId_ != seq)
    {
        return;
    }

    PreviewRequestId_ = -1;

    onDownloadingFailed();
}

void FileSharingBlock::onImageDownloaded(int64_t seq, QString, QPixmap image, QString localPath)
{
    localPath;

    assert(!image.isNull());

    if (PreviewRequestId_ != seq)
    {
        return;
    }

    PreviewRequestId_ = -1;

    Preview_ = image;

    update();
}

void FileSharingBlock::onImageDownloadingProgress(qint64 seq, int64_t bytesTotal, int64_t bytesTransferred, int32_t pctTransferred)
{
    seq;
    bytesTotal;
    bytesTransferred;
    pctTransferred;

}

void FileSharingBlock::onImageMetaDownloaded(int64_t seq, Data::LinkMetadata meta)
{
    seq;
    meta;

    assert(isPreviewable());
}

void FileSharingBlock::onGifImageVisibilityChanged(const bool isVisible)
{
    assert(isGifImage());

    if (isVisible)
    {
        if (isFileDownloaded() && !IsPausedByUser_)
        {
            playGif(getFileLocalPath());
        }

        return;
    }

    if (isGifPlaying())
    {
        pauseGif();
    }
}

void FileSharingBlock::onLocalCopyInfoReady(const bool isCopyExists)
{
    if (isCopyExists)
    {
        update();
    }
}

void FileSharingBlock::onPreviewMetainfoDownloaded(const QString &miniPreviewUri, const QString &fullPreviewUri)
{
    const auto &previewUri = (
        fullPreviewUri.isEmpty() ?
            miniPreviewUri :
            fullPreviewUri);

    requestPreview(previewUri);
}

void FileSharingBlock::onSnapAuthorAvatarChanged(QString aimId)
{
    assert(!aimId.isEmpty());

    if (SnapAuthorUin_ != aimId)
    {
        return;
    }

    assert(!SnapAuthorNick_.isEmpty());

    loadSnapAuthorAvatar(SnapAuthorUin_, SnapAuthorNick_);

    notifyBlockContentsChanged();
}

void FileSharingBlock::onSnapMetainfoDownloaded(int64_t _seq, bool _success, uint64_t _snap_id, int64_t _expire_utc, QString _author_uin, QString _author_name)
{
    assert(_seq > 0);
    assert(!_success || (_snap_id > 0));

    if (SnapMetainfoRequestId_ != _seq)
    {
        return;
    }

    SnapMetainfoRequestId_ = -1;

    auto isExpired = false;

    const auto hasExpirationDate = (_expire_utc > 0);
    if (hasExpirationDate)
    {
        const auto expireUtc = QDateTime::fromTime_t(_expire_utc, Qt::UTC);

        isExpired = (QDateTime::currentDateTimeUtc() >= expireUtc);
    }

    const auto isDownloadingFailed = (!_success || isExpired);
    if (isDownloadingFailed)
    {
        onDownloadingFailed();
        return;
    }

    assert(SnapId_ == 0);
    SnapId_ = _snap_id;

    assert(SnapAuthorUin_.isEmpty());
    SnapAuthorUin_ = _author_uin;

    assert(SnapAuthorNick_.isEmpty());
    SnapAuthorNick_ = _author_name;

    const auto contactListName = Logic::getContactListModel()->getDisplayName(_author_uin);
    const auto hasContactListName = (contactListName != _author_uin);
    if (hasContactListName)
    {
        SnapAuthorNick_ = contactListName;
    }

    loadSnapAuthorAvatar(_author_uin, SnapAuthorNick_);

    createAuthorNickControl(SnapAuthorNick_);

    sendGenericMetainfoRequests();

    notifyBlockContentsChanged();
}

namespace
{
    QPen getFileSizePen()
    {
        return QPen(QColor(0x97, 0x97, 0x97));
    }
}

UI_COMPLEX_MESSAGE_NS_END
