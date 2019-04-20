#include "zipscanner.h"
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QTextCodec>


ZipScanner::ZipScanner(const QString& inputDir, const QString& fileMask, bool recursive, bool repack, QObject *parent): QObject(parent),
    m_inputDir(inputDir), m_fileMask(fileMask), m_recursive(recursive), m_repack(repack)
{
}



inline bool isASCIIFilenameChar(int ch)
{
    return (ch >= 0x20 && ch < 0x7F);
}

ZipScanner::EncodingInvalidChars getFilenameEncInfo(const uchar* filename, int len)
{
    ZipScanner::EncodingInvalidChars res;

    const uchar* c = filename;
    for (int i = 0; i < len; i++, c++) {
        bool ok = isASCIIFilenameChar(*c);
        if (!ok) {
            res.ASCII++;
            if (*c < 0x80 || *c >= 0xF4 || (*c >= 0xB0 && *c < 0xE0)) {
                res.CP866++;
            }
            if (*c < 0xC0) {
                res.Win1251++;
                res.KOI8R++; // can't deffer them
            }

        }
    }

    return res;
}

bool
ZipScanner::fixZipFilenamesEncoding(zip_t * zip, const RepackData& repack_mode)
{    
    bool modified = false;
    for (RepackData::const_iterator it = repack_mode.cbegin(); it != repack_mode.cend(); ++it) {
        RepackMode mode = *it;
        zip_uint64_t id = it.key();

        if (mode != RepackMode::None) {
            QTextCodec* source_codec;
            switch (mode) {
            case RepackMode::FromCP866 : source_codec = QTextCodec::codecForName("CP866"); break;
            case RepackMode::FromWin1251 : source_codec = QTextCodec::codecForName("Windows-1251"); break;
            default:
                source_codec = nullptr;
            }

            if (source_codec) {
                const char * zip_filename = zip_get_name(zip, id, ZIP_FL_ENC_STRICT|ZIP_FL_ENC_RAW);
                if (!zip_filename) {
                    return false;
                }
                QString new_filename = source_codec->toUnicode(zip_filename);
                if (zip_file_rename(zip, id, new_filename.toUtf8().toStdString().c_str(), ZIP_FL_ENC_UTF_8) == 0) {
                    modified = true;
                    emit log(tr("renaming %3 %1 to <span style=\" color:#ff00ff;\"><b>%2</b></span>").arg(zip_filename).arg(new_filename).arg(id));
                } else {
                    emit log(tr("ERROR: zip_file_rename for file id %1").arg(id));
                }
            } else {
                return false;
            }

        }
    }
    return true;
}

