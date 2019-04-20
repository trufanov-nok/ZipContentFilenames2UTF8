#ifndef ZIPSCANNER_H
#define ZIPSCANNER_H

#include <zip.h>
#include <QObject>

class ZipScanner: public QObject
{
     Q_OBJECT

public:
    struct EncodingInvalidChars {
        int ASCII = 0;
        int CP866 = 0;
        int Win1251 = 0;
        int KOI8R = 0;
    };

    enum RepackMode
    {
        None,
        FromCP866,
        FromWin1251,
        Unknown
    };

    typedef QMap<zip_uint64_t, RepackMode> RepackData;

public:
    explicit ZipScanner(const QString &inputDir, const QString &fileMask, bool recursive, bool repack, QObject* parent = nullptr);
    bool startScan();
    QStringList result() const { return m_result; }
signals:
    void log(const QString& txt);
private:
    bool scan(const QString& path);
    bool fixZipFilenamesEncoding(zip_t * zip, const RepackData& repack_mode);
    bool checkZip(const QString& filename);
private:
    QString m_inputDir;
    QString m_fileMask;
    bool m_recursive;
    bool m_repack;

    QStringList m_result;
    QList<QTextCodec*> m_codecs;
};

#endif // ZIPSCANNER_H
