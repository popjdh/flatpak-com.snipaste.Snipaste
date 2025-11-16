#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
#include <QUrl>
#include <QStringList>
#include <QDebug>
#include <iostream>
#include <QFileSystemWatcher>
#include <QDir>
#include <QSet>
#include <QFileInfo>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    QByteArray watchPathEnv = qgetenv("WATCH_PATH");
    if (watchPathEnv.isEmpty()) {
        qWarning() << "[Warning] Environment variable WATCH_PATH is not set, quitting.";
        return 1;
    }
    QString watchPath = QString::fromLocal8Bit(watchPathEnv);
    QDir watchDir(watchPath);

    if (!watchDir.exists()) {
        qWarning() << "[Error] The watch directory does not exist:" << watchPath;
        return 1;
    }

    const QSet<QString> imageSuffixes = {
        "png", "jpg", "jpeg", "gif", "bmp", "webp"
    };

    QFileSystemWatcher watcher;
    if (!watcher.addPath(watchPath)) {
        qWarning() << "[Error] Could not monitor path:" << watchPath;
        return 1;
    }

    QStringList initialFiles = watchDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    QSet<QString> currentFiles(initialFiles.begin(), initialFiles.end());

    std::cout << "[Info] Started monitoring directory:" << watchPath.toStdString() << std::endl;

    QObject::connect(&watcher, &QFileSystemWatcher::directoryChanged, [&](const QString &path) {
        QDir changedDir(path);
        QStringList latestFilesList = changedDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
        QSet<QString> latestFiles(latestFilesList.begin(), latestFilesList.end());

        QSet<QString> newFiles = latestFiles - currentFiles;
        
        if (newFiles.isEmpty()) {
            currentFiles = latestFiles;
            return;
        }

        QStringList newImagePaths;
        for (const QString &fileName : newFiles) {
            QFileInfo fileInfo(changedDir.filePath(fileName));
            if (imageSuffixes.contains(fileInfo.suffix().toLower())) {
                newImagePaths << fileInfo.absoluteFilePath();
            }
        }
        
        if (newImagePaths.isEmpty()) {
            currentFiles = latestFiles;
            return;
        }

        QStringList uriList;
        for (const QString &filePath : newImagePaths) {
            uriList << QUrl::fromLocalFile(filePath).toString();
        }
        QString uriString = uriList.join("\n");

        QClipboard *clipboard = QGuiApplication::clipboard();
        if (!clipboard) {
            qWarning() << "[Error] Could not get the system clipboard.";
            currentFiles = latestFiles;
            return;
        }

        QMimeData *mimeData = new QMimeData();
        mimeData->setData("text/uri-list", uriString.toUtf8());
        mimeData->setText(newImagePaths.join("\n"));

        clipboard->setMimeData(mimeData);

        if (newImagePaths.size() == 1) {
            std::cout << "[Info] New image: " << newImagePaths.first().toStdString() << std::endl;
        } else {
            std::cout << "[Info] New image(s):" << std::endl;
            for(const auto& p : newImagePaths) {
                std::cout << "       - " << p.toStdString() << std::endl;
            }
        }

        currentFiles = latestFiles;
    });

    return app.exec();
}