bool
ZipScanner::checkZip(const QString& filename)
{
    bool res = true;
    int errorp;
    QFileInfo fi(filename);
    const qint64 old_filesize = fi.size();
    QString archieve(fi.fileName());

    emit log (tr("Opening <b>%1</b>").arg(archieve));

    int flags = ZIP_CHECKCONS|ZIP_FL_ENC_STRICT|ZIP_FL_ENC_RAW;
    if (!m_repack) {
        flags |= ZIP_RDONLY;
    }

    zip_t* zip = zip_open(filename.toStdString().c_str(), flags, &errorp);
    if (!zip) {
        emit log(tr("<span style=\" color:#ff0000;\">Error: while opening zip file %1!! Error code: %2</span>").arg(archieve).arg(errorp));
        return false;
    }


    zip_int64_t num = zip_get_num_entries(zip, ZIP_FL_ENC_RAW);

    bool need_repack = false;


    RepackData id_to_encoding;

    for (zip_uint64_t i = 0; i < (zip_uint64_t) num; ++i) {

        RepackMode repack_mode = RepackMode::None;
        const char * zip_filename = zip_get_name(zip, i, ZIP_FL_ENC_STRICT|ZIP_FL_ENC_RAW);

        if (!zip_filename) {
            emit log(tr("<span style=\" color:#ff0000;\">Error: while getting filename from zip file %1!! Index: %2</span>").arg(archieve).arg(i));
            res = false;
            break;
        }

        int len = strlen(zip_filename);
        QTextCodec::ConverterState state;
        QTextCodec *codec = QTextCodec::codecForName("UTF-8");
        const QString text = codec->toUnicode(zip_filename, len, &state);
        bool is_utf8 = state.invalidChars == 0;
        if (is_utf8) {
            is_utf8 = QString(zip_filename) == codec->fromUnicode(text);
        }

        if (!is_utf8) {
            EncodingInvalidChars data = getFilenameEncInfo((const uchar*)zip_filename, len);
            if (data.ASCII == 0) {
                emit log(tr("%1: %2 isn't in UTF-8, but ASCII so no repack required.").arg(archieve).arg(zip_filename));
                // not UTF but no conversion required
                continue;
            } else if (data.CP866 == 0) {
                emit log(tr("%1: %2 is in CP866 repack required.").arg(archieve).arg(zip_filename));
                repack_mode = RepackMode::FromCP866;
            } else if (data.Win1251 == 0) {
                emit log(tr("%1: %2 is in Win-1251 or KOI8-R repack required. Win-1251 will be used as can't distinguish real encoding unless KOI8-R is a system encoding").arg(archieve).arg(zip_filename));
                repack_mode = RepackMode::FromWin1251;
            } else {
                emit log(tr("<span style=\" color:#ff0000;\">Error in %1: can't detect encoding. Assume it's non Unicode.</span>").arg(archieve));
                repack_mode = RepackMode::Unknown;
            }

            if (!need_repack) {
                need_repack = true;
            }
        } else {
            emit log(tr("%1: %2 is in UTF-8.").arg(archieve).arg(zip_filename));
        }

        id_to_encoding[i] = repack_mode;
    }

    if (need_repack) {
        m_result.append(filename);

        if (m_repack) {

            // let's try to fix filename and update zip file.
            if (!fixZipFilenamesEncoding(zip, id_to_encoding)) {
                res = false;
            }
        }
    }


    emit log (tr("Closing %1.").arg(archieve));

    errorp = zip_close(zip);
    if (res) {
        if (errorp != 0) {
            emit log(tr("<span style=\" color:#ff0000;\">Error: while closing zip file %1!! Error code: %2</span>").arg(archieve).arg(errorp));
            return false;
        } else if (need_repack && m_repack) {
            QFileInfo fi2(filename);
            qint64 diff = fi2.size() - old_filesize;
            QString op(tr("size not changed"));
            if (diff > 0) {
                    op = tr("size increased by %1 bytes").arg(diff);
            } else if (diff < 0) {
                op = tr("size decreased by %1 bytes").arg(diff);
            }

            emit log (tr("<span style=\" color:#005500;\"><b>File successfully repacked</b> (%1).</span>").arg(op));



        } else {
            QString msg;
            if (need_repack) {
                if (!m_repack) {
                    msg = tr("<span style=\" color:#ff0000;\">Repack required</span>");
                } else {

                }
            } else {
                msg = tr("<span style=\" color:#0000ff;\">No repack required</span>");
            }

            emit log (tr("File closed. %1").arg(msg));
        }
    } else {
        m_result.last() += " [* error]";
        zip_discard(zip);
        emit log (tr("<span style=\" color:#ff0000;\">Errors were detected. All changes was discared. FIle is marked with [* error] in output.</span>"));
    }

    return res;

}


bool
ZipScanner::scan(const QString& path)
{
    bool res (true);
    QDir dir(path);
    emit log(tr("Scan folder <i>%1</i>").arg(dir.dirName()));

    QDir::Filters filters = QDir::Files|QDir::NoDotAndDotDot|QDir::Readable;
    if (m_recursive) {
        filters |=  QDir::AllDirs;
    }

    QFileInfoList files = dir.entryInfoList(m_fileMask.split(";", QString::SkipEmptyParts),
                                            filters,
                                            QDir::Name|QDir::DirsLast);

    for (QFileInfo fi: files) {
        if (fi.isDir()) {
            res = scan(fi.absoluteFilePath());
            if (!res) {
                return res;
            }
        } else {

            res = checkZip(fi.absoluteFilePath());
            emit log("\n");
        }
    }

    return res;
}

bool
ZipScanner::startScan()
{
    m_result.clear();
    return scan(m_inputDir);
}
