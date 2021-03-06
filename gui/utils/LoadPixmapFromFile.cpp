#include "stdafx.h"

#include "exif.h"
#include "utils.h"

#include "LoadPixmapFromFileTask.h"

namespace Utils
{
    LoadPixmapFromFileTask::LoadPixmapFromFileTask(const QString& path)
        : Path_(path)
    {
        assert(!Path_.isEmpty());
        assert(QFile::exists(Path_));
    }

    LoadPixmapFromFileTask::~LoadPixmapFromFileTask()
    {
    }

    void LoadPixmapFromFileTask::run()
    {
        if (!QFile::exists(Path_))
        {
            emit loadedSignal(QPixmap());
            return;
        }

        QFile file(Path_);
        if (!file.open(QIODevice::ReadOnly))
        {
            emit loadedSignal(QPixmap());
            return;
        }

        const auto data = file.readAll();

        QPixmap preview;
        Utils::loadPixmap(data, Out preview);

        if (preview.isNull())
        {
            emit loadedSignal(QPixmap());
            return;
        }

        const auto orientation = Exif::getExifOrientation((char*)data.data(), (size_t)data.size());

        Exif::applyExifOrientation(orientation, InOut preview);

        assert(!preview.isNull());
        emit loadedSignal(preview);
    }
}